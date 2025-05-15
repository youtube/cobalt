//
// Copyright 2023 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// GlobalMutex.cpp: Defines Global Mutex and utilities.

#include "libANGLE/GlobalMutex.h"

#include <atomic>

#include "common/debug.h"
#include "common/system_utils.h"

namespace egl
{
namespace priv
{
using GlobalMutexType = std::mutex;

#if !defined(ANGLE_ENABLE_ASSERTS) && !defined(ANGLE_ENABLE_GLOBAL_MUTEX_RECURSION)
// Default version.
class GlobalMutex final : angle::NonCopyable
{
  public:
    ANGLE_INLINE void lock() { mMutex.lock(); }
    ANGLE_INLINE void unlock() { mMutex.unlock(); }

  protected:
    GlobalMutexType mMutex;
};
#endif

#if defined(ANGLE_ENABLE_ASSERTS) && !defined(ANGLE_ENABLE_GLOBAL_MUTEX_RECURSION)
// Debug version.
class GlobalMutex final : angle::NonCopyable
{
  public:
    ANGLE_INLINE void lock()
    {
        const angle::ThreadId threadId = angle::GetCurrentThreadId();
        ASSERT(getOwnerThreadId() != threadId);
        mMutex.lock();
        ASSERT(getOwnerThreadId() == angle::InvalidThreadId());
        mOwnerThreadId.store(threadId, std::memory_order_relaxed);
    }

    ANGLE_INLINE void unlock()
    {
        ASSERT(getOwnerThreadId() == angle::GetCurrentThreadId());
        mOwnerThreadId.store(angle::InvalidThreadId(), std::memory_order_relaxed);
        mMutex.unlock();
    }

  private:
    ANGLE_INLINE angle::ThreadId getOwnerThreadId() const
    {
        return mOwnerThreadId.load(std::memory_order_relaxed);
    }

    GlobalMutexType mMutex;
    std::atomic<angle::ThreadId> mOwnerThreadId{angle::InvalidThreadId()};
};
#endif  // defined(ANGLE_ENABLE_ASSERTS) && !defined(ANGLE_ENABLE_GLOBAL_MUTEX_RECURSION)

#if defined(ANGLE_ENABLE_GLOBAL_MUTEX_RECURSION)
// Recursive version.
class GlobalMutex final : angle::NonCopyable
{
  public:
    ANGLE_INLINE void lock()
    {
        const angle::ThreadId threadId = angle::GetCurrentThreadId();
        if (ANGLE_UNLIKELY(!mMutex.try_lock()))
        {
            if (ANGLE_UNLIKELY(getOwnerThreadId() == threadId))
            {
                ASSERT(mLockLevel > 0);
                ++mLockLevel;
                return;
            }
            mMutex.lock();
        }
        ASSERT(getOwnerThreadId() == angle::InvalidThreadId());
        ASSERT(mLockLevel == 0);
        mOwnerThreadId.store(threadId, std::memory_order_relaxed);
        mLockLevel = 1;
    }

    ANGLE_INLINE void unlock()
    {
        ASSERT(getOwnerThreadId() == angle::GetCurrentThreadId());
        ASSERT(mLockLevel > 0);
        if (ANGLE_LIKELY(--mLockLevel == 0))
        {
            mOwnerThreadId.store(angle::InvalidThreadId(), std::memory_order_relaxed);
            mMutex.unlock();
        }
    }

  private:
    ANGLE_INLINE angle::ThreadId getOwnerThreadId() const
    {
        return mOwnerThreadId.load(std::memory_order_relaxed);
    }

    GlobalMutexType mMutex;
    std::atomic<angle::ThreadId> mOwnerThreadId{angle::InvalidThreadId()};
    uint32_t mLockLevel = 0;
};
#endif  // defined(ANGLE_ENABLE_GLOBAL_MUTEX_RECURSION)
}  // namespace priv

namespace
{
ANGLE_REQUIRE_CONSTANT_INIT std::atomic<priv::GlobalMutex *> g_Mutex(nullptr);
static_assert(std::is_trivially_destructible<decltype(g_Mutex)>::value,
              "global mutex is not trivially destructible");

priv::GlobalMutex *AllocateGlobalMutexImpl()
{
    priv::GlobalMutex *currentMutex = nullptr;
    std::unique_ptr<priv::GlobalMutex> newMutex(new priv::GlobalMutex());
    if (g_Mutex.compare_exchange_strong(currentMutex, newMutex.get(), std::memory_order_release,
                                        std::memory_order_acquire))
    {
        return newMutex.release();
    }
    return currentMutex;
}

priv::GlobalMutex *GetGlobalMutex()
{
    priv::GlobalMutex *mutex = g_Mutex.load(std::memory_order_acquire);
    return mutex != nullptr ? mutex : AllocateGlobalMutexImpl();
}
}  // anonymous namespace

// ScopedGlobalMutexLock implementation.
ScopedGlobalMutexLock::ScopedGlobalMutexLock() : mMutex(*GetGlobalMutex())
{
    mMutex.lock();
}

ScopedGlobalMutexLock::~ScopedGlobalMutexLock()
{
    mMutex.unlock();
}

// ScopedOptionalGlobalMutexLock implementation.
ScopedOptionalGlobalMutexLock::ScopedOptionalGlobalMutexLock(bool enabled)
{
    if (enabled)
    {
        mMutex = GetGlobalMutex();
        mMutex->lock();
    }
    else
    {
        mMutex = nullptr;
    }
}

ScopedOptionalGlobalMutexLock::~ScopedOptionalGlobalMutexLock()
{
    if (mMutex != nullptr)
    {
        mMutex->unlock();
    }
}

// Global functions.
#if defined(ANGLE_PLATFORM_WINDOWS) && !defined(ANGLE_STATIC)
void AllocateGlobalMutex()
{
    (void)AllocateGlobalMutexImpl();
}

void DeallocateGlobalMutex()
{
    priv::GlobalMutex *mutex = g_Mutex.exchange(nullptr);
    if (mutex != nullptr)
    {
        {
            // Wait for the mutex to become released by other threads before deleting.
            std::lock_guard<priv::GlobalMutex> lock(*mutex);
        }
        delete mutex;
    }
}
#endif

}  // namespace egl
