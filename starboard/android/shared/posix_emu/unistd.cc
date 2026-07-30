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

#include <unistd.h>

extern "C" {

long __real_sysconf(int name);
long __real_pathconf(const char* path, int name);

// POSIX reserves EINVAL for invalid names. Bionic also returns it for some
// valid names it doesn't support, so those return -1 without setting errno.

long __wrap_sysconf(int name) {
  switch (name) {
    // _SC_V6_* aren't in the NDK (see include/unistd.h) and the legacy
    // _SC_XBS5_* are defined but rejected by bionic. sysconf() leaves the
    // result for an unsupported option unspecified, and <unistd.h> uses -1 for
    // "not supported", so -1 without EINVAL is conformant. Issue 8 renamed
    // _SC_V6_* to _SC_V7_* and dropped _SC_XBS5_*; both are documented in
    // Issues 6 and 7.
    //   https://pubs.opengroup.org/onlinepubs/9799919799/functions/sysconf.html
    //   https://pubs.opengroup.org/onlinepubs/9799919799/basedefs/unistd.h.html
    case _SC_V6_ILP32_OFF32:
    case _SC_V6_ILP32_OFFBIG:
    case _SC_V6_LP64_OFF64:
    case _SC_V6_LPBIG_OFFBIG:
    case _SC_XBS5_ILP32_OFF32:
    case _SC_XBS5_ILP32_OFFBIG:
    case _SC_XBS5_LP64_OFF64:
    case _SC_XBS5_LPBIG_OFFBIG:
      return -1;
    default:
      return __real_sysconf(name);
  }
}

long __wrap_pathconf(const char* path, int name) {
  switch (name) {
    // _PC_REC_*_XFER_SIZE are <limits.h> pathname variables. When one has no
    // limit for the file, pathconf() "shall return -1 without changing errno".
    // Bionic's EINVAL is the optional "association not supported" error, that
    // is also conformant, but -1 without errno matches the tests' expectations.
    // _PC_SOCK_MAXBUF is a non-POSIX extension the NDK lacks (see
    // include/unistd.h), handled the same way.
    //   https://pubs.opengroup.org/onlinepubs/9799919799/functions/fpathconf.html
    //   https://pubs.opengroup.org/onlinepubs/9799919799/basedefs/limits.h.html
    case _PC_REC_INCR_XFER_SIZE:
    case _PC_REC_MAX_XFER_SIZE:
    case _PC_SOCK_MAXBUF:
      return -1;
    default:
      return __real_pathconf(path, name);
  }
}

}  // extern "C"
