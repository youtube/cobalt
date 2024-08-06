// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_CHROMIUM_MEDIA_AUDIO_MOCK_AUDIO_DEBUG_RECORDING_MANAGER_H_
#define THIRD_PARTY_CHROMIUM_MEDIA_AUDIO_MOCK_AUDIO_DEBUG_RECORDING_MANAGER_H_

#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "third_party/chromium/media/audio/audio_debug_recording_manager.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media_m96 {

class MockAudioDebugRecordingManager : public AudioDebugRecordingManager {
 public:
  explicit MockAudioDebugRecordingManager(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  MockAudioDebugRecordingManager(const MockAudioDebugRecordingManager&) =
      delete;
  MockAudioDebugRecordingManager& operator=(
      const MockAudioDebugRecordingManager&) = delete;

  ~MockAudioDebugRecordingManager() override;

  MOCK_METHOD1(EnableDebugRecording,
               void(AudioDebugRecordingManager::CreateWavFileCallback
                        create_file_callback));
  MOCK_METHOD0(DisableDebugRecording, void());
};

}  // namespace media_m96.

#endif  // THIRD_PARTY_CHROMIUM_MEDIA_AUDIO_MOCK_AUDIO_DEBUG_RECORDING_MANAGER_H_
