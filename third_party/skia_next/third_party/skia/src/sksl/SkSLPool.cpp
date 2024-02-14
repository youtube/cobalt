/*
 * Copyright 2020 Google LLC
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "src/sksl/SkSLPool.h"

#include "include/private/SkSLDefines.h"

#ifndef STARBOARD // Avoid redefining VLOG from base/logging.h
#define VLOG(...) // printf(__VA_ARGS__)
#endif

namespace SkSL {

static thread_local MemoryPool* sMemPool = nullptr;

static MemoryPool* get_thread_local_memory_pool() {
    return sMemPool;
}

static void set_thread_local_memory_pool(MemoryPool* memPool) {
    sMemPool = memPool;
}

Pool::~Pool() {
    if (get_thread_local_memory_pool() == fMemPool.get()) {
        SkDEBUGFAIL("SkSL pool is being destroyed while it is still attached to the thread");
        set_thread_local_memory_pool(nullptr);
    }

    fMemPool->reportLeaks();
    SkASSERT(fMemPool->isEmpty());

#ifndef STARBOARD
    VLOG("DELETE Pool:0x%016llX\n", (uint64_t)fMemPool.get());
#endif
}

std::unique_ptr<Pool> Pool::Create() {
    auto pool = std::unique_ptr<Pool>(new Pool);
    pool->fMemPool = MemoryPool::Make(/*preallocSize=*/65536, /*minAllocSize=*/32768);
#ifndef STARBOARD
    VLOG("CREATE Pool:0x%016llX\n", (uint64_t)pool->fMemPool.get());
#endif
    return pool;
}

bool Pool::IsAttached() {
    return get_thread_local_memory_pool();
}

void Pool::attachToThread() {
#ifndef STARBOARD
    VLOG("ATTACH Pool:0x%016llX\n", (uint64_t)fMemPool.get());
#endif
    SkASSERT(get_thread_local_memory_pool() == nullptr);
    set_thread_local_memory_pool(fMemPool.get());
}

void Pool::detachFromThread() {
    MemoryPool* memPool = get_thread_local_memory_pool();
#ifndef STARBOARD
    VLOG("DETACH Pool:0x%016llX\n", (uint64_t)memPool);
#endif
    SkASSERT(memPool == fMemPool.get());
    memPool->resetScratchSpace();
    set_thread_local_memory_pool(nullptr);
}

void* Pool::AllocMemory(size_t size) {
    // Is a pool attached?
    MemoryPool* memPool = get_thread_local_memory_pool();
    if (memPool) {
        void* ptr = memPool->allocate(size);
#ifndef STARBOARD
        VLOG("ALLOC  Pool:0x%016llX  0x%016llX\n", (uint64_t)memPool, (uint64_t)ptr);
#endif
        return ptr;
    }

    // There's no pool attached. Allocate memory using the system allocator.
    void* ptr = ::operator new(size);
#ifndef STARBOARD
    VLOG("ALLOC  Pool:__________________  0x%016llX\n", (uint64_t)ptr);
#endif
    return ptr;
}

void Pool::FreeMemory(void* ptr) {
    // Is a pool attached?
    MemoryPool* memPool = get_thread_local_memory_pool();
    if (memPool) {
#ifndef STARBOARD
        VLOG("FREE   Pool:0x%016llX  0x%016llX\n", (uint64_t)memPool, (uint64_t)ptr);
#endif
        memPool->release(ptr);
        return;
    }

#ifndef STARBOARD
    // There's no pool attached. Free it using the system allocator.
    VLOG("FREE   Pool:__________________  0x%016llX\n", (uint64_t)ptr);
#endif
    ::operator delete(ptr);
}

}  // namespace SkSL
