// Copyright 2017 Google Inc. All Rights Reserved.
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

#ifndef STARBOARD_SHARED_STARBOARD_MICROPHONE_MICROPHONE_INTERNAL_H_
#define STARBOARD_SHARED_STARBOARD_MICROPHONE_MICROPHONE_INTERNAL_H_

#include "starboard/microphone.h"
#include "starboard/shared/internal_only.h"

struct SbMicrophonePrivate {
  virtual ~SbMicrophonePrivate() {}
  virtual bool Open() = 0;
  virtual bool Close() = 0;
  virtual int Read(void* out_audio_data, int audio_data_size) = 0;

  static int GetAvailableMicrophones(SbMicrophoneInfo* out_info_array,
                                     int info_array_size);
  static bool IsMicrophoneSampleRateSupported(SbMicrophoneId id,
                                              int sample_rate_in_hz);
  static SbMicrophone CreateMicrophone(SbMicrophoneId id,
                                       int sample_rate_in_hz,
                                       int buffer_size_bytes);
  static void DestroyMicrophone(SbMicrophone microphone);
};

#endif  // STARBOARD_SHARED_STARBOARD_MICROPHONE_MICROPHONE_INTERNAL_H_
