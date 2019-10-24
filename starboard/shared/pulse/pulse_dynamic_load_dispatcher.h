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

#ifndef STARBOARD_SHARED_PULSE_PULSE_DYNAMIC_LOAD_DISPATCHER_H_
#define STARBOARD_SHARED_PULSE_PULSE_DYNAMIC_LOAD_DISPATCHER_H_

#include <pulse/pulseaudio.h>

#include "starboard/shared/internal_only.h"

namespace starboard {
namespace shared {
namespace pulse {

bool pulse_load_library();
void pulse_unload_library();

extern int (*pa_context_connect)(pa_context*,
                                 const char*,
                                 pa_context_flags_t,
                                 const pa_spawn_api*);
extern void (*pa_context_disconnect)(pa_context*);
extern pa_cvolume* (*pa_cvolume_set)(pa_cvolume*, unsigned, pa_volume_t);
extern uint32_t (*pa_stream_get_index)(const pa_stream*);
extern pa_context_state_t (*pa_context_get_state)(pa_context*);
extern pa_context* (*pa_context_new)(pa_mainloop_api*, const char*);
extern pa_operation* (*pa_context_set_sink_input_volume)(
    pa_context*,
    uint32_t,
    const pa_cvolume*,
    pa_context_success_cb_t,
    void*);
extern void (*pa_context_set_state_callback)(pa_context*,
                                             pa_context_notify_cb_t,
                                             void*);
extern void (*pa_context_unref)(pa_context*);

extern size_t (*pa_frame_size)(const pa_sample_spec*);
extern void (*pa_mainloop_free)(pa_mainloop*);
extern pa_mainloop_api* (*pa_mainloop_get_api)(pa_mainloop*);
extern int (*pa_mainloop_iterate)(pa_mainloop*, int, int*);
extern pa_mainloop* (*pa_mainloop_new)(void);

extern void (*pa_operation_unref)(pa_operation*);

extern int (*pa_stream_connect_playback)(pa_stream*,
                                         const char*,
                                         const pa_buffer_attr*,
                                         pa_stream_flags_t,
                                         const pa_cvolume*,
                                         pa_stream*);
extern pa_operation* (*pa_stream_cork)(pa_stream*,
                                       int,
                                       pa_stream_success_cb_t,
                                       void*);
extern int (*pa_stream_disconnect)(pa_stream*);
extern pa_stream_state_t (*pa_stream_get_state)(pa_stream*);
extern int (*pa_stream_get_time)(pa_stream*, pa_usec_t*);
extern int (*pa_stream_is_corked)(pa_stream*);
extern pa_stream* (*pa_stream_new)(pa_context*,
                                   const char*,
                                   const pa_sample_spec*,
                                   const pa_channel_map*);
extern void (*pa_stream_set_underflow_callback)(pa_stream*,
                                                pa_stream_notify_cb_t,
                                                void*);
extern void (*pa_stream_set_write_callback)(pa_stream*,
                                            pa_stream_request_cb_t,
                                            void*);
extern void (*pa_stream_unref)(pa_stream*);
extern size_t (*pa_stream_writable_size)(pa_stream*);
extern int (*pa_stream_write)(pa_stream*,
                              const void*,
                              size_t,
                              pa_free_cb_t,
                              int64_t,
                              pa_seek_mode_t);

}  // namespace pulse
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_PULSE_PULSE_DYNAMIC_LOAD_DISPATCHER_H_
