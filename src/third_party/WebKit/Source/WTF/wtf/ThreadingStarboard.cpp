/*
 * Copyright (C) 2007, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Justin Haygood (jhaygood@reaktix.com)
 * Copyright (C) 2011 Research In Motion Limited. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// Adapted from ThreadingPthreads.cpp

#include "config.h"
#include "Threading.h"

#if OS(STARBOARD)

#include "starboard/condition_variable.h"
#include "starboard/log.h"
#include "starboard/mutex.h"
#include "starboard/thread.h"
#include "starboard/time.h"

#include "CurrentTime.h"
#include "DateMath.h"
#include "dtoa.h"
#include "dtoa/cached-powers.h"
#include "HashMap.h"
#include "RandomNumberSeed.h"
#include "StackStats.h"
#include "StdLibExtras.h"
#include "ThreadFunctionInvocation.h"
#include "ThreadIdentifierDataPthreads.h"
#include "ThreadSpecific.h"
#include "UnusedParam.h"
#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/WTFThreadData.h>

namespace WTF {

class ThreadState {
    WTF_MAKE_FAST_ALLOCATED;
public:
    enum JoinableState {
        Joinable, // The default thread state. The thread can be joined on.

        Joined, // Somebody waited on this thread to exit and this thread
                // finally exited. This state is here because there can be a
                // period of time between when the thread exits (which causes
                // join to return and the remainder of waitOnThreadCompletion to
                // run) and when threadDidExit is called. We need threadDidExit
                // to take charge and delete the thread data since there's
                // nobody else to pick up the slack in this case (since
                // waitOnThreadCompletion has already returned).

        Detached // The thread has been detached and can no longer be joined
                 // on. At this point, the thread must take care of cleaning up
                 // after itself.
    };

    // Currently all threads created by WTF start out as joinable.
    ThreadState(SbThread handle)
        : m_joinableState(Joinable)
        , m_didExit(false)
        , m_handle(handle)
    {
    }

    JoinableState joinableState() { return m_joinableState; }
    SbThread handle() { return m_handle; }
    void didBecomeDetached() { m_joinableState = Detached; }
    void didExit() { m_didExit = true; }
    void didJoin() { m_joinableState = Joined; }
    bool hasExited() { return m_didExit; }

private:
    JoinableState m_joinableState;
    bool m_didExit;
    SbThread m_handle;
};

typedef HashMap<ThreadIdentifier, OwnPtr<ThreadState> > ThreadMap;

static Mutex* atomicallyInitializedStaticMutex;

void unsafeThreadWasDetached(ThreadIdentifier);
void threadDidExit(ThreadIdentifier);
void threadWasJoined(ThreadIdentifier);

static Mutex& threadMapMutex()
{
    DEFINE_STATIC_LOCAL(Mutex, mutex, ());
    return mutex;
}

void initializeThreading()
{
    if (atomicallyInitializedStaticMutex)
        return;

    WTF::double_conversion::initialize();
    // StringImpl::empty() does not construct its static string in a threadsafe
    // fashion, so ensure it has been initialized from here.
    StringImpl::empty();
    atomicallyInitializedStaticMutex = new Mutex;
    threadMapMutex();
    initializeRandomNumberGenerator();
    ThreadIdentifierData::initializeOnce();
    StackStats::initialize();
    wtfThreadData();
    s_dtoaP5Mutex = new Mutex;
    initializeDates();
}

void lockAtomicallyInitializedStaticMutex()
{
    ASSERT(atomicallyInitializedStaticMutex);
    atomicallyInitializedStaticMutex->lock();
}

void unlockAtomicallyInitializedStaticMutex()
{
    atomicallyInitializedStaticMutex->unlock();
}

static ThreadMap& threadMap()
{
    DEFINE_STATIC_LOCAL(ThreadMap, map, ());
    return map;
}

static ThreadIdentifier identifierByHandle(const SbThread& handle)
{
    MutexLocker locker(threadMapMutex());

    ThreadMap::iterator i = threadMap().begin();
    for (; i != threadMap().end(); ++i) {
        if (SbThreadIsEqual(i->value->handle(), handle) && !i->value->hasExited())
            return i->key;
    }

    return 0;
}

static ThreadIdentifier establishIdentifierForHandle(const SbThread& handle)
{
    ASSERT(!identifierByHandle(handle));
    MutexLocker locker(threadMapMutex());
    static ThreadIdentifier identifierCount = 1;
    threadMap().add(identifierCount, adoptPtr(new ThreadState(handle)));
    return identifierCount++;
}

static SbThread handleForIdentifierWithLockAlreadyHeld(ThreadIdentifier id)
{
    return threadMap().get(id)->handle();
}

static void* wtfThreadEntryPoint(void* param)
{
    // Balanced by .leakPtr() in createThreadInternal.
    OwnPtr<ThreadFunctionInvocation> invocation = adoptPtr(static_cast<ThreadFunctionInvocation*>(param));
    invocation->function(invocation->data);
    return 0;
}

ThreadIdentifier createThreadInternal(ThreadFunction entryPoint, void* data, const char*n)
{
    OwnPtr<ThreadFunctionInvocation> invocation = adoptPtr(new ThreadFunctionInvocation(entryPoint, data));
    SbThread handle = SbThreadCreate(0, kSbThreadNoPriority,
                                     kSbThreadNoAffinity, true, NULL,
                                     wtfThreadEntryPoint, invocation.get());
    if (!SbThreadIsValid(handle)) {
        LOG_ERROR("Failed to create thread at entry point %p with data %p", wtfThreadEntryPoint, invocation.get());
        return 0;
    }

    // Balanced by adoptPtr() in wtfThreadEntryPoint.
    ThreadFunctionInvocation* leakedInvocation = invocation.leakPtr();
    UNUSED_PARAM(leakedInvocation);

    return establishIdentifierForHandle(handle);
}

void initializeCurrentThreadInternal(const char* threadName)
{
    SbThreadSetName(threadName);
    ThreadIdentifier id = identifierByHandle(SbThreadGetCurrent());
    ASSERT(id);
    ThreadIdentifierData::initialize(id);
}

int waitForThreadCompletion(ThreadIdentifier threadID)
{
    SbThread handle;
    ASSERT(threadID);

    {
        // We don't want to lock across the call to join, since that can block our thread and cause deadlock.
        MutexLocker locker(threadMapMutex());
        handle = handleForIdentifierWithLockAlreadyHeld(threadID);
        ASSERT(handle);
    }

    bool joinResult = SbThreadJoin(handle, NULL);
    if (!joinResult)
        LOG_ERROR("ThreadIdentifier %u was unable to be joined.\n", threadID);

    MutexLocker locker(threadMapMutex());
    ThreadState* state = threadMap().get(threadID);
    ASSERT(state);
    ASSERT(state->joinableState() == ThreadState::Joinable);

    // The thread has already exited, so clean up after it.
    if (state->hasExited())
        threadMap().remove(threadID);
    // The thread hasn't exited yet, so don't clean anything up. Just signal that we've already joined on it so that it will clean up after itself.
    else
        state->didJoin();

    return joinResult ? 0 : 1;
}

void detachThread(ThreadIdentifier threadID)
{
    ASSERT(threadID);

    MutexLocker locker(threadMapMutex());
    SbThread handle = handleForIdentifierWithLockAlreadyHeld(threadID);
    ASSERT(handle);

    SbThreadDetach(handle);

    ThreadState* state = threadMap().get(threadID);
    ASSERT(state);
    if (state->hasExited())
        threadMap().remove(threadID);
    else
        threadMap().get(threadID)->didBecomeDetached();
}

void threadDidExit(ThreadIdentifier threadID)
{
    MutexLocker locker(threadMapMutex());
    ThreadState* state = threadMap().get(threadID);
    ASSERT(state);

    state->didExit();

    if (state->joinableState() != ThreadState::Joinable)
        threadMap().remove(threadID);
}

ThreadIdentifier currentThread()
{
    ThreadIdentifier id = ThreadIdentifierData::identifier();
    if (id)
        return id;

    // Not a WTF-created thread, ThreadIdentifier is not established yet.
    id = establishIdentifierForHandle(SbThreadGetCurrent());
    ThreadIdentifierData::initialize(id);
    return id;
}

Mutex::Mutex()
{
    bool result = SbMutexCreate(&m_mutex);
    ASSERT(result);
}

Mutex::~Mutex()
{
    bool result = SbMutexDestroy(&m_mutex);
    ASSERT(result);
}

void Mutex::lock()
{
    SbMutexAcquire(&m_mutex);
}

bool Mutex::tryLock()
{
    SbMutexResult result = SbMutexAcquireTry(&m_mutex);

    if (SbMutexIsSuccess(result)) {
        return true;
    }
    if (result == kSbMutexBusy)
        return false;

    ASSERT(false);
    return false;
}

void Mutex::unlock()
{
    bool result = SbMutexRelease(&m_mutex);
    ASSERT(result);
}


ThreadCondition::ThreadCondition()
{
    bool result = SbConditionVariableCreate(&m_condition, NULL);
    ASSERT(result);
}

ThreadCondition::~ThreadCondition()
{
    bool result = SbConditionVariableDestroy(&m_condition);
    ASSERT(result);
}

void ThreadCondition::wait(Mutex& mutex)
{
    SbConditionVariableResult result =
            SbConditionVariableWait(&m_condition, &mutex.impl());
    ASSERT(result != kSbConditionVariableFailed);
}

bool ThreadCondition::timedWait(Mutex& mutex, double absoluteTime)
{
    double relativeTime = absoluteTime - currentTime();
    if (relativeTime < 0)
        return false;

    bool result = SbConditionVariableIsSignaled(
        SbConditionVariableWaitTimed(
            &m_condition, &mutex.impl(),
            static_cast<SbTime>(relativeTime * kSbTimeSecond)));
    return result;
}

void ThreadCondition::signal()
{
    bool result = SbConditionVariableSignal(&m_condition);
    ASSERT(result);
}

void ThreadCondition::broadcast()
{
    bool result = SbConditionVariableBroadcast(&m_condition);
    ASSERT(result);
}

}  // namespace WTF

#endif // OS(STARBOARD)
