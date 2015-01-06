// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/media.h"

#include <string>

#include "base/file_path.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/stringize_macros.h"
#include "media/ffmpeg/ffmpeg_common.h"

#if !defined(USE_SYSTEM_FFMPEG)
#include "third_party/ffmpeg/ffmpeg_stubs.h"

using third_party_ffmpeg::kNumStubModules;
using third_party_ffmpeg::kModuleFfmpegsumo;
using third_party_ffmpeg::InitializeStubs;
using third_party_ffmpeg::StubPathMap;
#endif  // !defined(USE_SYSTEM_FFMPEG)

namespace media {

// Handy to prevent shooting ourselves in the foot with macro wizardry.
#if !defined(LIBAVCODEC_VERSION_MAJOR) || \
    !defined(LIBAVFORMAT_VERSION_MAJOR) || \
    !defined(LIBAVUTIL_VERSION_MAJOR)
#error FFmpeg headers not included!
#endif

#define AVCODEC_VERSION STRINGIZE(LIBAVCODEC_VERSION_MAJOR)
#define AVFORMAT_VERSION STRINGIZE(LIBAVFORMAT_VERSION_MAJOR)
#define AVUTIL_VERSION STRINGIZE(LIBAVUTIL_VERSION_MAJOR)

#if defined(OS_MACOSX)
// TODO(evan): should be using .so like ffmepgsumo here.
#define DSO_NAME(MODULE, VERSION) ("lib" MODULE "." VERSION ".dylib")
static const FilePath::CharType kSumoLib[] =
    FILE_PATH_LITERAL("ffmpegsumo.so");
#elif defined(OS_POSIX)
#define DSO_NAME(MODULE, VERSION) ("lib" MODULE ".so." VERSION)
static const FilePath::CharType kSumoLib[] =
    FILE_PATH_LITERAL("libffmpegsumo.so");
#else
#error "Do not know how to construct DSO name for this OS."
#endif

// Use a global to indicate whether the library has been initialized or not.  We
// rely on function level static initialization in InitializeMediaLibrary() to
// guarantee this is only set once in a thread safe manner.
static bool g_media_library_is_initialized = false;

static bool InitializeMediaLibraryInternal(const FilePath& module_dir) {
  DCHECK(!g_media_library_is_initialized);

#if defined(USE_SYSTEM_FFMPEG)
  // No initialization is necessary when using system ffmpeg,
  // we just link directly with system ffmpeg libraries.
  g_media_library_is_initialized = true;
#else
  StubPathMap paths;

  // First try to initialize with Chrome's sumo library.
  DCHECK_EQ(kNumStubModules, 1);
  paths[kModuleFfmpegsumo].push_back(module_dir.Append(kSumoLib).value());

  // If that fails, see if any system libraries are available.
  paths[kModuleFfmpegsumo].push_back(module_dir.Append(
      FILE_PATH_LITERAL(DSO_NAME("avutil", AVUTIL_VERSION))).value());
  paths[kModuleFfmpegsumo].push_back(module_dir.Append(
      FILE_PATH_LITERAL(DSO_NAME("avcodec", AVCODEC_VERSION))).value());
  paths[kModuleFfmpegsumo].push_back(module_dir.Append(
      FILE_PATH_LITERAL(DSO_NAME("avformat", AVFORMAT_VERSION))).value());

  g_media_library_is_initialized = InitializeStubs(paths);
#endif  // !defined(USE_SYSTEM_FFMPEG)
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
