/*
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

#include "assembler/jit/ExecutableAllocator.h"

#if ENABLE_ASSEMBLER && WTF_OS_STARBOARD

#include "assembler/wtf/Assertions.h"
#include "assembler/wtf/VMTags.h"

#include "starboard/log.h"
#include "starboard/memory.h"
#include "starboard/system.h"

namespace JSC {

size_t ExecutableAllocator::determinePageSize()
{
    return SB_MEMORY_PAGE_SIZE;
}

ExecutablePool::Allocation ExecutableAllocator::systemAlloc(size_t n)
{
    int flags = kSbMemoryMapProtectRead | kSbMemoryMapProtectWrite | kSbMemoryMapProtectExec;
    void* allocation = SbMemoryMap(n, flags, "ExecutableAllocator");
    if (allocation == SB_MEMORY_MAP_FAILED)
    {
        SbSystemBreakIntoDebugger();
        allocation = NULL;
    }
    ExecutablePool::Allocation alloc = { reinterpret_cast<char*>(allocation), n };
    return alloc;
}

void ExecutableAllocator::systemRelease(const ExecutablePool::Allocation& alloc)
{
    bool result = SbMemoryUnmap(alloc.pages, alloc.size);
    ASSERT_UNUSED(result, result);
}

#if WTF_CPU_ARM_TRADITIONAL && WTF_COMPILER_RVCT
__asm void ExecutableAllocator::cacheFlush(void* code, size_t size)
{
    ARM
    push {r7}
    add r1, r1, r0
    mov r7, #0xf0000
    add r7, r7, #0x2
    mov r2, #0x0
    svc #0x0
    pop {r7}
    bx lr
}
#endif

}

#endif // HAVE(ASSEMBLER)
