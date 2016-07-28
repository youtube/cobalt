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

// OSAllocator implementation based on OSAllocatorPosix, but for Starboard.

#include "assembler/wtf/Platform.h"

#if WTF_OS_STARBOARD

#include "yarr/OSAllocator.h"

#include "starboard/log.h"
#include "starboard/memory.h"
#include "starboard/system.h"

namespace WTF {

void* OSAllocator::reserveUncommitted(size_t bytes, Usage usage, bool writable, bool executable)
{
    // Memory mapped through SbMemoryMap is committed.
    return reserveAndCommit(bytes, usage, writable, executable);
}

void* OSAllocator::reserveAndCommit(size_t bytes, Usage usage, bool writable, bool executable)
{
    int flags = kSbMemoryMapProtectRead;
    if (writable) {
        flags |= kSbMemoryMapProtectWrite;
    }
    if (executable) {
#if SB_CAN(MAP_EXECUTABLE_MEMORY)
        flags |= kSbMemoryMapProtectExec;
#else
        // Terminate the application in this case.
        SbSystemBreakIntoDebugger();
#endif
    }

    void* result = SbMemoryMap(bytes, flags, "OSAllocator");
    if (result == SB_MEMORY_MAP_FAILED) {
        SbSystemBreakIntoDebugger();
    }
    return result;
}

void OSAllocator::commit(void* address, size_t bytes, bool, bool)
{
    UNUSED_PARAM(address);
    UNUSED_PARAM(bytes);
}

void OSAllocator::decommit(void* address, size_t bytes)
{
    UNUSED_PARAM(address);
    UNUSED_PARAM(bytes);
}

void OSAllocator::releaseDecommitted(void* address, size_t bytes)
{
    if (!SbMemoryUnmap(address, bytes)) {
        SbSystemBreakIntoDebugger();
    }
}

} // namespace WTF

#endif
