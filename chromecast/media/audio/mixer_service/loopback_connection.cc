// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/audio/mixer_service/loopback_connection.h"

#include <algorithm>
#include <limits>
#include <utility>

#include "base/check.h"
#include "chromecast/media/audio/mixer_service/mixer_service_transport.pb.h"
#include "chromecast/media/audio/net/common.pb.h"
#include "chromecast/media/audio/net/conversions.h"
#include "chromecast/net/io_buffer_pool.h"
#include "net/socket/stream_socket.h"

namespace chromecast {
namespace media {
namespace mixer_service {

namespace {

enum MessageTypes : int {
  kRequest = 1,
};

}

LoopbackConnection::LoopbackConnection(Delegate* delegate)
    : LoopbackConnection(delegate, nullptr) {}

LoopbackConnection::LoopbackConnection(
    Delegate* delegate,
    std::unique_ptr<MixerSocket> connected_socket_for_test)
    : delegate_(delegate), socket_(std::move(connected_socket_for_test)) {
  DCHECK(delegate_);
}

LoopbackConnection::~LoopbackConnection() = default;

void LoopbackConnection::Connect() {
  if (socket_) {
    OnConnected(std::move(socket_));
    return;
  }
  MixerConnection::Connect();
}

void LoopbackConnection::OnConnected(std::unique_ptr<MixerSocket> socket) {
  format_ = kUnknownSampleFormat;
  sample_rate_ = 0;
  num_channels_ = 0;

  socket_ = std::move(socket);
  socket_->SetDelegate(this);

  Generic message;
  message.mutable_loopback_request();
  socket_->SendProto(kRequest, message);
}

void LoopbackConnection::OnConnectionError() {
  delegate_->OnLoopbackInterrupted(LoopbackInterruptReason::kDisconnected);
  socket_.reset();
  MixerConnection::Connect();
}

bool LoopbackConnection::HandleMetadata(const Generic& message) {
  if (message.has_stream_config()) {
    format_ = audio_service::ConvertSampleFormat(
        message.stream_config().sample_format());
    sample_rate_ = message.stream_config().sample_rate();
    num_channels_ = message.stream_config().num_channels();
  }
  if (message.has_stream_interruption()) {
    delegate_->OnLoopbackInterrupted(static_cast<LoopbackInterruptReason>(
        message.stream_interruption().reason()));
  }
  return true;
}

bool LoopbackConnection::HandleAudioData(char* data,
                                         size_t size,
                                         int64_t timestamp) {
  if (format_ != kUnknownSampleFormat && size > 0) {
    delegate_->OnLoopbackAudio(timestamp, format_, sample_rate_, num_channels_,
                               reinterpret_cast<uint8_t*>(data), size);
  }
  return true;
}

}  // namespace mixer_service
}  // namespace media
}  // namespace chromecast
