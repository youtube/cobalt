// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_ANDROID_SHARED_POSIX_EMU_INCLUDE_UNISTD_H_
#define STARBOARD_ANDROID_SHARED_POSIX_EMU_INCLUDE_UNISTD_H_

#include_next <unistd.h>

// sysconf()/pathconf() names that musl defines but the NDK doesn't. The
// values are outside bionic's ranges (_SC_* up to 0x9d, _PC_* up to 19)
// so bionic itself would still reject them.
// __wrap_sysconf()/__wrap_pathconf() in posix_emu/unistd.cc handle them.

#if !defined(_SC_V6_ILP32_OFF32)
#define _SC_V6_ILP32_OFF32 0x10000
#endif
#if !defined(_SC_V6_ILP32_OFFBIG)
#define _SC_V6_ILP32_OFFBIG 0x10001
#endif
#if !defined(_SC_V6_LP64_OFF64)
#define _SC_V6_LP64_OFF64 0x10002
#endif
#if !defined(_SC_V6_LPBIG_OFFBIG)
#define _SC_V6_LPBIG_OFFBIG 0x10003
#endif

#if !defined(_PC_SOCK_MAXBUF)
#define _PC_SOCK_MAXBUF 0x10000
#endif

#endif  // STARBOARD_ANDROID_SHARED_POSIX_EMU_INCLUDE_UNISTD_H_
