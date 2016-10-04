// Copyright 2016 Google Inc. All Rights Reserved.
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

#include "pr_starboard.h"

#include "starboard/condition_variable.h"
#include "starboard/log.h"
#include "starboard/memory.h"
#include "starboard/mutex.h"
#include "starboard/once.h"
#include "starboard/queue.h"
#include "starboard/thread.h"
#include "starboard/time.h"
#include "starboard/types.h"

namespace {

typedef starboard::Queue<bool> SetupSignalQueue;

// Utility function to convert a PRInterval to signed 64 bit integer
// microseconds.
int64_t PR_IntervalToMicrosecondsInt64(PRIntervalTime ticks) {
  uint32_t microseconds_as_uint32 = PR_IntervalToMicroseconds(ticks);
  int64_t microseconds_as_int64 = static_cast<int64_t>(microseconds_as_uint32);
  return microseconds_as_int64;
}

// Struct to bundle up arguments to be passed into SbThreadCreate.
struct ThreadEntryPointWrapperContext {
  ThreadEntryPointWrapperContext(void* pr_context,
                                 PRThread* pr_thread,
                                 PRThreadEntryPoint pr_entry_point,
                                 SetupSignalQueue* setup_signal_queue)
      : pr_context(pr_context),
        pr_thread(pr_thread),
        pr_entry_point(pr_entry_point),
        setup_signal_queue(setup_signal_queue) {}
  void* pr_context;
  PRThread* pr_thread;
  PRThreadEntryPoint pr_entry_point;
  SetupSignalQueue* setup_signal_queue;
};

// The thread local key that corresponds to where local PRThread* data is held.
SbThreadLocalKey g_pr_thread_local_key = kSbThreadLocalKeyInvalid;
// The SbOnceControlStructure to to ensure that the local key is only created
// once.
SbOnceControl g_pr_thread_key_once_control = SB_ONCE_INITIALIZER;

void PrThreadDtor(void* value) {
  PRThread* pr_thread = reinterpret_cast<PRThread*>(value);
  delete pr_thread;
}

void InitPrThreadKey() {
  g_pr_thread_local_key = SbThreadCreateLocalKey(PrThreadDtor);
}

SbThreadLocalKey GetPrThreadKey() {
  SB_CHECK(SbOnce(&g_pr_thread_key_once_control, InitPrThreadKey));
  return g_pr_thread_local_key;
}

void* ThreadEntryPointWrapper(void* context_as_void_pointer) {
  ThreadEntryPointWrapperContext* context =
      reinterpret_cast<ThreadEntryPointWrapperContext*>(
          context_as_void_pointer);
  void* pr_context = context->pr_context;
  PRThreadEntryPoint pr_entry_point = context->pr_entry_point;
  PRThread* pr_thread = context->pr_thread;
  SetupSignalQueue* setup_signal_queue = context->setup_signal_queue;

  delete context;

  pr_thread->sb_thread = SbThreadGetCurrent();
  SbThreadLocalKey key = GetPrThreadKey();
  SB_CHECK(SbThreadIsValidLocalKey(key));
  SbThreadSetLocalValue(key, pr_thread);

  setup_signal_queue->Put(true);
  pr_entry_point(pr_context);

  return NULL;
}

}  // namespace

PRLock* PR_NewLock() {
  PRLock* lock = new PRLock();
  if (!SbMutexCreate(lock)) {
    delete lock;
    return NULL;
  }
  return lock;
}

void PR_DestroyLock(PRLock* lock) {
  SB_DCHECK(lock);
  SbMutexDestroy(lock);
  delete lock;
}

PRCondVar* PR_NewCondVar(PRLock* lock) {
  SB_DCHECK(lock);
  PRCondVar* cvar = new PRCondVar();
  if (!SbConditionVariableCreate(&cvar->sb_condition_variable, lock)) {
    delete cvar;
    return NULL;
  }
  cvar->lock = lock;
  return cvar;
}

void PR_DestroyCondVar(PRCondVar* cvar) {
  SbConditionVariableDestroy(&cvar->sb_condition_variable);
  delete cvar;
}

PRStatus PR_WaitCondVar(PRCondVar* cvar, PRIntervalTime timeout) {
  SbConditionVariableResult result;
  if (timeout == PR_INTERVAL_NO_WAIT) {
    result = SbConditionVariableWaitTimed(&cvar->sb_condition_variable,
                                          cvar->lock, 0);
  } else if (timeout == PR_INTERVAL_NO_TIMEOUT) {
    result = SbConditionVariableWait(&cvar->sb_condition_variable, cvar->lock);
  } else {
    int64_t microseconds = PR_IntervalToMicrosecondsInt64(timeout);
    result = SbConditionVariableWaitTimed(&cvar->sb_condition_variable,
                                          cvar->lock, microseconds);
  }
  return pr_starboard::ToPRStatus(result != kSbConditionVariableFailed);
}

