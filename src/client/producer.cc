// Copyright (c) 2014, Facebook, Inc.  All rights reserved.
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree. An additional grant
// of patent rights can be found in the PATENTS file in the same directory.
//
#include <chrono>
#include <memory>
#include <set>
#include <thread>
#include "include/RocketSpeed.h"
#include "include/Env.h"
#include "include/Slice.h"
#include "include/Status.h"
#include "include/Types.h"
#include "src/messages/messages.h"
#include "src/messages/msg_loop.h"
#include "src/port/port.h"

namespace rocketspeed {

Producer::~Producer() {
}

// Internal implementation of the Producer API.
class ProducerImpl : public Producer {
 public:
  virtual ~ProducerImpl();

  virtual PublishStatus Publish(const Topic& name,
                                const TopicOptions& options,
                                const Slice& data);

  ProducerImpl(const HostId& pilot_host_id,
               TenantID tenant_id,
               int port,
               PublishCallback callback);

 private:
  // Callback for MessageDataAck message.
  static void ProcessDataAck(const ApplicationCallbackContext arg,
                             std::unique_ptr<Message> msg);

  // HostId of this machine, i.e. the one the producer is running on.
  HostId host_id_;

  // HostId of pilot machine to send messages to.
  HostId pilot_host_id_;

  // Tenant ID of this producer.
  TenantID tenant_id_;

  // Incoming message loop object.
  MsgLoop* msg_loop_ = nullptr;
  std::thread msg_loop_thread_;

  // Messages sent, awaiting ack.
  std::set<MsgId> messages_sent_;

  // Ack callback.
  PublishCallback callback_;
};

// Implementation of Producer::Open from RocketSpeed.h
Status Producer::Open(const Configuration* config,
                      PublishCallback callback,
                      Producer** producer) {
  // Validate arguments.
  if (config == nullptr) {
    return Status::InvalidArgument("config must not be null.");
  }
  if (producer == nullptr) {
    return Status::InvalidArgument("producer must not be null.");
  }
  if (config->GetPilotHostIds().empty()) {
    return Status::InvalidArgument("Must have at least one pilot.");
  }

  // Construct new Producer client.
  // TODO(pja) 1 : Just using first pilot for now, should use some sort of map.
  *producer = new ProducerImpl(config->GetPilotHostIds().front(),
                               config->GetTenantID(),
                               config->GetLocalPort(),
                               callback);
  return Status::OK();
}

ProducerImpl::ProducerImpl(const HostId& pilot_host_id,
                           TenantID tenant_id,
                           int port,
                           PublishCallback callback)
: pilot_host_id_(pilot_host_id)
, tenant_id_(tenant_id)
, callback_(callback) {
  // Initialise host_id_.
  char myname[1024];
  if (gethostname(&myname[0], sizeof(myname))) {
    assert(false);
  }
  host_id_ = HostId(myname, port);

  // Setup callbacks.
  std::map<MessageType, MsgCallbackType> callbacks;
  callbacks[MessageType::mDataAck] = MsgCallbackType(ProcessDataAck);

  auto command_callback = [this] (std::unique_ptr<Command> command) {
    assert(command);
    auto message = static_cast<MessageCommand*>(command.get());
    bool added = messages_sent_.insert(message->GetMessageId()).second;
    if (added) {
      msg_loop_->GetClient().Send(message->GetRecipient(),
                                  message->GetMessage());
    } else {
      // This means we have already sent a message with this ID, which
      // means two separate messages have been given the same ID.
    }
  };

  // Construct message loop.
  msg_loop_ = new MsgLoop(Env::Default(),
                          EnvOptions(),
                          host_id_,
                          std::make_shared<NullLogger>(),  // no logging
                          static_cast<ApplicationCallbackContext>(this),
                          callbacks,
                          command_callback);

  msg_loop_thread_ = std::thread([this] () {
    msg_loop_->Run();
  });

  while (!msg_loop_->IsRunning()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
}

ProducerImpl::~ProducerImpl() {
  // Delete the message loop.
  // This stops the event loop, which may block.
  delete msg_loop_;

  // Wait for thread to join.
  msg_loop_thread_.join();
}

PublishStatus ProducerImpl::Publish(const Topic& name,
                                    const TopicOptions& options,
                                    const Slice& data) {
  // Construct message.
  MessageData message(tenant_id_,
                      host_id_,
                      Slice(name),
                      data,
                      options.retention);

  // Take note of message ID before we move into the command.
  const MsgId msgid = message.GetMessageId();

  // Construct command.
  std::unique_ptr<Command> command(new MessageCommand(msgid,
                                                      pilot_host_id_,
                                                      message));

  // Send to event loop for processing (the loop will free it).
  Status status = msg_loop_->SendCommand(std::move(command));

  // Return status with the generated message ID.
  return PublishStatus(status, msgid);
}

void ProducerImpl::ProcessDataAck(const ApplicationCallbackContext arg,
                                  std::unique_ptr<Message> msg) {
  ProducerImpl* self = static_cast<ProducerImpl*>(arg);
  const MessageDataAck* ackMsg = static_cast<const MessageDataAck*>(msg.get());

  // For each ack'd message, if it was waiting for an ack then remove it
  // from the waiting list and let the application know about the ack.
  for (const auto& ack : ackMsg->GetAcks()) {
    if (self->messages_sent_.erase(ack.msgid)) {
      ResultStatus rs;
      rs.msgid = ack.msgid;
      if (ack.status == MessageDataAck::AckStatus::Success) {
        rs.status = Status::OK();
        // TODO(pja) 1: get seqno
      } else {
        rs.status = Status::IOError("publish failed");
      }

      if (self->callback_) {
        self->callback_(rs);
      }
    } else {
      // We've received an ack for a message that has already been acked
      // (or was never sent). This is possible if a message was sent twice
      // before the first ack arrived, so just ignore.
    }
  }
}

}  // namespace rocketspeed
