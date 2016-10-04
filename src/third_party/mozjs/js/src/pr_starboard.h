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

// This header defines the interface for the starboard based implementation of
// the subset of nspr that spider monkey 24 depends on.  It should directly
// match the nspr api, with the exception that accessing thread local data
// should use PRTLSIndex, rather than PRUintn.

#ifndef PR_STARBOARD_H_
#define PR_STARBOARD_H_

#include "starboard/condition_variable.h"
#include "starboard/log.h"
#include "starboard/memory.h"
#include "starboard/mutex.h"
#include "starboard/string.h"
#include "starboard/thread.h"
#include "starboard/types.h"

#define PR_CALLBACK

#define PR_MSEC_PER_SEC 1000L
#define PR_USEC_PER_SEC 1000000L
#define PR_NSEC_PER_SEC 1000000000L
#define PR_USEC_PER_MSEC 1000L
#define PR_NSEC_PER_MSEC 1000000L

typedef enum { PR_FAILURE = -1, PR_SUCCESS = 0 } PRStatus;

namespace pr_starboard {

// Utility function to map true to PR_SUCCESS and false to PR_FAILURE.
static inline PRStatus ToPRStatus(bool result) {
  return result ? PR_SUCCESS : PR_FAILURE;
}

}  // namespace pr_starboard

typedef enum PRThreadPriority {
  PR_PRIORITY_FIRST = 0,
  PR_PRIORITY_LOW = 0,
  PR_PRIORITY_NORMAL = 1,
  PR_PRIORITY_HIGH = 2,
  PR_PRIORITY_URGENT = 3,
  PR_PRIORITY_LAST = 3
} PRThreadPriority;

typedef enum PRThreadScope {
  PR_LOCAL_THREAD,
  PR_GLOBAL_THREAD,
  PR_GLOBAL_BOUND_THREAD
} PRThreadScope;

typedef enum PRThreadState {
  PR_JOINABLE_THREAD,
  PR_UNJOINABLE_THREAD
} PRThreadState;

typedef enum PRThreadType { PR_USER_THREAD, PR_SYSTEM_THREAD } PRThreadType;

typedef SbThreadLocalKey PRTLSIndex;
typedef uint32_t PRIntervalTime;

typedef int32_t PRInt32;
typedef uint32_t PRUint32;

typedef int64_t PRInt64;
typedef uint64_t PRUint64;

typedef void(PR_CALLBACK* PRThreadPrivateDTOR)(void* priv);

struct PRThread {
  PRThread(SbThread sb_thread) : sb_thread(sb_thread) {}
  SbThread sb_thread;
};

typedef SbMutex PRLock;

#define PR_INTERVAL_NO_WAIT 0UL
#define PR_INTERVAL_NO_TIMEOUT 0xffffffffUL

struct PRCondVar {
  SbConditionVariable sb_condition_variable;
  SbMutex* lock;
};

PRLock* PR_NewLock();

static inline void PR_Lock(PRLock* lock) {
  SbMutexAcquire(lock);
}

static inline void PR_Unlock(PRLock* lock) {
  SbMutexRelease(lock);
}

void PR_DestroyLock(PRLock* lock);

PRCondVar* PR_NewCondVar(PRLock* lock);

void PR_DestroyCondVar(PRCondVar* cvar);

PRStatus PR_WaitCondVar(PRCondVar* cvar, PRIntervalTime timeout);

static inline PRStatus PR_NotifyCondVar(PRCondVar* cvar) {
  return pr_starboard::ToPRStatus(
      SbConditionVariableSignal(&cvar->sb_condition_variable));
}

static inline PRStatus PR_NotifyAllCondVar(PRCondVar* cvar) {
  return pr_starboard::ToPRStatus(
      SbConditionVariableBroadcast(&cvar->sb_condition_variable));
}

typedef void (*PRThreadEntryPoint)(void*);

PRThread* PR_CreateThread(PRThreadType type,
                          PRThreadEntryPoint start,
                          void* arg,
                          PRThreadPriority priority,
                          PRThreadScope scope,
                          PRThreadState state,
                          PRUint32 stackSize);

static inline PRStatus PR_JoinThread(PRThread* pr_thread) {
  SB_DCHECK(pr_thread);
  SB_DCHECK(SbThreadIsValid(pr_thread->sb_thread));
  return pr_starboard::ToPRStatus(SbThreadJoin(pr_thread->sb_thread, NULL));
}

PRThread* PR_GetCurrentThread();

void PR_DetachThread();

PRStatus PR_NewThreadPrivateIndex(PRTLSIndex* newIndex,
                                  PRThreadPrivateDTOR destructor);

static inline PRStatus PR_SetThreadPrivate(PRTLSIndex index, void* priv) {
  return pr_starboard::ToPRStatus(SbThreadSetLocalValue(index, priv));
}

static inline void* PR_GetThreadPrivate(PRTLSIndex index) {
  return SbThreadGetLocalValue(index);
}

static inline void PR_SetCurrentThreadName(const char* name) {
  SbThreadSetName(name);
}

static inline PRUint32 PR_vsnprintf(char* out,
                                    PRUint32 outlen,
                                    const char* fmt,
                                    va_list ap) {
  return static_cast<PRUint32>(SbStringFormat(out, outlen, fmt, ap));
}

PRUint32 PR_snprintf(char* out, PRUint32 outlen, const char* fmt, ...);

PRIntervalTime PR_MillisecondsToInterval(PRUint32 milli);

PRUint32 PR_IntervalToMicroseconds(PRIntervalTime ticks);

struct PRCallOnceType {};
typedef PRStatus(PR_CALLBACK* PRCallOnceWithArgFN)(void* arg);

PRStatus PR_CallOnceWithArg(PRCallOnceType* once,
                            PRCallOnceWithArgFN func,
                            void* arg);

static inline PRUint32 PR_TicksPerSecond() {
  return 1000;
}

#endif  // #ifndef PR_STARBOARD_H_
