//  Copyright (c) 2014, Facebook, Inc.  All rights reserved.
//  This source code is licensed under the BSD-style license found in the
//  LICENSE file in the root directory of this source tree. An additional grant
//  of patent rights can be found in the PATENTS file in the same directory.
//
#define __STDC_FORMAT_MACROS
#include "src/pilot/pilot.h"
#include <map>
#include <string>
#include <thread>
#include <vector>
#include "src/util/storage.h"
#include "src/util/memory.h"

namespace rocketspeed {

void AppendClosure::operator()(Status append_status, SequenceNumber seqno) {
  // IMPORTANT: This may be called after Stop(). Must not use the log storage
  // or log router after this point.

  // Record latency
  uint64_t latency = pilot_->options_.env->NowMicros() - append_time_;
  Status st = pilot_->options_.msg_loop->SendCommand(
    std::unique_ptr<Command>(MakeExecuteCommand(
      [this, latency, append_status, seqno] () {
        pilot_->worker_data_[worker_id_].stats_.append_latency->Record(latency);
        pilot_->worker_data_[worker_id_].stats_.append_requests->Add(1);
        pilot_->AppendCallback(append_status,
                               seqno,
                               std::move(msg_),
                               logid_,
                               append_time_,
                               worker_id_,
                               origin_);
        pilot_->worker_data_[worker_id_].append_closure_pool_->Deallocate(this);
      })), worker_id_);

  if (!st.ok()) {
    LOG_WARN(pilot_->options_.info_log,
      "Failed to send command for append callback on Log(%" PRIu64 ")"
      " Topic(%s,%s): %s",
      logid_,
      msg_->GetNamespaceId().ToString().c_str(),
      msg_->GetTopicName().ToString().c_str(),
      st.ToString().c_str());
    pilot_->worker_data_[worker_id_].append_closure_pool_->Deallocate(this);
  }
}

/**
 * Sanitize user-specified options
 */
PilotOptions Pilot::SanitizeOptions(PilotOptions options) {
  if (options.info_log == nullptr) {
    Status s = CreateLoggerFromOptions(options.env,
                                       options.log_dir,
                                       "LOG.pilot",
                                       options.log_file_time_to_roll,
                                       options.max_log_file_size,
                                       options.info_log_level,
                                       &options.info_log);
    if (!s.ok()) {
      // No place suitable for logging
      options.info_log = std::make_shared<NullLogger>();
    }
  }

  return std::move(options);
}

/**
 * Private constructor for a Pilot
 */
Pilot::Pilot(PilotOptions options):
  options_(SanitizeOptions(std::move(options))) {

  for (int i = 0; i < options_.msg_loop->GetNumWorkers(); ++i) {
    worker_data_.emplace_back(WorkerData());
  }
  log_storage_ = options_.storage;
  options_.msg_loop->RegisterCallbacks(InitializeCallbacks());

  LOG_INFO(options_.info_log, "Created a new Pilot");
  options_.info_log->Flush();
}

Pilot::~Pilot() {
  options_.info_log->Flush();
}

void Pilot::Stop() {
  assert(!options_.msg_loop->IsRunning());  // must stop message loop first
  log_storage_.reset();
  options_.storage.reset();
  options_.log_router.reset();
}

/**
 * This is a static method to create a Pilot
 */
Status Pilot::CreateNewInstance(PilotOptions options,
                                Pilot** pilot) {
  if (!options.msg_loop) {
    assert(false);
    return Status::InvalidArgument("Message loop must be provided");
  }

  if (!options.storage) {
    assert(false);
    return Status::InvalidArgument("Log storage must be provided");
  }

  if (!options.log_router) {
    assert(false);
    return Status::InvalidArgument("Log router must be provided");
  }

  *pilot = new Pilot(std::move(options));

  // Ensure we managed to connect to the log storage.
  if ((*pilot)->log_storage_ == nullptr) {
    delete *pilot;
    *pilot = nullptr;
    return Status::NotInitialized();
  }

  return Status::OK();
}

// A callback method to process MessageData
void Pilot::ProcessPublish(std::unique_ptr<Message> msg, StreamID origin) {
  // Sanity checks.
  assert(msg);
  assert(msg->GetMessageType() == MessageType::mPublish);

  int worker_id = options_.msg_loop->GetThreadWorkerIndex();
  WorkerData& worker_data = worker_data_[worker_id];

  // Route topic to log ID.
  MessageData* msg_data = static_cast<MessageData*>(msg.release());
  LogID logid;
  if (!options_.log_router->GetLogID(msg_data->GetNamespaceId(),
                                     msg_data->GetTopicName(),
                                     &logid).ok()) {
    assert(false);  // GetLogID should never fail.
    return;
  }

  LOG_INFO(options_.info_log,
      "Received data (%.16s) for Topic(%s,%s) in Log(%" PRIu64 ")",
      msg_data->GetPayload().ToString().c_str(),
      msg_data->GetNamespaceId().ToString().c_str(),
      msg_data->GetTopicName().ToString().c_str(),
      logid);

  // Setup AppendCallback
  uint64_t now = options_.env->NowMicros();
  AppendClosure* closure;
  std::unique_ptr<MessageData> msg_owned(msg_data);
  closure = worker_data.append_closure_pool_->Allocate(
    this,
    std::move(msg_owned),
    logid,
    now,
    worker_id,
    origin);

  // Asynchronously append to log storage.
  auto append_callback = std::ref(*closure);
  auto status = log_storage_->AppendAsync(logid,
                                          msg_data->GetStorageSlice(),
                                          std::move(append_callback));

  // Fault injection: insert corrupt data into the logs.
  if (options_.FAULT_corrupt_extra_probability != 0.0) {
    std::uniform_real_distribution<double> dist(0.0, 1.0);
    if (dist(worker_data.prng_) < options_.FAULT_corrupt_extra_probability) {
      LOG_INFO(options_.info_log,
        "Fault-injection: appending corrupt record into Log(%" PRIu64 ")",
        logid);
      worker_data.stats_.FAULT_corrupt_writes->Add(1);
      log_storage_->AppendAsync(logid, "invalid",
        [] (Status, SequenceNumber) {});
    }
  }

  if (!status.ok()) {
    // Append call failed, log and send failure ack.
    worker_data.stats_.failed_appends->Add(1);
    LOG_ERROR(options_.info_log,
      "Failed to append to Topic(%s,%s) in Log(%" PRIu64 ") (%s)",
      msg_data->GetNamespaceId().ToString().c_str(),
      msg_data->GetTopicName().ToString().c_str(),
      logid,
      status.ToString().c_str());
    options_.info_log->Flush();

    SendAck(msg_data, 0, MessageDataAck::AckStatus::Failure, worker_id, origin);

    // If AppendAsync, the closure will never be invoked, so delete now.
    worker_data.append_closure_pool_->Deallocate(closure);
  }
}

void Pilot::AppendCallback(Status append_status,
                           SequenceNumber seqno,
                           std::unique_ptr<MessageData> msg,
                           LogID logid,
                           uint64_t append_time,
                           int worker_id,
                           StreamID origin) {
  if (append_status.ok()) {
    // Append successful, send success ack.
    SendAck(msg.get(),
            seqno,
            MessageDataAck::AckStatus::Success,
            worker_id,
            origin);
    LOG_INFO(options_.info_log,
        "Appended (%.16s) successfully to Topic(%s,%s) in Log(%" PRIu64
        ")@%" PRIu64,
        msg->GetPayload().ToString().c_str(),
        msg->GetNamespaceId().ToString().c_str(),
        msg->GetTopicName().ToString().c_str(),
        logid,
        seqno);
  } else {
    // Append failed, send failure ack.
    worker_data_[worker_id].stats_.failed_appends->Add(1);
    LOG_ERROR(options_.info_log,
        "AppendAsync failed for Topic(%s,%s) in Log(%" PRIu64 ") (%s)",
        msg->GetNamespaceId().ToString().c_str(),
        msg->GetTopicName().ToString().c_str(),
        logid,
        append_status.ToString().c_str());
    options_.info_log->Flush();
    SendAck(msg.get(),
            0,
            MessageDataAck::AckStatus::Failure,
            worker_id,
            origin);
  }
}

void Pilot::SendAck(MessageData* msg,
                    SequenceNumber seqno,
                    MessageDataAck::AckStatus status,
                    int worker_id,
                    StreamID origin) {
  MessageDataAck::Ack ack;
  ack.status = status;
  ack.msgid = msg->GetMessageId();
  ack.seqno = seqno;

  // create new message
  MessageDataAck newmsg(msg->GetTenantID(), {ack});

  // send message
  Status st = options_.msg_loop->SendResponse(newmsg, origin, worker_id);
  if (!st.ok()) {
    // This is entirely possible, other end may have disconnected by the time
    // we get round to sending an ack. This shouldn't be a rare occurrence.
    LOG_WARN(options_.info_log,
      "SendAck failed. Stream(%llu) Topic(%s,%s): %s",
      origin,
      msg->GetNamespaceId().ToString().c_str(),
      msg->GetTopicName().ToString().c_str(),
      st.ToString().c_str());
  }
}

// A static method to initialize the callback map
std::map<MessageType, MsgCallbackType> Pilot::InitializeCallbacks() {
  // create a temporary map and initialize it
  std::map<MessageType, MsgCallbackType> cb;
  cb[MessageType::mPublish] = [this] (std::unique_ptr<Message> msg,
                                      StreamID origin) {
    ProcessPublish(std::move(msg), origin);
  };

  // return the updated map
  return cb;
}

Statistics Pilot::GetStatisticsSync() const {
  return options_.msg_loop->AggregateStatsSync(
    [this] (int i) { return worker_data_[i].stats_.all; });
}

std::string Pilot::GetInfoSync(std::vector<std::string> args) {
  return "Unknown info for pilot";
}

}  // namespace rocketspeed
