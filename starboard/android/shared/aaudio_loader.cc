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

#include "starboard/android/shared/aaudio_loader.h"

#include <dlfcn.h>

#include <memory>

#include "starboard/common/log.h"

namespace starboard {

namespace {

bool LoadSymbols() {
  void* lib_handle = dlopen("libaaudio.so", RTLD_NOW);
  if (!lib_handle) {
    SB_LOG(INFO) << "AAudio is not supported on this device (failed to load "
                    "libaaudio.so).";
    return false;
  }

#define RESOLVE_SYMBOL(c_name, static_name)                            \
  using PFN_##c_name = decltype(AAudio::static_name);                  \
  AAudio::static_name =                                                \
      reinterpret_cast<PFN_##c_name>(dlsym(lib_handle, #c_name));      \
  if (!AAudio::static_name) {                                          \
    SB_LOG(WARNING) << "Failed to resolve AAudio symbol: " << #c_name; \
    dlclose(lib_handle);                                               \
    return false;                                                      \
  }

  RESOLVE_SYMBOL(AAudio_convertResultToText, ConvertResultToText);
  RESOLVE_SYMBOL(AAudio_createStreamBuilder, CreateStreamBuilder);
  RESOLVE_SYMBOL(AAudioStreamBuilder_delete, StreamBuilder_Delete);
  RESOLVE_SYMBOL(AAudioStreamBuilder_setDirection, StreamBuilder_SetDirection);
  RESOLVE_SYMBOL(AAudioStreamBuilder_setSharingMode,
                 StreamBuilder_SetSharingMode);
  RESOLVE_SYMBOL(AAudioStreamBuilder_setPerformanceMode,
                 StreamBuilder_SetPerformanceMode);
  RESOLVE_SYMBOL(AAudioStreamBuilder_setFormat, StreamBuilder_SetFormat);
  RESOLVE_SYMBOL(AAudioStreamBuilder_setChannelCount,
                 StreamBuilder_SetChannelCount);
  RESOLVE_SYMBOL(AAudioStreamBuilder_setSampleRate,
                 StreamBuilder_SetSampleRate);
  RESOLVE_SYMBOL(AAudioStreamBuilder_setDataCallback,
                 StreamBuilder_SetDataCallback);
  RESOLVE_SYMBOL(AAudioStreamBuilder_setBufferCapacityInFrames,
                 StreamBuilder_SetBufferCapacityInFrames);
  RESOLVE_SYMBOL(AAudioStreamBuilder_openStream, StreamBuilder_OpenStream);
  RESOLVE_SYMBOL(AAudioStream_close, Stream_Close);
  RESOLVE_SYMBOL(AAudioStream_requestStart, Stream_RequestStart);
  RESOLVE_SYMBOL(AAudioStream_requestPause, Stream_RequestPause);
  RESOLVE_SYMBOL(AAudioStream_requestStop, Stream_RequestStop);
  RESOLVE_SYMBOL(AAudioStream_requestFlush, Stream_RequestFlush);
  RESOLVE_SYMBOL(AAudioStream_getState, Stream_GetState);
  RESOLVE_SYMBOL(AAudioStream_getTimestamp, Stream_GetTimestamp);
  RESOLVE_SYMBOL(AAudioStream_write, Stream_Write);
  RESOLVE_SYMBOL(AAudioStream_setBufferSizeInFrames,
                 Stream_SetBufferSizeInFrames);
  RESOLVE_SYMBOL(AAudioStream_waitForStateChange, Stream_WaitForStateChange);
  RESOLVE_SYMBOL(AAudioStream_getBufferSizeInFrames,
                 Stream_GetBufferSizeInFrames);
  RESOLVE_SYMBOL(AAudioStream_getFramesPerBurst, Stream_GetFramesPerBurst);
  RESOLVE_SYMBOL(AAudioStream_getFramesRead, Stream_GetFramesRead);
  RESOLVE_SYMBOL(AAudioStream_getXRunCount, Stream_GetXRunCount);
#undef RESOLVE_SYMBOL

  SB_LOG(INFO) << "Successfully initialized AAudio NDK API.";
  return true;
}

}  // namespace

// Define static function pointer members.
const char* (*AAudio::ConvertResultToText)(aaudio_result_t) = nullptr;
aaudio_result_t (*AAudio::CreateStreamBuilder)(AAudioStreamBuilder**) = nullptr;
aaudio_result_t (*AAudio::StreamBuilder_Delete)(AAudioStreamBuilder*) = nullptr;
void (*AAudio::StreamBuilder_SetDirection)(AAudioStreamBuilder*,
                                           aaudio_direction_t) = nullptr;
void (*AAudio::StreamBuilder_SetSharingMode)(AAudioStreamBuilder*,
                                             aaudio_sharing_mode_t) = nullptr;
void (*AAudio::StreamBuilder_SetPerformanceMode)(AAudioStreamBuilder*,
                                                 aaudio_performance_mode_t) =
    nullptr;
void (*AAudio::StreamBuilder_SetFormat)(AAudioStreamBuilder*,
                                        aaudio_format_t) = nullptr;
void (*AAudio::StreamBuilder_SetChannelCount)(AAudioStreamBuilder*,
                                              int32_t) = nullptr;
void (*AAudio::StreamBuilder_SetSampleRate)(AAudioStreamBuilder*,
                                            int32_t) = nullptr;
void (*AAudio::StreamBuilder_SetDataCallback)(AAudioStreamBuilder*,
                                              AAudioStream_dataCallback,
                                              void*) = nullptr;
void (*AAudio::StreamBuilder_SetBufferCapacityInFrames)(AAudioStreamBuilder*,
                                                        int32_t) = nullptr;
aaudio_result_t (*AAudio::StreamBuilder_OpenStream)(AAudioStreamBuilder*,
                                                    AAudioStream**) = nullptr;
aaudio_result_t (*AAudio::Stream_Close)(AAudioStream*) = nullptr;
aaudio_result_t (*AAudio::Stream_RequestStart)(AAudioStream*) = nullptr;
aaudio_result_t (*AAudio::Stream_RequestPause)(AAudioStream*) = nullptr;
aaudio_result_t (*AAudio::Stream_RequestStop)(AAudioStream*) = nullptr;
aaudio_result_t (*AAudio::Stream_RequestFlush)(AAudioStream*) = nullptr;
aaudio_stream_state_t (*AAudio::Stream_GetState)(AAudioStream*) = nullptr;
aaudio_result_t (*AAudio::Stream_GetTimestamp)(AAudioStream*,
                                               clockid_t,
                                               int64_t*,
                                               int64_t*) = nullptr;
aaudio_result_t (*AAudio::Stream_Write)(AAudioStream*,
                                        const void*,
                                        int32_t,
                                        int64_t) = nullptr;
aaudio_result_t (*AAudio::Stream_SetBufferSizeInFrames)(AAudioStream*,
                                                        int32_t) = nullptr;
aaudio_result_t (*AAudio::Stream_WaitForStateChange)(AAudioStream*,
                                                     aaudio_stream_state_t,
                                                     aaudio_stream_state_t*,
                                                     int64_t) = nullptr;
int32_t (*AAudio::Stream_GetBufferSizeInFrames)(AAudioStream*) = nullptr;
int32_t (*AAudio::Stream_GetFramesPerBurst)(AAudioStream*) = nullptr;
int64_t (*AAudio::Stream_GetFramesRead)(AAudioStream*) = nullptr;
int32_t (*AAudio::Stream_GetXRunCount)(AAudioStream*) = nullptr;

bool AAudio::Load() {
  static const bool is_supported = LoadSymbols();
  return is_supported;
}

}  // namespace starboard
