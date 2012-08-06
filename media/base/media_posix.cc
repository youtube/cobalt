// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/media.h"

#include <string>

#include <dlfcn.h>

#include "base/file_path.h"
#include "base/logging.h"
#include "base/native_library.h"
#include "base/path_service.h"
#include "base/stringize_macros.h"
#include "media/ffmpeg/ffmpeg_common.h"
#include "third_party/ffmpeg/ffmpeg_stubs.h"

namespace tp_ffmpeg = third_party_ffmpeg;

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
static const FilePath::CharType sumo_name[] =
    FILE_PATH_LITERAL("ffmpegsumo.so");
#elif defined(OS_POSIX)
#define DSO_NAME(MODULE, VERSION) ("lib" MODULE ".so." VERSION)
static const FilePath::CharType sumo_name[] =
    FILE_PATH_LITERAL("libffmpegsumo.so");
#else
#error "Do not know how to construct DSO name for this OS."
#endif
static const FilePath::CharType openmax_name[] =
    FILE_PATH_LITERAL("libOmxCore.so");

// Retrieves the DSOName for the given key.
static std::string GetDSOName(tp_ffmpeg::StubModules stub_key) {
  // TODO(ajwong): Remove this once mac is migrated. Either that, or have GYP
  // set a constant that we can switch implementations based off of.
  switch (stub_key) {
    case tp_ffmpeg::kModuleAvcodec54:
      return FILE_PATH_LITERAL(DSO_NAME("avcodec", AVCODEC_VERSION));
    case tp_ffmpeg::kModuleAvformat54:
      return FILE_PATH_LITERAL(DSO_NAME("avformat", AVFORMAT_VERSION));
    case tp_ffmpeg::kModuleAvutil51:
      return FILE_PATH_LITERAL(DSO_NAME("avutil", AVUTIL_VERSION));
    default:
      LOG(DFATAL) << "Invalid stub module requested: " << stub_key;
      return FILE_PATH_LITERAL("");
  }
}

static bool g_media_library_is_initialized = false;

// Attempts to initialize the media library (loading DSOs, etc.).
// Returns true if everything was successfully initialized, false otherwise.
bool InitializeMediaLibrary(const FilePath& module_dir) {
  if (g_media_library_is_initialized)
    return true;

  // TODO(ajwong): We need error resolution.
  tp_ffmpeg::StubPathMap paths;
  for (int i = 0; i < static_cast<int>(tp_ffmpeg::kNumStubModules); ++i) {
    tp_ffmpeg::StubModules module = static_cast<tp_ffmpeg::StubModules>(i);

    // Add the sumo library first so it takes precedence.
    paths[module].push_back(module_dir.Append(sumo_name).value());

    // Add the more specific FFmpeg library name.
    FilePath path = module_dir.Append(GetDSOName(module));
    paths[module].push_back(path.value());
  }

  g_media_library_is_initialized = tp_ffmpeg::InitializeStubs(paths);
  return g_media_library_is_initialized;
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
