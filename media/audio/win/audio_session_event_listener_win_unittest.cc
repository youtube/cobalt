// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/win/audio_session_event_listener_win.h"

#include <memory>

#include "base/callback_helpers.h"
#include "base/win/scoped_com_initializer.h"
#include "media/audio/audio_unittest_util.h"
#include "media/audio/win/core_audio_util_win.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

static bool DevicesAvailable() {
  return media::CoreAudioUtil::IsSupported() &&
         media::CoreAudioUtil::NumberOfActiveDevices(eRender) > 0;
}

}  // namespace

namespace media {

class AudioSessionEventListenerTest : public testing::Test {
 public:
  AudioSessionEventListenerTest() {
    if (!DevicesAvailable())
      return;

    audio_client_ =
        CoreAudioUtil::CreateClient(std::string(), eRender, eConsole);
    CHECK(audio_client_);

    // AudioClient must be initialized to succeed in getting the session.
    WAVEFORMATEXTENSIBLE format;
    EXPECT_TRUE(SUCCEEDED(
        CoreAudioUtil::GetSharedModeMixFormat(audio_client_.Get(), &format)));

    // Perform a shared-mode initialization without event-driven buffer
    // handling.
    uint32_t endpoint_buffer_size = 0;
    HRESULT hr = CoreAudioUtil::SharedModeInitialize(
        audio_client_.Get(), &format, nullptr, 0, &endpoint_buffer_size,
        nullptr);
    EXPECT_TRUE(SUCCEEDED(hr));

    listener_ = std::make_unique<AudioSessionEventListener>(
        audio_client_.Get(),
        base::BindOnce(&AudioSessionEventListenerTest::OnSessionDisconnected,
                       base::Unretained(this)));
  }

  ~AudioSessionEventListenerTest() override = default;

  void SimulateSessionDisconnected() {
    listener_->OnSessionDisconnected(DisconnectReasonDeviceRemoval);
  }

  MOCK_METHOD0(OnSessionDisconnected, void());

 protected:
  base::win::ScopedCOMInitializer com_init_;
  Microsoft::WRL::ComPtr<IAudioClient> audio_client_;
  std::unique_ptr<AudioSessionEventListener> listener_;
};

TEST_F(AudioSessionEventListenerTest, Works) {
  ABORT_AUDIO_TEST_IF_NOT(DevicesAvailable());

  EXPECT_CALL(*this, OnSessionDisconnected());
  SimulateSessionDisconnected();

  // Calling it again shouldn't crash, but also shouldn't make any more calls.
  EXPECT_CALL(*this, OnSessionDisconnected()).Times(0);
  SimulateSessionDisconnected();
}

}  // namespace media
