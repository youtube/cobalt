// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/thread.h"

#include <windows.h>

#include "starboard/shared/win32/thread_private.h"

using starboard::shared::win32::GetCurrentSbThreadPrivate;
using starboard::shared::win32::SbThreadPrivate;

namespace {

// The code below is from
// https://msdn.microsoft.com/en-us/library/xcb2z8hs.aspx
// A |dwThreadID| of -1 means "current thread";
//
// Usage: SetThreadName ((DWORD)-1, "MainThread");
//
const DWORD MS_VC_EXCEPTION = 0x406D1388;
#pragma pack(push, 8)
typedef struct tagTHREADNAME_INF {
  DWORD dwType;      // Must be 0x1000.
  LPCSTR szName;     // Pointer to name (in user addr space).
  DWORD dwThreadID;  // Thread ID (-1=caller thread).
  DWORD dwFlags;     // Reserved for future use, must be zero.
} THREADNAME_INFO;
#pragma pack(pop)
void SetThreadName(DWORD dwThreadID, const char* threadName) {
  THREADNAME_INFO info;
  info.dwType = 0x1000;
  info.szName = threadName;
  info.dwThreadID = dwThreadID;
  info.dwFlags = 0;
#pragma warning(push)
#pragma warning(disable : 6320 6322)
  __try {
    RaiseException(MS_VC_EXCEPTION, 0, sizeof(info) / sizeof(ULONG_PTR),
                   reinterpret_cast<ULONG_PTR*>(&info));
  } __except(EXCEPTION_EXECUTE_HANDLER) {
  }
#pragma warning(pop)
}

}  // namespace

void SbThreadSetName(const char* name) {
  SbThreadPrivate* thread_private = GetCurrentSbThreadPrivate();

  // We store the thread name in our own TLS context as well as telling
  // the OS because it's much easier to retrieve from our own TLS context.
  thread_private->name_ = name;
  SetThreadName(static_cast<DWORD>(-1), name);
}
