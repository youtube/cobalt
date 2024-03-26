##  Copyright (c) 2017 The WebM project authors. All Rights Reserved.
##
##  Use of this source code is governed by a BSD-style license
##  that can be found in the LICENSE file in the root of the source
##  tree. An additional intellectual property rights grant can be found
##  in the file PATENTS.  All contributing project authors may
##  be found in the AUTHORS file in the root of the source tree.
if (NOT LIBWEBM_BUILD_X86_MINGW_GCC_CMAKE_)
set(LIBWEBM_BUILD_X86_MINGW_GCC_CMAKE_ 1)

set(CMAKE_SYSTEM_PROCESSOR "x86")
set(CMAKE_SYSTEM_NAME "Windows")
set(CMAKE_C_COMPILER_ARG1 "-m32")
set(CMAKE_CXX_COMPILER_ARG1 "-m32")

if ("${CROSS}" STREQUAL "")
  set(CROSS i686-w64-mingw32-)
endif ()

set(CMAKE_C_COMPILER ${CROSS}gcc)
set(CMAKE_CXX_COMPILER ${CROSS}g++)

# Disable googletest CMake usage for mingw cross compiles.
set(LIBWEBM_DISABLE_GTEST_CMAKE 1)

endif ()  # LIBWEBM_BUILD_X86_MINGW_GCC_CMAKE_
