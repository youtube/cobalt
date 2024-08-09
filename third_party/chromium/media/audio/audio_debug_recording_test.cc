// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/chromium/media/audio/audio_debug_recording_test.h"

#include "third_party/chromium/media/audio/mock_audio_debug_recording_manager.h"
#include "third_party/chromium/media/audio/mock_audio_manager.h"
#include "third_party/chromium/media/audio/test_audio_thread.h"

namespace media_m96 {

AudioDebugRecordingTest::AudioDebugRecordingTest() = default;

AudioDebugRecordingTest::~AudioDebugRecordingTest() = default;

void AudioDebugRecordingTest::CreateAudioManager() {
  DCHECK(AudioManager::Get() == nullptr);
  mock_audio_manager_ =
      std::make_unique<MockAudioManager>(std::make_unique<TestAudioThread>());
  ASSERT_NE(nullptr, AudioManager::Get());
  ASSERT_EQ(static_cast<AudioManager*>(mock_audio_manager_.get()),
            AudioManager::Get());
}

void AudioDebugRecordingTest::ShutdownAudioManager() {
  DCHECK(mock_audio_manager_);
  ASSERT_TRUE(mock_audio_manager_->Shutdown());
}

void AudioDebugRecordingTest::InitializeAudioDebugRecordingManager() {
  DCHECK(mock_audio_manager_);
  mock_audio_manager_->InitializeDebugRecording();
  mock_debug_recording_manager_ = static_cast<MockAudioDebugRecordingManager*>(
      mock_audio_manager_->GetAudioDebugRecordingManager());
  ASSERT_NE(nullptr, mock_debug_recording_manager_);
}

}  // namespace media_m96
