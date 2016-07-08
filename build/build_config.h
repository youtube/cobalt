// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file adds defines about the platform we're currently building on.
//  Operating System:
//    OS_WIN / OS_MACOSX / OS_LINUX / OS_POSIX (MACOSX or LINUX)
//    OS_STARBOARD
//  Compiler:
//    COMPILER_MSVC / COMPILER_GCC
//  Processor:
//    ARCH_CPU_X86 / ARCH_CPU_X86_64 / ARCH_CPU_X86_FAMILY (X86 or X86_64)
//    ARCH_CPU_PPC_FAMILY
//    ARCH_CPU_MIPS / ARCH_CPU_MIPSEL / ARCH_CPU_MIPS_FAMILY
//    ARCH_CPU_ARM / ARCH_CPU_ARMEL / ARCH_CPU_ARM_FAMILY
//    ARCH_CPU_32_BITS / ARCH_CPU_64_BITS
//    ARCH_CPU_BIG_ENDIAN / ARCH_CPU_LITTLE_ENDIAN

#ifndef BUILD_BUILD_CONFIG_H_
#define BUILD_BUILD_CONFIG_H_

#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif

// A set of macros to use for platform detection.
#if defined(STARBOARD)
#define OS_STARBOARD 1
#elif defined(__APPLE__)
#define OS_MACOSX 1
#if defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE
#define OS_IOS 1
#endif  // defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE
#elif defined(ANDROID)
#define OS_ANDROID 1
#elif defined(__native_client__)
#define OS_NACL 1
#elif defined(__linux__)
#define OS_LINUX 1
// Use TOOLKIT_GTK on linux if TOOLKIT_VIEWS isn't defined.
#if !defined(TOOLKIT_VIEWS)
#define TOOLKIT_GTK
#endif
#elif defined(__LB_SHELL__)
// NO toolkit!
#elif defined(_WIN32)
#define OS_WIN 1
#define TOOLKIT_VIEWS 1
#elif defined(__FreeBSD__)
#define OS_FREEBSD 1
#define TOOLKIT_GTK
#elif defined(__OpenBSD__)
#define OS_OPENBSD 1
#define TOOLKIT_GTK
#elif defined(__sun)
#define OS_SOLARIS 1
#define TOOLKIT_GTK
#else
#error Please add support for your platform in build/build_config.h
#endif

#if defined(USE_OPENSSL) && defined(USE_NSS)
#error Cannot use both OpenSSL and NSS
#endif

// For access to standard BSD features, use OS_BSD instead of a
// more specific macro.
#if defined(OS_FREEBSD) || defined(OS_OPENBSD)
#define OS_BSD 1
#endif

// For access to standard POSIXish features, use OS_POSIX instead of a
// more specific macro.
#if defined(OS_MACOSX) || defined(OS_LINUX) || defined(OS_FREEBSD) ||     \
    defined(OS_OPENBSD) || defined(OS_SOLARIS) || defined(OS_ANDROID) ||  \
    defined(OS_NACL) || defined(__LB_SHELL__)
#define OS_POSIX 1
#endif

#if defined(OS_POSIX) && !defined(OS_MACOSX) && !defined(OS_ANDROID) && \
    !defined(OS_NACL) && !defined(__LB_SHELL__)
#define USE_X11 1  // Use X for graphics.
#endif

// Use tcmalloc
#if (defined(OS_WIN) || defined(OS_LINUX)) && !defined(NO_TCMALLOC)
#define USE_TCMALLOC 1
#endif

// Compiler detection.
#if defined(__SNC__)
#define COMPILER_SNC
#endif

#if defined(__ghs) || defined(__ghs__)
#define COMPILER_GHS 1
#endif

#if defined(__GNUC__)
#define COMPILER_GCC 1
#elif defined(_MSC_VER)
#define COMPILER_MSVC 1
#else
#error Please add support for your compiler in build/build_config.h
#endif

