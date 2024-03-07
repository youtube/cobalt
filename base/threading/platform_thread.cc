// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/threading/platform_thread.h"

#include "base/task/current_thread.h"
#include "third_party/abseil-cpp/absl/base/attributes.h"

#if BUILDFLAG(IS_FUCHSIA)
#include "base/fuchsia/scheduler.h"
#endif

#if defined(STARBOARD)
#include "base/check_op.h"
#include "starboard/once.h"
#include "starboard/thread.h"
#endif

namespace base {

namespace {

#if defined(STARBOARD)
ABSL_CONST_INIT SbOnceControl s_once_flag = SB_ONCE_INITIALIZER;
ABSL_CONST_INIT SbThreadLocalKey s_thread_local_key = kSbThreadLocalKeyInvalid;

void InitThreadLocalKey() {
  s_thread_local_key = SbThreadCreateLocalKey(NULL);
  DCHECK(SbThreadIsValidLocalKey(s_thread_local_key));
}

void EnsureThreadLocalKeyInited() {
  SbOnce(&s_once_flag, InitThreadLocalKey);
  DCHECK(SbThreadIsValidLocalKey(s_thread_local_key));
}

ThreadType* GetThreadType() {
  return static_cast<ThreadType*>(
      SbThreadGetLocalValue(s_thread_local_key));
}
#else
ABSL_CONST_INIT thread_local ThreadType current_thread_type =
    ThreadType::kDefault;
#endif

}  // namespace

// static
void PlatformThread::SetCurrentThreadType(ThreadType thread_type) {
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
ThreadType PlatformThread::GetCurrentThreadType() {
#if defined(STARBOARD)
  ThreadType* rv = GetThreadType();
  return !!rv ? *rv : ThreadType::kDefault;
#else
  return current_thread_type;
#endif
}

// static
absl::optional<TimeDelta> PlatformThread::GetThreadLeewayOverride() {
#if BUILDFLAG(IS_FUCHSIA)
  // On Fuchsia, all audio threads run with the CPU scheduling profile that uses
  // an interval of |kAudioSchedulingPeriod|. Using the default leeway may lead
  // to some tasks posted to audio threads to be executed too late (see
  // http://crbug.com/1368858).
  if (GetCurrentThreadType() == ThreadType::kRealtimeAudio)
    return kAudioSchedulingPeriod;
#endif
  return absl::nullopt;
}

namespace internal {

void SetCurrentThreadType(ThreadType thread_type,
                          MessagePumpType pump_type_hint) {
  CHECK_LE(thread_type, ThreadType::kMaxValue);
  SetCurrentThreadTypeImpl(thread_type, pump_type_hint);
#if defined(STARBOARD)
  EnsureThreadLocalKeyInited();
  SbThreadSetLocalValue(s_thread_local_key, &thread_type);
#else
  current_thread_type = thread_type;
#endif
}

}  // namespace internal

}  // namespace base
