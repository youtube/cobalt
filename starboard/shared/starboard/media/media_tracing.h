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

#ifndef STARBOARD_SHARED_STARBOARD_MEDIA_MEDIA_TRACING_H_
#define STARBOARD_SHARED_STARBOARD_MEDIA_MEDIA_TRACING_H_

#include <mutex>

#include "build/build_config.h"
#include "starboard/configuration.h"

#if !BUILDFLAG(COBALT_IS_RELEASE_BUILD)
// This cannot be `starboard`, as it conflicts with nested starboard namespaces.
#define PERFETTO_TRACK_EVENT_NAMESPACE starboard_tracing
#include "perfetto/tracing.h"

PERFETTO_DEFINE_CATEGORIES(perfetto::Category("starboard"));

#define MEDIA_TRACE_EVENT(...) TRACE_EVENT(__VA_ARGS__)
#define MEDIA_TRACE_EVENT_BEGIN(...) TRACE_EVENT_BEGIN(__VA_ARGS__)
#define MEDIA_TRACE_EVENT_END(...) TRACE_EVENT_END(__VA_ARGS__)

// Ideally, this should be called right after perfetto::Tracing::Initialize() in
// PerfettoTracedProcess::SetupClientLibrary().  However, this would require
// moving "media_tracing.*" to //starboard/common, and probably being exposed as
// STARBOARD_TRACE_EVENT.
// I would limit the scope to media for now, and expand its scope when needed.
inline void EnsureMediaTracingIsInitialized() {
  static std::once_flag s_once_flag;
  std::call_once(s_once_flag,
                 []() { starboard_tracing::TrackEvent::Register(); });
}

#else  // !BUILDFLAG(COBALT_IS_RELEASE_BUILD)

#define MEDIA_TRACE_EVENT(...) ((void)0)
#define MEDIA_TRACE_EVENT_BEGIN(...) ((void)0)
#define MEDIA_TRACE_EVENT_END(...) ((void)0)

inline void EnsureMediaTracingIsInitialized() {}

#endif  // !BUILDFLAG(COBALT_IS_RELEASE_BUILD)

#endif  // STARBOARD_SHARED_STARBOARD_MEDIA_MEDIA_TRACING_H_
