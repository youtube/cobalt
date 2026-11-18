// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/test/mock_stream_factory.h"

namespace audio {

MockReceiverAudioOutputStream::MockReceiverAudioOutputStream() = default;
MockReceiverAudioOutputStream::~MockReceiverAudioOutputStream() = default;

MockStreamFactory::MockStreamFactory() = default;
MockStreamFactory::~MockStreamFactory() = default;

void MockStreamFactory::CreateOutputStream(
    mojo::PendingReceiver<media::mojom::AudioOutputStream> stream,
    mojo::PendingAssociatedRemote<media::mojom::AudioOutputStreamObserver>
        observer,
    mojo::PendingRemote<media::mojom::AudioLog> log,
    const std::string& device_id,
    const media::AudioParameters& params,
    const base::UnguessableToken& group_id,
    CreateOutputStreamCallback callback) {
  stream_.Bind(std::move(stream));
  created_callback_ = std::move(callback);
}

void MockStreamFactory::CreateSwitchableOutputStream(
    mojo::PendingReceiver<media::mojom::AudioOutputStream> stream,
    mojo::PendingReceiver<media::mojom::DeviceSwitchInterface>
        device_switch_receiver,
    mojo::PendingAssociatedRemote<media::mojom::AudioOutputStreamObserver>
        observer,
    mojo::PendingRemote<media::mojom::AudioLog> log,
    const std::string& device_id,
    const media::AudioParameters& params,
    const base::UnguessableToken& group_id,
    CreateOutputStreamCallback callback) {
  stream_.Bind(std::move(stream));
  created_callback_ = std::move(callback);
}

}  // namespace audio
