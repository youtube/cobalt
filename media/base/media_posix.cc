// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/media.h"

#include <string>

#include <dlfcn.h>

#include "base/file_path.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "third_party/ffmpeg/ffmpeg_stubs.h"

namespace tp_ffmpeg = third_party_ffmpeg;

namespace media {

namespace {

#if defined(OS_MACOSX)
#define DSO_NAME(MODULE, VERSION) ("lib" MODULE "." #VERSION ".dylib")
const FilePath::CharType sumo_name[] =
    FILE_PATH_LITERAL("libffmpegsumo.dylib");
#elif defined(OS_POSIX)
#define DSO_NAME(MODULE, VERSION) ("lib" MODULE ".so." #VERSION)
const FilePath::CharType sumo_name[] = FILE_PATH_LITERAL("libffmpegsumo.so");
#else
#error "Do not know how to construct DSO name for this OS."
#endif

// Retrieves the DSOName for the given key.
std::string GetDSOName(tp_ffmpeg::StubModules stub_key) {
  // TODO(ajwong): Remove this once mac is migrated. Either that, or have GYP
  // set a constant that we can switch implementations based off of.
  switch (stub_key) {
    case tp_ffmpeg::kModuleAvcodec52:
      return FILE_PATH_LITERAL(DSO_NAME("avcodec", 52));
    case tp_ffmpeg::kModuleAvformat52:
      return FILE_PATH_LITERAL(DSO_NAME("avformat", 52));
    case tp_ffmpeg::kModuleAvutil50:
      return FILE_PATH_LITERAL(DSO_NAME("avutil", 50));
    default:
      LOG(DFATAL) << "Invalid stub module requested: " << stub_key;
      return FILE_PATH_LITERAL("");
  }
}

}  // namespace

// Attempts to initialize the media library (loading DSOs, etc.).
// Returns true if everything was successfully initialized, false otherwise.
bool InitializeMediaLibrary(const FilePath& module_dir) {
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

  return tp_ffmpeg::InitializeStubs(paths);
}

}  // namespace media
