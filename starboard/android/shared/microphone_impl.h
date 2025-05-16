// Copyright 2025 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>
#include <queue>

#include "starboard/android/shared/starboard_bridge.h"
#include "starboard/common/mutex.h"
#include "starboard/shared/starboard/microphone/microphone_internal.h"

namespace starboard {
namespace android {
namespace shared {

class SbMicrophoneImpl : public SbMicrophonePrivate {
 public:
  SbMicrophoneImpl();
  ~SbMicrophoneImpl() override;

  bool Open() override;
  bool Close() override;
  int Read(void* out_audio_data, int audio_data_size) override;

  void SetPermission(bool is_granted);
  static bool RequestAudioPermission();
  static bool IsMicrophoneDisconnected();
  static bool IsMicrophoneMute();

 private:
  enum State { kWaitPermission, kPermissionGranted, kOpened, kClosed };

  static void SwapAndPublishBuffer(SLAndroidSimpleBufferQueueItf buffer_object,
                                   void* context);
  void SwapAndPublishBuffer();

  bool CreateAudioRecorder();
  void DeleteAudioRecorder();

  void ClearBuffer();

  bool StartRecording();
  bool StopRecording();

  SLObjectItf engine_object_;
  SLEngineItf engine_;
  SLObjectItf recorder_object_;
  SLRecordItf recorder_;
  SLAndroidSimpleBufferQueueItf buffer_object_;
  SLAndroidConfigurationItf config_object_;

  // Keeps track of the microphone's current state.
  State state_;
  // Audio data that has been delivered to the buffer queue.
  Mutex delivered_queue_mutex_;
  std::queue<int16_t*> delivered_queue_;
  // Audio data that is ready to be read.
  Mutex ready_queue_mutex_;
  std::queue<int16_t*> ready_queue_;
};

}  // namespace shared
}  // namespace android
}  // namespace starboard