PRThread* PR_GetCurrentThread() {
  SbThreadLocalKey key = GetPrThreadKey();
  SB_CHECK(SbThreadIsValidLocalKey(key));

  PRThread* value = static_cast<PRThread*>(SbThreadGetLocalValue(key));
  // We could potentially be a thread that was not created through
  // PR_CreateThread.  In this case, we must allocate a PRThread and do the
  // setup that would normally have been done in PR_CreateThread.
  if (!value) {
    PRThread* pr_thread = new PRThread(SbThreadGetCurrent());
    SbThreadSetLocalValue(key, pr_thread);
    value = pr_thread;
  }

  return value;
}

uint32_t PR_snprintf(char* out, uint32_t outlen, const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  uint32_t ret = PR_vsnprintf(out, outlen, fmt, args);
  va_end(args);
  return ret;
}

PRThread* PR_CreateThread(PRThreadType type,
                          PRThreadEntryPoint start,
                          void* arg,
                          PRThreadPriority priority,
                          PRThreadScope scope,
                          PRThreadState state,
                          PRUint32 stackSize) {
  int64_t sb_stack_size = static_cast<int64_t>(stackSize);

  SbThreadPriority sb_priority;
  switch (priority) {
    case PR_PRIORITY_LOW:
      sb_priority = kSbThreadPriorityLow;
      break;
    case PR_PRIORITY_NORMAL:
      sb_priority = kSbThreadPriorityNormal;
      break;
    case PR_PRIORITY_HIGH:
      sb_priority = kSbThreadPriorityHigh;
      break;
    case PR_PRIORITY_LAST:
      sb_priority = kSbThreadPriorityHighest;
      break;
    default:
      sb_priority = kSbThreadNoPriority;
  }

  SbThreadAffinity sb_affinity = kSbThreadNoAffinity;

  SB_DCHECK(state == PR_JOINABLE_THREAD || state == PR_UNJOINABLE_THREAD);
  bool sb_joinable = (state == PR_JOINABLE_THREAD);

  // This heap allocated PRThread object will have a pointer to it stored in
  // the newly created child thread's thread local storage.  Once the newly
  // created child thread finishes, it will be freed in the destructor
  // associated with it through thread local storage.
  PRThread* pr_thread = new PRThread(kSbThreadInvalid);

  // Utility queue for the ThreadEntryWrapper to signal us once it's done
  // running its initial setup and we can safely exit.
  SetupSignalQueue setup_signal_queue;

  // This heap allocated context object is freed after
  // ThreadEntryPointWrapper's initial setup is complete, right before the
  // nspr level entry point is run.
  ThreadEntryPointWrapperContext* context = new ThreadEntryPointWrapperContext(
      arg, pr_thread, start, &setup_signal_queue);

  // Note that pr_thread->sb_thread will be set to the correct value in the
  // setup section of ThreadEntryPointWrapper.  It is done there rather than
  // here to account for the unlikely but possible case in which we enter the
  // newly created child thread, and then the child thread passes references
  // to itself off into its potential children or co-threads that interact
  // with it before we can copy what SbThreadCreate returns into
  // pr_thread->sb_thread from this current thread.
  SbThreadCreate(sb_stack_size, sb_priority, sb_affinity, sb_joinable, NULL,
                 ThreadEntryPointWrapper, context);

  // Now we must wait for the setup section of ThreadEntryPointWrapper to run
  // and initialize pr_thread (both the struct itself and the corresponding
  // new thread's private data) before we can safely return.  We expect to
  // receive true rather than false by convention.
  bool setup_signal = setup_signal_queue.Get();
  SB_DCHECK(setup_signal);

  return pr_thread;
}

PRStatus PR_CallOnceWithArg(PRCallOnceType* once,
                            PRCallOnceWithArgFN func,
                            void* arg) {
  SB_NOTREACHED() << "Not implemented";
  return PR_FAILURE;
}

PRStatus PR_NewThreadPrivateIndex(PRTLSIndex* newIndex,
                                  PRThreadPrivateDTOR destructor) {
  SbThreadLocalKey key = SbThreadCreateLocalKey(destructor);
  if (!SbThreadIsValidLocalKey(key)) {
    return pr_starboard::ToPRStatus(false);
  }
  *newIndex = key;
  return pr_starboard::ToPRStatus(true);
}

PRIntervalTime PR_MillisecondsToInterval(PRUint32 milli) {
  PRUint64 tock = static_cast<PRUint64>(milli);
  PRUint64 msecPerSec = static_cast<PRInt64>(PR_MSEC_PER_SEC);
  PRUint64 rounding = static_cast<PRInt64>(PR_MSEC_PER_SEC >> 1);
  PRUint64 tps = static_cast<PRInt64>(PR_TicksPerSecond());

  tock *= tps;
  tock += rounding;
  tock /= msecPerSec;

  PRUint64 ticks = static_cast<PRUint64>(tock);
  return ticks;
}

PRUint32 PR_IntervalToMicroseconds(PRIntervalTime ticks) {
  PRUint64 tock = static_cast<PRInt64>(ticks);
  PRUint64 usecPerSec = static_cast<PRInt64>(PR_USEC_PER_SEC);
  PRUint64 tps = static_cast<PRInt64>(PR_TicksPerSecond());
  PRUint64 rounding = static_cast<PRUint64>(tps) >> 1;

  tock *= usecPerSec;
  tock += rounding;
  tock /= tps;

  PRUint32 micro = static_cast<PRUint32>(tock);
  return micro;
}
