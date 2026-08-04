// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/threading/platform_thread.h"

#include "base/memory/cobalt_memory_context.h"
#include "base/strings/string_util.h"
#include "base/task/current_thread.h"
#include "base/threading/thread_id_name_manager.h"
#include "base/trace_event/base_tracing.h"

#if BUILDFLAG(IS_FUCHSIA)
#include "base/fuchsia/scheduler.h"
#endif

namespace base {

namespace {

constinit thread_local ThreadType current_thread_type = ThreadType::kDefault;

}  // namespace

void PlatformThreadId::WriteIntoTrace(perfetto::TracedValue&& context) const {
  perfetto::WriteIntoTracedValue(std::move(context), value_);
}

// static
void PlatformThreadBase::SetCurrentThreadType(ThreadType thread_type) {
  MessagePumpType message_pump_type = MessagePumpType::DEFAULT;
  if (CurrentIOThread::IsSet()) {
    message_pump_type = MessagePumpType::IO;
  }
#if !BUILDFLAG(IS_NACL)
  else if (CurrentUIThread::IsSet()) {
    message_pump_type = MessagePumpType::UI;
  }
#endif
  internal::SetCurrentThreadType(thread_type, message_pump_type);
}

// static
ThreadType PlatformThreadBase::GetCurrentThreadType() {
  return current_thread_type;
}

// static
std::optional<TimeDelta> PlatformThreadBase::GetThreadLeewayOverride() {
#if BUILDFLAG(IS_FUCHSIA)
  // On Fuchsia, all audio threads run with the CPU scheduling profile that uses
  // an interval of |kAudioSchedulingPeriod|. Using the default leeway may lead
  // to some tasks posted to audio threads to be executed too late (see
  // http://crbug.com/1368858).
  if (GetCurrentThreadType() == ThreadType::kRealtimeAudio) {
    return kAudioSchedulingPeriod;
  }
#endif
  return std::nullopt;
}

// static
void PlatformThreadBase::SetNameCommon(const std::string& name) {
  if (memory::GetCurrentMemoryContext() == memory::MemoryContext::kUnknown) {
    if (StartsWith(name, "V8", CompareCase::SENSITIVE)) {
      memory::SetCurrentMemoryContext(memory::MemoryContext::kScriptHeap);
    } else if (StartsWith(name, "Compositor", CompareCase::SENSITIVE) ||
               name == "CrGpuMain" || name == "RasterWorker" ||
               name == "GpuMemoryThread") {
      memory::SetCurrentMemoryContext(memory::MemoryContext::kGraphics);
    } else if (name == "Media" || name == "VideoDecoder" ||
               StartsWith(name, "FFmpeg", CompareCase::INSENSITIVE_ASCII)) {
      memory::SetCurrentMemoryContext(memory::MemoryContext::kMedia);
    }
  }
  ThreadIdNameManager::GetInstance()->SetName(name);
}

namespace internal {

void SetCurrentThreadType(ThreadType thread_type,
                          MessagePumpType pump_type_hint) {
  CHECK_LE(thread_type, ThreadType::kMaxValue);
  SetCurrentThreadTypeImpl(thread_type, pump_type_hint);
  current_thread_type = thread_type;
}

}  // namespace internal

}  // namespace base
