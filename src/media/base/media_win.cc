// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/media.h"

#include <windows.h>
#if defined(_WIN32_WINNT_WIN8)
// The Windows 8 SDK defines FACILITY_VISUALCPP in winerror.h.
#undef FACILITY_VISUALCPP
#endif
#include <delayimp.h>

#include "base/file_path.h"
#include "base/logging.h"
#include "base/native_library.h"
#include "base/path_service.h"

#pragma comment(lib, "delayimp.lib")

namespace media {

// FFmpeg library name.
static const char* kFFmpegDLL = "ffmpegsumo.dll";

// Use a global to indicate whether the library has been initialized or not.  We
// rely on function level static initialization in InitializeMediaLibrary() to
// guarantee this is only set once in a thread safe manner.
static bool g_media_library_is_initialized = false;

static bool InitializeMediaLibraryInternal(const FilePath& base_path) {
  DCHECK(!g_media_library_is_initialized);

  // LoadLibraryEx(..., LOAD_WITH_ALTERED_SEARCH_PATH) cannot handle
  // relative path.
  if (!base_path.IsAbsolute())
    return false;

  // Use alternate DLL search path so we don't load dependencies from the
  // system path.  Refer to http://crbug.com/35857
  HMODULE lib = ::LoadLibraryEx(
      base_path.AppendASCII(kFFmpegDLL).value().c_str(), NULL,
      LOAD_WITH_ALTERED_SEARCH_PATH);

  // Check that we loaded the library successfully.
  g_media_library_is_initialized = (lib != NULL);
  return g_media_library_is_initialized;
}

bool InitializeMediaLibrary(const FilePath& base_path) {
  static const bool kMediaLibraryInitialized =
      InitializeMediaLibraryInternal(base_path);
  DCHECK_EQ(kMediaLibraryInitialized, g_media_library_is_initialized);
  return kMediaLibraryInitialized;
}

void InitializeMediaLibraryForTesting() {
  FilePath file_path;
  CHECK(PathService::Get(base::DIR_EXE, &file_path));
  CHECK(InitializeMediaLibrary(file_path));
}

bool IsMediaLibraryInitialized() {
  return g_media_library_is_initialized;
}

}  // namespace media
