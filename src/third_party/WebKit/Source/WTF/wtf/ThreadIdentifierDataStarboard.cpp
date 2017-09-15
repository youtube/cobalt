/*
 * Copyright (C) 2009, 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if OS(STARBOARD)

#include "ThreadIdentifierDataStarboard.h"
#include "Threading.h"
#include "starboard/thread.h"

namespace WTF {

std::map<ThreadIdentifier, ThreadIdentifierData*> ThreadIdentifierData::local_handle;
Mutex ThreadIdentifierData::lock_local_handle;

SbThreadLocalKey ThreadIdentifierData::m_key = kSbThreadLocalKeyInvalid;

void threadDidExit(ThreadIdentifier);

ThreadIdentifierData::~ThreadIdentifierData()
{
    threadDidExit(m_identifier);
}

void ThreadIdentifierData::initializeOnce()
{
    m_key = SbThreadCreateLocalKey(destruct);
    if (!SbThreadIsValidLocalKey(m_key))
        CRASH();
}

ThreadIdentifier ThreadIdentifierData::identifier()
{
    ASSERT(SbThreadIsValidLocalKey(m_key));
    ThreadIdentifierData* threadIdentifierData = static_cast<ThreadIdentifierData*>(SbThreadGetLocalValue(m_key));

    return threadIdentifierData ? threadIdentifierData->m_identifier : 0;
}

void ThreadIdentifierData::initialize(ThreadIdentifier id)
{
    ASSERT(!identifier());
    ThreadIdentifierData *created = new ThreadIdentifierData(id);
    SbThreadSetLocalValue(m_key, created);
    {
      MutexLocker m(lock_local_handle);
      ASSERT(local_handle.find(id) == local_handle.end());
      local_handle.insert(std::make_pair(id, created));
    }
}

void ThreadIdentifierData::destruct(void* data)
{
    ThreadIdentifierData* threadIdentifierData = static_cast<ThreadIdentifierData*>(data);
    ASSERT(threadIdentifierData);

    if (threadIdentifierData->m_isDestroyedOnce) {
        delete threadIdentifierData;
        return;
    }

    threadIdentifierData->m_isDestroyedOnce = true;
    // Re-setting the value for key causes another destruct() call after all other thread-specific destructors were called.
    SbThreadSetLocalValue(m_key, threadIdentifierData);
}

} // namespace WTF

#endif // USE(PTHREADS)
