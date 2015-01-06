// Copyright (c) 2014, Facebook, Inc.  All rights reserved.
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree. An additional grant
// of patent rights can be found in the PATENTS file in the same directory.
//
#pragma once

#include <memory>
#include <string>

#include "include/Types.h"
#include "src/messages/messages.h"
#include "src/util/common/autovector.h"

namespace rocketspeed {

enum CommandType : uint8_t {
  kNotInitialized = 0,
  kSendCommand = 0x01,
  kAcceptCommand = 0x02,
  kStorageUpdateCommand = 0x03,
  kStorageLoadCommand = 0x04,
  kStorageSnapshotCommand = 0x05,
};

/**
 * Interface class for sending messages from any thread to the event loop for
 * processing on the event loop thread.
 */
class Command {
 public:
  explicit Command(uint64_t issued_time) : issued_time_(issued_time) {}

  virtual ~Command() {}

  // Get type of the command.
  virtual CommandType GetCommandType() const = 0;

  // Get the time when this command was issued by the sender.
  uint64_t GetIssuedTime() const { return issued_time_; }

 private:
  uint64_t issued_time_;
};

/**
 * Command for sending a message to remote recipients.
 * The SendCommand is special because the event loop processes it inline
 * instead of invoking the application callback. The message associated with
 * this command is sent to the host specified via a call to GetDestination().
 */
class SendCommand : public Command {
 public:
  // Allocate one ClientID in-place for the common case.
  typedef autovector<ClientID, 1> Recipients;

  explicit SendCommand(uint64_t issued_time) : Command(issued_time) {}

  virtual ~SendCommand() {}

  CommandType GetCommandType() const { return kSendCommand; }

  virtual void GetMessage(std::string* out) = 0;

  // If this is a command to send a mesage to remote hosts, then returns the
  // list of destination HostIds.
  virtual const Recipients& GetDestination() const = 0;
};

/**
 * SendCommand where message is serialized before sending.
 */
class SerializedSendCommand : public SendCommand {
 public:
  SerializedSendCommand() = default;

  SerializedSendCommand(std::string message,
                        const ClientID& host,
                        uint64_t issued_time):
    SendCommand(issued_time),
    message_(std::move(message)) {
    recipient_.push_back(host);
    assert(message_.size() > 0);
  }

  SerializedSendCommand(std::string message,
                        Recipients hosts,
                        uint64_t issued_time):
    SendCommand(issued_time),
    recipient_(std::move(hosts)),
    message_(std::move(message)) {
    assert(message_.size() > 0);
  }

  void GetMessage(std::string* out) {
    out->assign(std::move(message_));
  }

  const Recipients& GetDestination() const {
    return recipient_;
  }

 private:
  Recipients recipient_;
  std::string message_;
};

}  // namespace rocketspeed
