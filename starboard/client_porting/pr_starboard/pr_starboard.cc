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

#include "starboard/client_porting/pr_starboard/pr_starboard.h"

#include "nb/thread_local_object.h"
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

typedef starboard::Queue<PRThread*> SetupSignalQueue;
typedef nb::ThreadLocalObject<PRThread> ThreadLocalPRThread;
SB_ONCE_INITIALIZE_FUNCTION(ThreadLocalPRThread,
                            g_local_pr_thread);

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
                                 PRThreadEntryPoint pr_entry_point,
                                 SetupSignalQueue* setup_signal_queue)
      : pr_context(pr_context),
        pr_entry_point(pr_entry_point),
        setup_signal_queue(setup_signal_queue) {}
  void* pr_context;
  PRThreadEntryPoint pr_entry_point;
  SetupSignalQueue* setup_signal_queue;
};

void* ThreadEntryPointWrapper(void* context_as_void_pointer) {
  ThreadEntryPointWrapperContext* context =
      reinterpret_cast<ThreadEntryPointWrapperContext*>(
          context_as_void_pointer);
  void* pr_context = context->pr_context;
  PRThreadEntryPoint pr_entry_point = context->pr_entry_point;
  SetupSignalQueue* setup_signal_queue = context->setup_signal_queue;

  delete context;

  SB_DCHECK(g_local_pr_thread()->GetIfExists() == NULL);
  PRThread* pr_thread = g_local_pr_thread()->GetOrCreate(SbThreadGetCurrent());
  SB_DCHECK(pr_thread);
  setup_signal_queue->Put(pr_thread);
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
  return g_local_pr_thread()->GetOrCreate(SbThreadGetCurrent());
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

  // A queue that serves two purposes.  The first is that it is a way for our
  // child thread to signal to us that it is done with its wrapper level
  // setup, and that we can safely return.  The second, is that it is a
  // channel for it to pass us the address of the child thread's |PRThread|
  // object.
  SetupSignalQueue setup_signal_queue;

  // This heap allocated context object is freed after the initial setup of
  // |ThreadEntryPointWrapper| is complete, right before the nspr level entry
  // point is run.
  ThreadEntryPointWrapperContext* context =
      new ThreadEntryPointWrapperContext(arg, start, &setup_signal_queue);

  SbThreadCreate(sb_stack_size, sb_priority, sb_affinity, sb_joinable, NULL,
                 ThreadEntryPointWrapper, context);

  // Now we must wait for the setup section of |ThreadEntryPointWrapper| to
  // run and pass us the initialized |pr_thread|.
  PRThread* child_pr_thread = setup_signal_queue.Get();
  SB_DCHECK(child_pr_thread);
  SB_DCHECK(SbThreadIsValid(child_pr_thread->sb_thread));

  return child_pr_thread;
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
