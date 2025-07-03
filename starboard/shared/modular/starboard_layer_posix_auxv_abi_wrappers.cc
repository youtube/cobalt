// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/shared/modular/starboard_layer_posix_auxv_abi_wrappers.h"

#include <errno.h>
#include <sys/auxv.h>

#include "starboard/shared/modular/starboard_layer_posix_errno_abi_wrappers.h"

unsigned long __abi_wrap_getauxval(unsigned long type) {
  switch (type) {
#if defined(AT_NULL)
    case MUSL_AT_NULL:
      return getauxval(AT_NULL);
#endif
#if defined(AT_IGNORE)
    case MUSL_AT_IGNORE:
      return getauxval(AT_IGNORE);
#endif
#if defined(AT_EXECFD)
    case MUSL_AT_EXECFD:
      return getauxval(AT_EXECFD);
#endif
#if defined(AT_PHDR)
    case MUSL_AT_PHDR:
      return getauxval(AT_PHDR);
#endif
#if defined(AT_PHENT)
    case MUSL_AT_PHENT:
      return getauxval(AT_PHENT);
#endif
#if defined(AT_PHNUM)
    case MUSL_AT_PHNUM:
      return getauxval(AT_PHNUM);
#endif
#if defined(AT_PAGESZ)
    case MUSL_AT_PAGESZ:
      return getauxval(AT_PAGESZ);
#endif
#if defined(AT_BASE)
    case MUSL_AT_BASE:
      return getauxval(AT_BASE);
#endif
#if defined(AT_FLAGS)
    case MUSL_AT_FLAGS:
      return getauxval(AT_FLAGS);
#endif
#if defined(AT_ENTRY)
    case MUSL_AT_ENTRY:
      return getauxval(AT_ENTRY);
#endif
#if defined(AT_NOTELF)
    case MUSL_AT_NOTELF:
      return getauxval(AT_NOTELF);
#endif
#if defined(AT_UID)
    case MUSL_AT_UID:
      return getauxval(AT_UID);
#endif
#if defined(AT_EUID)
    case MUSL_AT_EUID:
      return getauxval(AT_EUID);
#endif
#if defined(AT_GID)
    case MUSL_AT_GID:
      return getauxval(AT_GID);
#endif
#if defined(AT_EGID)
    case MUSL_AT_EGID:
      return getauxval(AT_EGID);
#endif
#if defined(AT_CLKTCK)
    case MUSL_AT_CLKTCK:
      return getauxval(AT_CLKTCK);
#endif
#if defined(AT_PLATFORM)
    case MUSL_AT_PLATFORM:
      return getauxval(AT_PLATFORM);
#endif
    case MUSL_AT_HWCAP:
      // NEON, AES, PMULL, and SHA1/256/512 are used for arm devices in
      // //third_party/boringssl.
      return getauxval(AT_HWCAP);
#if defined(AT_FPUCW)
    case MUSL_AT_FPUCW:
      return getauxval(AT_FPUCW);
#endif
#if defined(AT_DCACHEBSIZE)
    case MUSL_AT_DCACHEBSIZE:
      return getauxval(AT_DCACHEBSIZE);
#endif
#if defined(AT_ICACHEBSIZE)
    case MUSL_AT_ICACHEBSIZE:
      return getauxval(AT_ICACHEBSIZE);
#endif
#if defined(AT_UCACHEBSIZE)
    case MUSL_AT_UCACHEBSIZE:
      return getauxval(AT_UCACHEBSIZE);
#endif
#if defined(AT_IGNOREPPC)
    case MUSL_AT_IGNOREPPC:
      return getauxval(AT_IGNOREPPC);
#endif
#if defined(AT_SECURE)
    case MUSL_AT_SECURE:
      return getauxval(AT_SECURE);
#endif
#if defined(AT_BASE_PLATFORM)
    case MUSL_AT_BASE_PLATFORM:
      return getauxval(AT_BASE_PLATFORM);
#endif
#if defined(AT_RANDOM)
    case MUSL_AT_RANDOM:
      return getauxval(AT_RANDOM);
#endif
    case MUSL_AT_HWCAP2:
      // AES, PMULL, and SHA1/2 are used for arm devices in
      // //third_party/boringssl.
      return getauxval(AT_HWCAP2);
    case MUSL_AT_EXECFN:
      // Return 0 as the returned value to
      // //third_party/boringssl/src/crypto/fipsmodule/rand/urandom.c is only
      // used to print a warning message.
      return 0;
#if defined(AT_SYSINFO)
    case MUSL_AT_SYSINFO:
      return getauxval(AT_SYSINFO);
#endif
#if defined(AT_SYSINFO_EHDR)
    case MUSL_AT_SYSINFO_EHDR:
      return getauxval(AT_SYSINFO_EHDR);
#endif
#if defined(AT_L1I_CACHESHAPE)
    case MUSL_AT_L1I_CACHESHAPE:
      return getauxval(AT_L1I_CACHESHAPE);
#endif
#if defined(AT_L1D_CACHESHAPE)
    case MUSL_AT_L1D_CACHESHAPE:
      return getauxval(AT_L1D_CACHESHAPE);
#endif
#if defined(AT_L2_CACHESHAPE)
    case MUSL_AT_L2_CACHESHAPE:
      return getauxval(AT_L2_CACHESHAPE);
#endif
#if defined(AT_L3_CACHESHAPE)
    case MUSL_AT_L3_CACHESHAPE:
      return getauxval(AT_L3_CACHESHAPE);
#endif
#if defined(AT_L1I_CACHEGEOMETRY)
    case MUSL_AT_L1I_CACHEGEOMETRY:
      return getauxval(AT_L1I_CACHEGEOMETRY);
#endif
#if defined(AT_L1D_CACHEGEOMETRY)
    case MUSL_AT_L1D_CACHEGEOMETRY:
      return getauxval(AT_L1D_CACHEGEOMETRY);
#endif
#if defined(AT_L2_CACHEGEOMETRY)
    case MUSL_AT_L2_CACHEGEOMETRY:
      return getauxval(AT_L2_CACHEGEOMETRY);
#endif
#if defined(AT_L3_CACHEGEOMETRY)
    case MUSL_AT_L3_CACHEGEOMETRY:
      return getauxval(AT_L3_CACHEGEOMETRY);
#endif
#if defined(AT_MINSIGSTKSZ)
    case MUSL_AT_MINSIGSTKSZ:
      return getauxval(AT_MINSIGSTKSZ);
#endif
    default:
      errno = ENOENT;
      return 0;
  }
}
