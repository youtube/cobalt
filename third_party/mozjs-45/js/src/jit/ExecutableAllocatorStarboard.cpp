/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 *
 * Copyright (C) 2008 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "mozilla/DebugOnly.h"
#include "mozilla/TaggedAnonymousMemory.h"

#include "jit/ExecutableAllocator.h"
#include "js/Utility.h"

using namespace js::jit;

size_t
ExecutableAllocator::determinePageSize()
{
    return SB_MEMORY_PAGE_SIZE;
}

void*
js::jit::AllocateExecutableMemory(void* addr, size_t bytes, unsigned permissions, const char* tag,
                                  size_t pageSize)
{
    // Starboard memory has no concept of passing in an address.
    SB_DCHECK(addr == nullptr);
    MOZ_ASSERT(bytes % pageSize == 0);

#if !SB_CAN(MAP_EXECUTABLE_MEMORY)
    SB_NOTREACHED();
    return nullptr;
#elif !(SB_API_VERSION >= SB_MMAP_REQUIRED_VERSION || SB_HAS(MMAP))
    SB_NOTIMPLEMENTED();
    return nullptr;
#else
    void* p = SbMemoryMap(bytes, kSbMemoryMapProtectReadWrite | kSbMemoryMapProtectExec, tag);
    return (p == SB_MEMORY_MAP_FAILED) ? nullptr : p;
#endif
}

void
js::jit::DeallocateExecutableMemory(void* addr, size_t bytes, size_t pageSize)
{
    MOZ_ASSERT(bytes % pageSize == 0);
#if SB_API_VERSION >= SB_MMAP_REQUIRED_VERSION || SB_HAS(MMAP)
    mozilla::DebugOnly<bool> result = SbMemoryUnmap(addr, bytes);
    MOZ_ASSERT(result);
#else   // SB_API_VERSION >= SB_MMAP_REQUIRED_VERSION || SB_HAS(MMAP)
    SB_NOTIMPLEMENTED();
#endif  // SB_API_VERSION >= SB_MMAP_REQUIRED_VERSION || SB_HAS(MMAP)
}

ExecutablePool::Allocation
ExecutableAllocator::systemAlloc(size_t n)
{
    void* allocation = AllocateExecutableMemory(nullptr, n, initialProtectionFlags(Executable),
                                                "js-jit-code", pageSize);
    ExecutablePool::Allocation alloc = { reinterpret_cast<char*>(allocation), n };
    return alloc;
}

void
ExecutableAllocator::systemRelease(const ExecutablePool::Allocation& alloc)
{
    DeallocateExecutableMemory(alloc.pages, alloc.size, pageSize);
}

void
ExecutableAllocator::reprotectRegion(void* start, size_t size, ProtectionSetting setting)
{
    // Starboard has no concept of reprotecting memory.
    SB_NOTREACHED();
}

/* static */ unsigned
ExecutableAllocator::initialProtectionFlags(ProtectionSetting protection)
{
#if SB_CAN(MAP_EXECUTABLE_MEMORY)
    return kSbMemoryMapProtectRead | kSbMemoryMapProtectExec;
#else
    return kSbMemoryMapProtectReadWrite;
#endif
}
