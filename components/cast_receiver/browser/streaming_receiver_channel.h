// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAST_RECEIVER_BROWSER_STREAMING_RECEIVER_CHANNEL_H_
#define COMPONENTS_CAST_RECEIVER_BROWSER_STREAMING_RECEIVER_CHANNEL_H_

#include <memory>
#include <string_view>
#include <vector>

#include "components/cast/message_port/message_port.h"

namespace cast_receiver {

class InputEvent;

// Represents a channel for transmitting various messages over the connected
// channel.
//
// This class wraps a MessagePort and handles the serialization of input events.
class StreamingReceiverChannel
    : public cast_api_bindings::MessagePort::Receiver {
 public:
  explicit StreamingReceiverChannel(
      std::unique_ptr<cast_api_bindings::MessagePort> message_port);
  ~StreamingReceiverChannel() override;

  StreamingReceiverChannel(const StreamingReceiverChannel&) = delete;
  StreamingReceiverChannel& operator=(const StreamingReceiverChannel&) = delete;

  // Sends an InputEvent to the sender.
  void SendInputEvent(const InputEvent& event);

 private:
  // cast_api_bindings::MessagePort::Receiver implementation:
  bool OnMessage(std::string_view message,
                 std::vector<std::unique_ptr<cast_api_bindings::MessagePort>>
                     ports) override;
  void OnPipeError() override;

  std::unique_ptr<cast_api_bindings::MessagePort> message_port_;
};

}  // namespace cast_receiver

#endif  // COMPONENTS_CAST_RECEIVER_BROWSER_STREAMING_RECEIVER_CHANNEL_H_
