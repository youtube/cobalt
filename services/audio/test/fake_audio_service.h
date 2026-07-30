// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_AUDIO_TEST_FAKE_AUDIO_SERVICE_H_
#define SERVICES_AUDIO_TEST_FAKE_AUDIO_SERVICE_H_

#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/audio/public/mojom/audio_service.mojom.h"
#include "services/audio/test/fake_output_stream.h"

namespace audio {

class FakeAudioService : public mojom::AudioService {
 public:
  FakeAudioService();
  FakeAudioService(const FakeAudioService&) = delete;
  FakeAudioService& operator=(const FakeAudioService&) = delete;

  ~FakeAudioService() override;

  FakeOutputStream& fake_output_stream() { return fake_output_stream_; }

  // mojom::AudioService implementation:
  void BindSystemInfo(
      mojo::PendingReceiver<mojom::SystemInfo> receiver) override {}
  void BindStreamFactory(mojo::PendingReceiver<media::mojom::AudioStreamFactory>
                             receiver) override;
  void BindDebugRecording(
      mojo::PendingReceiver<mojom::DebugRecording> receiver) override {}
  void BindDeviceNotifier(
      mojo::PendingReceiver<mojom::DeviceNotifier> receiver) override {}
  void BindLogFactoryManager(
      mojo::PendingReceiver<mojom::LogFactoryManager> receiver) override {}
  void BindTestingApi(
      mojo::PendingReceiver<mojom::TestingApi> receiver) override {}
  void BindMlModelManager(
      mojo::PendingReceiver<mojom::MlModelManager> receiver) override {}

 private:
  FakeOutputStream fake_output_stream_;
};

}  // namespace audio

#endif  // SERVICES_AUDIO_TEST_FAKE_AUDIO_SERVICE_H_