// Processor architecture detection.  For more info on what's defined, see:
//   http://msdn.microsoft.com/en-us/library/b0084kay.aspx
//   http://www.agner.org/optimize/calling_conventions.pdf
//   or with gcc, run: "echo | gcc -E -dM -"
#if defined(OS_STARBOARD)
#  include "starboard/configuration.h"
#  if SB_IS(32_BIT)
#    define ARCH_CPU_32_BITS 1
#  elif SB_IS(64_BIT)
#    define ARCH_CPU_64_BITS 1
#  endif  // SB_IS(32_BIT)
#  if SB_IS(BIG_ENDIAN)
#    define ARCH_CPU_BIG_ENDIAN 1
#  else  // SB_IS(BIG_ENDIAN)
#    define ARCH_CPU_LITTLE_ENDIAN 1
#  endif  // SB_IS(BIG_ENDIAN)
#  if SB_IS(ARCH_X86)
#    define ARCH_CPU_X86_FAMILY 1
#    if SB_IS(32_BIT)
#      define ARCH_CPU_X86 1
#    elif SB_IS(64_BIT)
#      define ARCH_CPU_X86_64 1
#    endif  // SB_IS(32_BIT)
#  elif SB_IS(ARCH_PPC)
#    define ARCH_CPU_PPC_FAMILY 1
#  elif SB_IS(ARCH_MIPS)
#    define ARCH_CPU_MIPS_FAMILY 1
#    if SB_IS(BIG_ENDIAN)
#      define ARCH_CPU_MIPS 1
#    else  // SB_IS(BIG_ENDIAN)
#      define ARCH_CPU_MIPSEL 1
#    endif  // SB_IS(BIG_ENDIAN)
#  elif SB_IS(ARCH_ARM)
#    define ARCH_CPU_ARM_FAMILY 1
#    if SB_IS(BIG_ENDIAN)
#      define ARCH_CPU_ARM 1
#    else  // SB_IS(BIG_ENDIAN)
#      define ARCH_CPU_ARMEL 1
#    endif  // SB_IS(BIG_ENDIAN)
#  endif  // SB_IS(ARCH_X86)
#elif defined(_M_X64) || defined(__x86_64__)
#define ARCH_CPU_X86_FAMILY 1
#define ARCH_CPU_X86_64 1
#define ARCH_CPU_64_BITS 1
#define ARCH_CPU_LITTLE_ENDIAN 1
#elif defined(__LB_PS3__) || defined(__LB_WIIU__) || defined(__LB_XB360__)
#define ARCH_CPU_32_BITS 1
#define ARCH_CPU_BIG_ENDIAN 1
#define ARCH_CPU_PPC_FAMILY 1
#elif defined(_M_IX86) || defined(__i386__)
#define ARCH_CPU_X86_FAMILY 1
#define ARCH_CPU_X86 1
#define ARCH_CPU_32_BITS 1
#define ARCH_CPU_LITTLE_ENDIAN 1
#elif defined(__ARMEL__)
#define ARCH_CPU_ARM_FAMILY 1
#define ARCH_CPU_ARMEL 1
#define ARCH_CPU_32_BITS 1
#define ARCH_CPU_LITTLE_ENDIAN 1
#elif defined(__pnacl__)
#define ARCH_CPU_32_BITS 1
#elif defined(__MIPSEL__)
#define ARCH_CPU_MIPS_FAMILY 1
#define ARCH_CPU_MIPSEL 1
#define ARCH_CPU_32_BITS 1
#define ARCH_CPU_LITTLE_ENDIAN 1
#else
#error Please add support for your architecture in build/build_config.h
#endif

// Type detection for wchar_t.
#if defined(OS_STARBOARD)
#  if SB_IS(WCHAR_T_UTF16)
#    define WCHAR_T_IS_UTF16 1
#  elif SB_IS(WCHAR_T_UTF32)
#    define WCHAR_T_IS_UTF32 1
#  endif
#elif defined(OS_WIN) || \
    (defined(__LB_SHELL__) && \
        !(defined(__LB_LINUX__) || defined(__LB_ANDROID__)))
#define WCHAR_T_IS_UTF16 1
#elif defined(OS_POSIX) && defined(COMPILER_GCC) && \
    defined(__WCHAR_MAX__) && \
    (__WCHAR_MAX__ == 0x7fffffff || __WCHAR_MAX__ == 0xffffffff)
#define WCHAR_T_IS_UTF32 1
#elif defined(OS_POSIX) && defined(COMPILER_GCC) && \
    defined(__WCHAR_MAX__) && \
    (__WCHAR_MAX__ == 0x7fff || __WCHAR_MAX__ == 0xffff)
// On Posix, we'll detect short wchar_t, but projects aren't guaranteed to
// compile in this mode (in particular, Chrome doesn't). This is intended for
// other projects using base who manage their own dependencies and make sure
// short wchar works for them.
#define WCHAR_T_IS_UTF16 1
#else
#error Please add support for your compiler in build/build_config.h
#endif

#if defined(OS_STARBOARD)
#  if SB_IS(WCHAR_T_UNSIGNED)
#    define WCHAR_T_IS_UNSIGNED 1
#  elif SB_IS(WCHAR_T_SIGNED)
#    define WCHAR_T_IS_UNSIGNED 0
#  endif
#elif defined(__ARMEL__) && !defined(OS_IOS)
#define WCHAR_T_IS_UNSIGNED 1
#elif defined(__MIPSEL__)
#define WCHAR_T_IS_UNSIGNED 0
#endif

// TODO: Worry about these defines if/when we need to support Android.
#if defined(OS_ANDROID)
// The compiler thinks std::string::const_iterator and "const char*" are
// equivalent types.
#define STD_STRING_ITERATOR_IS_CHAR_POINTER
// The compiler thinks base::string16::const_iterator and "char16*" are
// equivalent types.
#define BASE_STRING16_ITERATOR_IS_CHAR16_POINTER
#endif

#endif  // BUILD_BUILD_CONFIG_H_
