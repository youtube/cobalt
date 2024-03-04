// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/threading/scoped_blocking_call.h"

#include "base/lazy_instance.h"
#include "base/threading/thread_local.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "base/trace_event/base_tracing.h"
#include "base/tracing_buildflags.h"
#include "build/build_config.h"

#if BUILDFLAG(ENABLE_BASE_TRACING)
#include "third_party/perfetto/protos/perfetto/trace/track_event/source_location.pbzero.h"  // nogncheck
#endif  // BUILDFLAG(ENABLE_BASE_TRACING)

#if DCHECK_IS_ON()
#include "base/auto_reset.h"
#include "third_party/abseil-cpp/absl/base/attributes.h"
#if defined(STARBOARD)
#include "base/check_op.h"
#include "starboard/once.h"
#include "starboard/thread.h"
#endif
#endif

namespace base {

namespace {

#if DCHECK_IS_ON()
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

bool GetConstructionInProgress() {
  void* construction_in_progress = SbThreadGetLocalValue(s_thread_local_key);
  return !!construction_in_progress ? reinterpret_cast<intptr_t>(construction_in_progress) != 0 : false;
}
#else
// Used to verify that the trace events used in the constructor do not result in
// instantiating a ScopedBlockingCall themselves (which would cause an infinite
// reentrancy loop).
ABSL_CONST_INIT thread_local bool construction_in_progress = false;
#endif
#endif

}  // namespace

ScopedBlockingCall::ScopedBlockingCall(const Location& from_here,
                                       BlockingType blocking_type)
    : UncheckedScopedBlockingCall(
          blocking_type,
          UncheckedScopedBlockingCall::BlockingCallType::kRegular) {
#if DCHECK_IS_ON()
#if defined(STARBOARD)
  EnsureThreadLocalKeyInited();
  SbThreadSetLocalValue(s_thread_local_key, reinterpret_cast<void*>(static_cast<intptr_t>(true)));
#else
  const AutoReset<bool> resetter(&construction_in_progress, true, false);
#endif
#endif

  internal::AssertBlockingAllowed();
  TRACE_EVENT_BEGIN(
      "base", "ScopedBlockingCall", [&](perfetto::EventContext ctx) {
        ctx.event()->set_source_location_iid(
            base::trace_event::InternedSourceLocation::Get(&ctx, from_here));
      });

#if DCHECK_IS_ON() && defined(STARBOARD)
  SbThreadSetLocalValue(s_thread_local_key, reinterpret_cast<void*>(static_cast<intptr_t>(false)));
#endif
}

ScopedBlockingCall::~ScopedBlockingCall() {
  TRACE_EVENT_END("base");
}

namespace internal {

ScopedBlockingCallWithBaseSyncPrimitives::
    ScopedBlockingCallWithBaseSyncPrimitives(const Location& from_here,
                                             BlockingType blocking_type)
    : UncheckedScopedBlockingCall(
          blocking_type,
          UncheckedScopedBlockingCall::BlockingCallType::kBaseSyncPrimitives) {
#if DCHECK_IS_ON()
#if defined(STARBOARD)
  EnsureThreadLocalKeyInited();
  SbThreadSetLocalValue(s_thread_local_key, reinterpret_cast<void*>(static_cast<intptr_t>(true)));
#else
  const AutoReset<bool> resetter(&construction_in_progress, true, false);
#endif
#endif

  internal::AssertBaseSyncPrimitivesAllowed();
  TRACE_EVENT_BEGIN(
      "base", "ScopedBlockingCallWithBaseSyncPrimitives",
      [&](perfetto::EventContext ctx) {
        perfetto::protos::pbzero::SourceLocation* source_location_data =
            ctx.event()->set_source_location();
        source_location_data->set_file_name(from_here.file_name());
        source_location_data->set_function_name(from_here.function_name());
      });

#if DCHECK_IS_ON() && defined(STARBOARD)
  SbThreadSetLocalValue(s_thread_local_key, reinterpret_cast<void*>(static_cast<intptr_t>(false)));
#endif
}

ScopedBlockingCallWithBaseSyncPrimitives::
    ~ScopedBlockingCallWithBaseSyncPrimitives() {
  TRACE_EVENT_END("base");
}

}  // namespace internal

}  // namespace base
