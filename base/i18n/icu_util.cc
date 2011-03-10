// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/i18n/icu_util.h"

#include "build/build_config.h"

#if defined(OS_WIN)
#include <windows.h>
#endif

#include <string>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "base/sys_string_conversions.h"
#include "unicode/putil.h"
#include "unicode/udata.h"

#if defined(OS_MACOSX)
#include "base/mac/foundation_util.h"
#endif

#define ICU_UTIL_DATA_FILE   0
#define ICU_UTIL_DATA_SHARED 1
#define ICU_UTIL_DATA_STATIC 2

#ifndef ICU_UTIL_DATA_IMPL

#if defined(OS_WIN)
#define ICU_UTIL_DATA_IMPL ICU_UTIL_DATA_SHARED
#else
#define ICU_UTIL_DATA_IMPL ICU_UTIL_DATA_STATIC
#endif

#endif  // ICU_UTIL_DATA_IMPL

#if ICU_UTIL_DATA_IMPL == ICU_UTIL_DATA_FILE
#define ICU_UTIL_DATA_FILE_NAME "icudt" U_ICU_VERSION_SHORT "l.dat"
#elif ICU_UTIL_DATA_IMPL == ICU_UTIL_DATA_SHARED
#define ICU_UTIL_DATA_SYMBOL "icudt" U_ICU_VERSION_SHORT "_dat"
#if defined(OS_WIN)
#define ICU_UTIL_DATA_SHARED_MODULE_NAME "icudt.dll"
#endif
#endif

namespace icu_util {

bool Initialize() {
#ifndef NDEBUG
  // Assert that we are not called more than once.  Even though calling this
  // function isn't harmful (ICU can handle it), being called twice probably
  // indicates a programming error.
  static bool called_once = false;
  DCHECK(!called_once);
  called_once = true;
#endif

#if (ICU_UTIL_DATA_IMPL == ICU_UTIL_DATA_SHARED)
  // We expect to find the ICU data module alongside the current module.
  FilePath data_path;
  PathService::Get(base::DIR_MODULE, &data_path);
  data_path = data_path.AppendASCII(ICU_UTIL_DATA_SHARED_MODULE_NAME);

  HMODULE module = LoadLibrary(data_path.value().c_str());
  if (!module) {
    LOG(ERROR) << "Failed to load " << ICU_UTIL_DATA_SHARED_MODULE_NAME;
    return false;
  }

  FARPROC addr = GetProcAddress(module, ICU_UTIL_DATA_SYMBOL);
  if (!addr) {
    LOG(ERROR) << ICU_UTIL_DATA_SYMBOL << ": not found in "
               << ICU_UTIL_DATA_SHARED_MODULE_NAME;
    return false;
  }

  UErrorCode err = U_ZERO_ERROR;
  udata_setCommonData(reinterpret_cast<void*>(addr), &err);
  return err == U_ZERO_ERROR;
#elif (ICU_UTIL_DATA_IMPL == ICU_UTIL_DATA_STATIC)
  // Mac/Linux bundle the ICU data in.
  return true;
#elif (ICU_UTIL_DATA_IMPL == ICU_UTIL_DATA_FILE)
#if !defined(OS_MACOSX)
  // For now, expect the data file to be alongside the executable.
  // This is sufficient while we work on unit tests, but will eventually
  // likely live in a data directory.
  FilePath data_path;
  bool path_ok = PathService::Get(base::DIR_EXE, &data_path);
  DCHECK(path_ok);
  u_setDataDirectory(data_path.value().c_str());
  // Only look for the packaged data file;
  // the default behavior is to look for individual files.
  UErrorCode err = U_ZERO_ERROR;
  udata_setFileAccess(UDATA_ONLY_PACKAGES, &err);
  return err == U_ZERO_ERROR;
#else
  // If the ICU data directory is set, ICU won't actually load the data until
  // it is needed.  This can fail if the process is sandboxed at that time.
  // Instead, Mac maps the file in and hands off the data so the sandbox won't
  // cause any problems.

  // Chrome doesn't normally shut down ICU, so the mapped data shouldn't ever
  // be released.
  static file_util::MemoryMappedFile mapped_file;
  if (!mapped_file.IsValid()) {
    // Assume it is in the MainBundle's Resources directory.
    FilePath data_path =
      base::mac::PathForMainAppBundleResource(CFSTR(ICU_UTIL_DATA_FILE_NAME));
    if (data_path.empty()) {
      LOG(ERROR) << ICU_UTIL_DATA_FILE_NAME << " not found in bundle";
      return false;
    }
    if (!mapped_file.Initialize(data_path)) {
      LOG(ERROR) << "Couldn't mmap " << data_path.value();
      return false;
    }
  }
  UErrorCode err = U_ZERO_ERROR;
  udata_setCommonData(const_cast<uint8*>(mapped_file.data()), &err);
  return err == U_ZERO_ERROR;
#endif  // OS check
#endif
}

}  // namespace icu_util
