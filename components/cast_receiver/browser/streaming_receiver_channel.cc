// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_receiver/browser/streaming_receiver_channel.h"

#include <string>

#include "base/check.h"
#include "base/logging.h"
#include "components/cast_receiver/proto/input_event.pb.h"

namespace cast_receiver {

StreamingReceiverChannel::StreamingReceiverChannel(
    std::unique_ptr<cast_api_bindings::MessagePort> message_port)
    : message_port_(std::move(message_port)) {
  CHECK(message_port_);
  message_port_->SetReceiver(this);
}

StreamingReceiverChannel::~StreamingReceiverChannel() {
  if (message_port_) {
    message_port_->Close();
  }
}

void StreamingReceiverChannel::SendInputEvent(const InputEvent& event) {
  if (!message_port_ || !message_port_->CanPostMessage()) {
    LOG(WARNING) << "Cannot send message, port is closed or invalid.";
    return;
  }

  std::string serialized;
  if (!event.SerializeToString(&serialized)) {
    LOG(ERROR) << "Failed to serialize InputEvent.";
    return;
  }

  message_port_->PostMessage(serialized);
}

bool StreamingReceiverChannel::OnMessage(
    std::string_view message,
    std::vector<std::unique_ptr<cast_api_bindings::MessagePort>> ports) {
  // TODO(b/501522411): Handle messages received from the sender.
  return true;
}

void StreamingReceiverChannel::OnPipeError() {
  LOG(WARNING) << "Streaming Receiver Channel pipe error.";
}

}  // namespace cast_receiver
