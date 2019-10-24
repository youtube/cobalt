// Copyright 2019 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/shared/pulse/pulse_dynamic_load_dispatcher.h"

#include <dlfcn.h>

#include "starboard/common/log.h"
#include "starboard/once.h"

namespace starboard {
namespace shared {
namespace pulse {

namespace {
const char kPulseLibraryName[] = "libpulse.so";
void* pulse_lib_handle = NULL;
}  // namespace

int (*pa_context_connect)(pa_context*,
                          const char*,
                          pa_context_flags_t,
                          const pa_spawn_api*) = NULL;
void (*pa_context_disconnect)(pa_context*) = NULL;
pa_cvolume* (*pa_cvolume_set)(pa_cvolume*, unsigned, pa_volume_t) = NULL;
uint32_t (*pa_stream_get_index)(const pa_stream*) = NULL;
pa_context_state_t (*pa_context_get_state)(pa_context*) = NULL;
pa_context* (*pa_context_new)(pa_mainloop_api*, const char*) = NULL;
pa_operation* (*pa_context_set_sink_input_volume)(pa_context*,
                                                  uint32_t,
                                                  const pa_cvolume*,
                                                  pa_context_success_cb_t,
                                                  void*) = NULL;
void (*pa_context_set_state_callback)(pa_context*,
                                      pa_context_notify_cb_t,
                                      void*) = NULL;
void (*pa_context_unref)(pa_context*) = NULL;

size_t (*pa_frame_size)(const pa_sample_spec*) = NULL;
void (*pa_mainloop_free)(pa_mainloop*);
pa_mainloop_api* (*pa_mainloop_get_api)(pa_mainloop*) = NULL;
int (*pa_mainloop_iterate)(pa_mainloop*, int, int*) = NULL;
pa_mainloop* (*pa_mainloop_new)(void) = NULL;

void (*pa_operation_unref)(pa_operation*) = NULL;

int (*pa_stream_connect_playback)(pa_stream*,
                                  const char*,
                                  const pa_buffer_attr*,
                                  pa_stream_flags_t,
                                  const pa_cvolume*,
                                  pa_stream*) = NULL;
pa_operation* (*pa_stream_cork)(pa_stream*,
                                int,
                                pa_stream_success_cb_t,
                                void*) = NULL;
int (*pa_stream_disconnect)(pa_stream*) = NULL;
pa_stream_state_t (*pa_stream_get_state)(pa_stream*) = NULL;
int (*pa_stream_get_time)(pa_stream*, pa_usec_t*) = NULL;
int (*pa_stream_is_corked)(pa_stream*) = NULL;
pa_stream* (*pa_stream_new)(pa_context*,
                            const char*,
                            const pa_sample_spec*,
                            const pa_channel_map*) = NULL;
void (*pa_stream_set_underflow_callback)(pa_stream*,
                                         pa_stream_notify_cb_t,
                                         void*) = NULL;
void (*pa_stream_set_write_callback)(pa_stream*,
                                     pa_stream_request_cb_t,
                                     void*) = NULL;
void (*pa_stream_unref)(pa_stream*) = NULL;
size_t (*pa_stream_writable_size)(pa_stream*) = NULL;
int (*pa_stream_write)(pa_stream*,
                       const void*,
                       size_t,
                       pa_free_cb_t,
                       int64_t,
                       pa_seek_mode_t) = NULL;

void ReportSymbolError() {
  SB_LOG(ERROR) << "Pulse audio error: " << dlerror();
  dlclose(pulse_lib_handle);
  pulse_lib_handle = NULL;
}

bool pulse_load_library() {
  SB_DCHECK(!pulse_lib_handle);

  pulse_lib_handle = dlopen(kPulseLibraryName, RTLD_LAZY);
  if (!pulse_lib_handle) {
    return false;
  }

#define INITSYMBOL(symbol)                                                  \
  symbol =                                                                  \
      reinterpret_cast<decltype(symbol)>(dlsym(pulse_lib_handle, #symbol)); \
  if (!symbol) {                                                            \
    ReportSymbolError();                                                    \
    return false;                                                           \
  }

  INITSYMBOL(pa_context_connect);
  INITSYMBOL(pa_context_disconnect);
  INITSYMBOL(pa_context_get_state);
  INITSYMBOL(pa_context_new);
  INITSYMBOL(pa_context_set_sink_input_volume)
  INITSYMBOL(pa_context_set_state_callback);
  INITSYMBOL(pa_context_unref);
  INITSYMBOL(pa_cvolume_set);
  INITSYMBOL(pa_frame_size);
  INITSYMBOL(pa_mainloop_free);
  INITSYMBOL(pa_mainloop_get_api);
  INITSYMBOL(pa_mainloop_iterate);
  INITSYMBOL(pa_mainloop_new);
  INITSYMBOL(pa_operation_unref);
  INITSYMBOL(pa_stream_connect_playback);
  INITSYMBOL(pa_stream_cork);
  INITSYMBOL(pa_stream_disconnect);
  INITSYMBOL(pa_stream_get_index);
  INITSYMBOL(pa_stream_get_state);
  INITSYMBOL(pa_stream_get_time);
  INITSYMBOL(pa_stream_is_corked);
  INITSYMBOL(pa_stream_new);
  INITSYMBOL(pa_stream_set_underflow_callback);
  INITSYMBOL(pa_stream_set_write_callback);
  INITSYMBOL(pa_stream_unref);
  INITSYMBOL(pa_stream_writable_size);
  INITSYMBOL(pa_stream_write);

  return true;
}

void pulse_unload_library() {
  SB_DCHECK(pulse_lib_handle);

  dlclose(pulse_lib_handle);
  pulse_lib_handle = NULL;
}

}  // namespace pulse
}  // namespace shared
}  // namespace starboard
