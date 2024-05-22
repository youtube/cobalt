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
#include <pthread.h>

#include "base/check_op.h"
#endif

namespace base {

namespace {

#if defined(STARBOARD)

enum TlsValue {
  kDefault = 0,
  kBackground = 1,
  kUtility = 2,
  kResourceEfficient = 3 ,
  kCompositing = 4,
  kDisplayCritical = 5,
  kRealtimeAudio = 6,
  kMaxValue = kRealtimeAudio,
};

TlsValue ThreadTypeToTlsValue(ThreadType type) {
  if (type == ThreadType::kMaxValue) {
    type = ThreadType::kRealtimeAudio;
  }
  switch(type) {
    case ThreadType::kDefault:
      return TlsValue::kDefault;
    case ThreadType::kBackground:
      return TlsValue::kBackground;
    case  ThreadType::kUtility:
      return TlsValue::kUtility;
    case  ThreadType::kResourceEfficient:
      return TlsValue::kResourceEfficient;
    case  ThreadType::kCompositing:
      return TlsValue::kCompositing;
    case  ThreadType::kDisplayCritical:
      return TlsValue::kDisplayCritical;
    case  ThreadType::kRealtimeAudio:
      return TlsValue::kRealtimeAudio;
  }
}

ThreadType TlsValueToThreadType(TlsValue tls_value) {
  if (tls_value == TlsValue::kMaxValue) {
    tls_value = TlsValue::kRealtimeAudio;
  }
  switch(tls_value) {
    case TlsValue::kDefault:
      return ThreadType::kDefault;
    case TlsValue::kBackground:
      return ThreadType::kBackground;
    case TlsValue::kUtility:
      return ThreadType::kUtility;
    case TlsValue::kResourceEfficient:
      return ThreadType::kResourceEfficient;
    case TlsValue::kCompositing:
      return ThreadType::kCompositing;
    case  TlsValue::kDisplayCritical:
      return ThreadType::kDisplayCritical;
    case TlsValue::kRealtimeAudio:
      return ThreadType::kRealtimeAudio;
  }
}

ABSL_CONST_INIT pthread_once_t s_once_flag = PTHREAD_ONCE_INIT;
ABSL_CONST_INIT pthread_key_t s_thread_local_key = 0;

void InitThreadLocalKey() {
  int res = pthread_key_create(&s_thread_local_key , NULL);
  DCHECK(res == 0);
}

void EnsureThreadLocalKeyInited() {
  pthread_once(&s_once_flag, InitThreadLocalKey);
}

ThreadType GetCurrentThreadTypeValue() {
  EnsureThreadLocalKeyInited();
  return TlsValueToThreadType(TlsValue(reinterpret_cast<intptr_t>(pthread_getspecific(s_thread_local_key))));
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
  ThreadType type = GetCurrentThreadTypeValue(); 
  return type;
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
  pthread_setspecific(s_thread_local_key, reinterpret_cast<void*>(ThreadTypeToTlsValue(thread_type)));
#else
  current_thread_type = thread_type;
#endif
}

}  // namespace internal

}  // namespace base
