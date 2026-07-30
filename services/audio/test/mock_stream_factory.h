// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_AUDIO_TEST_MOCK_STREAM_FACTORY_H_
#define SERVICES_AUDIO_TEST_MOCK_STREAM_FACTORY_H_

#include "media/mojo/mojom/audio_output_stream.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/audio/public/cpp/fake_stream_factory.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace audio {

class MockReceiverAudioOutputStream : public media::mojom::AudioOutputStream {
 public:
  MockReceiverAudioOutputStream();
  ~MockReceiverAudioOutputStream() override;

  MOCK_METHOD0(Play, void());
  MOCK_METHOD0(Pause, void());
  MOCK_METHOD1(SetVolume, void(double));
  MOCK_METHOD0(Flush, void());

  void Bind(mojo::PendingReceiver<media::mojom::AudioOutputStream> receiver) {
    receiver_.reset();
    receiver_.Bind(std::move(receiver));
  }

  void Close() { receiver_.reset(); }

 private:
  mojo::Receiver<media::mojom::AudioOutputStream> receiver_{this};
};

class MockStreamFactory : public FakeStreamFactory {
 public:
  MockStreamFactory();
  ~MockStreamFactory() override;

  // media::mojom::AudioStreamFactory overrides:
  void CreateOutputStream(
      mojo::PendingReceiver<media::mojom::AudioOutputStream> stream,
      mojo::PendingAssociatedRemote<media::mojom::AudioOutputStreamObserver>
          observer,
      mojo::PendingRemote<media::mojom::AudioLog> log,
      const std::string& device_id,
      const media::AudioParameters& params,
      const base::UnguessableToken& group_id,
      CreateOutputStreamCallback callback) override;

  void CreateSwitchableOutputStream(
      mojo::PendingReceiver<media::mojom::AudioOutputStream> stream,
      mojo::PendingReceiver<media::mojom::DeviceSwitchInterface>
          device_switch_receiver,
      mojo::PendingAssociatedRemote<media::mojom::AudioOutputStreamObserver>
          observer,
      mojo::PendingRemote<media::mojom::AudioLog> log,
      const std::string& device_id,
      const media::AudioParameters& params,
      const base::UnguessableToken& group_id,
      CreateOutputStreamCallback callback) override;

  MockReceiverAudioOutputStream& stream() { return stream_; }

  CreateOutputStreamCallback& created_callback() { return created_callback_; }

 private:
  MockReceiverAudioOutputStream stream_;
  CreateOutputStreamCallback created_callback_;
};

}  // namespace audio

#endif  // SERVICES_AUDIO_TEST_MOCK_STREAM_FACTORY_H_
