##  Copyright (c) 2016 The WebM project authors. All Rights Reserved.
##
##  Use of this source code is governed by a BSD-style license
##  that can be found in the LICENSE file in the root of the source
##  tree. An additional intellectual property rights grant can be found
##  in the file PATENTS.  All contributing project authors may
##  be found in the AUTHORS file in the root of the source tree.

include(CheckCXXCompilerFlag)

# String used to cache failed CXX flags.
set(LIBWEBM_FAILED_CXX_FLAGS)

# Checks C++ compiler for support of $cxx_flag. Adds $cxx_flag to
# $CMAKE_CXX_FLAGS when the compile test passes. Caches $c_flag in
# $LIBWEBM_FAILED_CXX_FLAGS when the test fails.
function (add_cxx_flag_if_supported cxx_flag)
  unset(CXX_FLAG_FOUND CACHE)
  string(FIND "${CMAKE_CXX_FLAGS}" "${cxx_flag}" CXX_FLAG_FOUND)
  unset(CXX_FLAG_FAILED CACHE)
  string(FIND "${LIBWEBM_FAILED_CXX_FLAGS}" "${cxx_flag}" CXX_FLAG_FAILED)

  if (${CXX_FLAG_FOUND} EQUAL -1 AND ${CXX_FLAG_FAILED} EQUAL -1)
    unset(CXX_FLAG_SUPPORTED CACHE)
    message("Checking CXX compiler flag support for: " ${cxx_flag})
    check_cxx_compiler_flag("${cxx_flag}" CXX_FLAG_SUPPORTED)
    if (CXX_FLAG_SUPPORTED)
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${cxx_flag}" CACHE STRING ""
          FORCE)
    else ()
      set(LIBWEBM_FAILED_CXX_FLAGS "${LIBWEBM_FAILED_CXX_FLAGS} ${cxx_flag}"
          CACHE STRING "" FORCE)
    endif ()
  endif ()
endfunction ()

# Checks CXX compiler for support of $cxx_flag and terminates generation when
# support is not present.
function (require_cxx_flag cxx_flag)
  unset(CXX_FLAG_FOUND CACHE)
  string(FIND "${CMAKE_CXX_FLAGS}" "${cxx_flag}" CXX_FLAG_FOUND)

  if (${CXX_FLAG_FOUND} EQUAL -1)
    unset(LIBWEBM_HAVE_CXX_FLAG CACHE)
    message("Checking CXX compiler flag support for: " ${cxx_flag})
    check_cxx_compiler_flag("${cxx_flag}" LIBWEBM_HAVE_CXX_FLAG)
    if (NOT LIBWEBM_HAVE_CXX_FLAG)
      message(FATAL_ERROR
              "${PROJECT_NAME} requires support for CXX flag: ${cxx_flag}.")
    endif ()
    set(CMAKE_CXX_FLAGS "${cxx_flag} ${CMAKE_CXX_FLAGS}" CACHE STRING "" FORCE)
  endif ()
endfunction ()

# Checks only non-MSVC targets for support of $cxx_flag.
function (require_cxx_flag_nomsvc cxx_flag)
  if (NOT MSVC)
    require_cxx_flag(${cxx_flag})
  endif ()
endfunction ()

# Adds $preproc_def to CXX compiler command line (as -D$preproc_def) if not
# already present.
function (add_cxx_preproc_definition preproc_def)
  unset(PREPROC_DEF_FOUND CACHE)
  string(FIND "${CMAKE_CXX_FLAGS}" "${preproc_def}" PREPROC_DEF_FOUND)

  if (${PREPROC_DEF_FOUND} EQUAL -1)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D${preproc_def}" CACHE STRING ""
        FORCE)
  endif ()
endfunction ()
