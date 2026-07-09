// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_ANDROID_SHARED_AAUDIO_LOADER_H_
#define STARBOARD_ANDROID_SHARED_AAUDIO_LOADER_H_

#include <aaudio/AAudio.h>

namespace starboard {

// AAudio is a helper class with public static function pointers responsible for
// dynamically loading libaaudio.so and calling AAudio symbols at runtime.
//
// Threading Model:
// This class is thread-safe. Once Load() returns true, all static
// function pointers are populated and can be called from any thread.
class AAudio {
 public:
  // Dynamically loads libaaudio.so if available and resolves all symbols.
  // Returns true if AAudio is supported on this device.
  static bool Load();

  // Public static function pointers for AAudio NDK API.
  static const char* (*ConvertResultToText)(aaudio_result_t returnCode);
  static aaudio_result_t (*CreateStreamBuilder)(AAudioStreamBuilder** builder);
  static aaudio_result_t (*StreamBuilder_Delete)(AAudioStreamBuilder* builder);
  static void (*StreamBuilder_SetDirection)(AAudioStreamBuilder* builder,
                                            aaudio_direction_t direction);
  static void (*StreamBuilder_SetSharingMode)(
      AAudioStreamBuilder* builder,
      aaudio_sharing_mode_t sharingMode);
  static void (*StreamBuilder_SetPerformanceMode)(
      AAudioStreamBuilder* builder,
      aaudio_performance_mode_t performanceMode);
  static void (*StreamBuilder_SetFormat)(AAudioStreamBuilder* builder,
                                         aaudio_format_t format);
  static void (*StreamBuilder_SetChannelCount)(AAudioStreamBuilder* builder,
                                               int32_t channelCount);
  static void (*StreamBuilder_SetSampleRate)(AAudioStreamBuilder* builder,
                                             int32_t sampleRate);
  static void (*StreamBuilder_SetDataCallback)(
      AAudioStreamBuilder* builder,
      AAudioStream_dataCallback callback,
      void* userData);
  static void (*StreamBuilder_SetBufferCapacityInFrames)(
      AAudioStreamBuilder* builder,
      int32_t numFrames);
  static aaudio_result_t (*StreamBuilder_OpenStream)(
      AAudioStreamBuilder* builder,
      AAudioStream** stream);
  static aaudio_result_t (*Stream_Close)(AAudioStream* stream);
  static aaudio_result_t (*Stream_RequestStart)(AAudioStream* stream);
  static aaudio_result_t (*Stream_RequestPause)(AAudioStream* stream);
  static aaudio_result_t (*Stream_RequestStop)(AAudioStream* stream);
  static aaudio_result_t (*Stream_RequestFlush)(AAudioStream* stream);
  static aaudio_stream_state_t (*Stream_GetState)(AAudioStream* stream);
  static aaudio_result_t (*Stream_GetTimestamp)(AAudioStream* stream,
                                                clockid_t clockId,
                                                int64_t* framePosition,
                                                int64_t* timeNanoseconds);
  static aaudio_result_t (*Stream_Write)(AAudioStream* stream,
                                         const void* buffer,
                                         int32_t numFrames,
                                         int64_t timeoutNanoseconds);
  static aaudio_result_t (*Stream_SetBufferSizeInFrames)(AAudioStream* stream,
                                                         int32_t numFrames);
  static aaudio_result_t (*Stream_WaitForStateChange)(
      AAudioStream* stream,
      aaudio_stream_state_t inputState,
      aaudio_stream_state_t* nextState,
      int64_t timeoutNanoseconds);
  static int32_t (*Stream_GetBufferSizeInFrames)(AAudioStream* stream);
  static int32_t (*Stream_GetFramesPerBurst)(AAudioStream* stream);
  static int64_t (*Stream_GetFramesRead)(AAudioStream* stream);
  static int32_t (*Stream_GetXRunCount)(AAudioStream* stream);
};

}  // namespace starboard

#endif  // STARBOARD_ANDROID_SHARED_AAUDIO_LOADER_H_
