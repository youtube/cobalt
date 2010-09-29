// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/media.h"

#include <string>

#include <dlfcn.h>

#include "base/file_path.h"
#include "base/logging.h"
#include "base/native_library.h"
#include "base/path_service.h"
#include "media/ffmpeg/ffmpeg_common.h"
#include "third_party/ffmpeg/ffmpeg_stubs.h"
#if defined(OS_LINUX)
// OpenMAX IL stub is generated only on Linux.
#include "third_party/openmax/il_stubs.h"
#endif

namespace tp_ffmpeg = third_party_ffmpeg;

namespace media {

namespace {

// Handy to prevent shooting ourselves in the foot with macro wizardry.
#if !defined(LIBAVCODEC_VERSION_MAJOR) || \
    !defined(LIBAVFORMAT_VERSION_MAJOR) || \
    !defined(LIBAVUTIL_VERSION_MAJOR)
#error FFmpeg headers not included!
#endif

#define STRINGIZE(x) #x
#define STRINGIZE_MACRO(x) STRINGIZE(x)

#define AVCODEC_VERSION STRINGIZE_MACRO(LIBAVCODEC_VERSION_MAJOR)
#define AVFORMAT_VERSION STRINGIZE_MACRO(LIBAVFORMAT_VERSION_MAJOR)
#define AVUTIL_VERSION STRINGIZE_MACRO(LIBAVUTIL_VERSION_MAJOR)

#if defined(OS_MACOSX)
#define DSO_NAME(MODULE, VERSION) ("lib" MODULE "." VERSION ".dylib")
const FilePath::CharType sumo_name[] =
    FILE_PATH_LITERAL("libffmpegsumo.dylib");
#elif defined(OS_POSIX)
#define DSO_NAME(MODULE, VERSION) ("lib" MODULE ".so." VERSION)
const FilePath::CharType sumo_name[] = FILE_PATH_LITERAL("libffmpegsumo.so");
#else
#error "Do not know how to construct DSO name for this OS."
#endif
const FilePath::CharType openmax_name[] = FILE_PATH_LITERAL("libOmxCore.so");

// Retrieves the DSOName for the given key.
std::string GetDSOName(tp_ffmpeg::StubModules stub_key) {
  // TODO(ajwong): Remove this once mac is migrated. Either that, or have GYP
  // set a constant that we can switch implementations based off of.
  switch (stub_key) {
    case tp_ffmpeg::kModuleAvcodec52:
      return FILE_PATH_LITERAL(DSO_NAME("avcodec", AVCODEC_VERSION));
    case tp_ffmpeg::kModuleAvformat52:
      return FILE_PATH_LITERAL(DSO_NAME("avformat", AVFORMAT_VERSION));
    case tp_ffmpeg::kModuleAvutil50:
      return FILE_PATH_LITERAL(DSO_NAME("avutil", AVUTIL_VERSION));
    default:
      LOG(DFATAL) << "Invalid stub module requested: " << stub_key;
      return FILE_PATH_LITERAL("");
  }
}

}  // namespace

// Address of vpx_codec_vp8_cx_algo.
static void* vp8_cx_algo_address = NULL;

// Address of vpx_codec_vp8_dx_algo.
static void* vp8_dx_algo_address = NULL;

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

  bool ret = tp_ffmpeg::InitializeStubs(paths);

  // TODO(hclam): This is temporary to obtain address of
  // vpx_codec_vp8_cx_algo. This should be removed once libvpx has a
  // getter method for it.
  base::NativeLibrary sumo_lib =
      base::LoadNativeLibrary(module_dir.Append(sumo_name));
  if (sumo_lib) {
    vp8_cx_algo_address = base::GetFunctionPointerFromNativeLibrary(
        sumo_lib, "vpx_codec_vp8_cx_algo");
    vp8_dx_algo_address = base::GetFunctionPointerFromNativeLibrary(
        sumo_lib, "vpx_codec_vp8_dx_algo");
  }
  return ret;
}

#if defined(OS_LINUX)
namespace tp_openmax = third_party_openmax;
bool InitializeOpenMaxLibrary(const FilePath& module_dir) {
  // TODO(ajwong): We need error resolution.
  tp_openmax::StubPathMap paths;
  for (int i = 0; i < static_cast<int>(tp_openmax::kNumStubModules); ++i) {
    tp_openmax::StubModules module = static_cast<tp_openmax::StubModules>(i);

    // Add the OpenMAX library first so it takes precedence.
    paths[module].push_back(module_dir.Append(openmax_name).value());
  }

  bool result = tp_openmax::InitializeStubs(paths);
  if (!result) {
    LOG(FATAL) << "Cannot load " << openmax_name << "."
               << " Make sure it exists for OpenMAX.";
  }
  return result;
}
#else
bool InitializeOpenMaxLibrary(const FilePath& module_dir) {
  NOTIMPLEMENTED() << "OpenMAX is only used in Linux.";
  return false;
}
#endif

void* GetVp8CxAlgoAddress() {
  return vp8_cx_algo_address;
}

void* GetVp8DxAlgoAddress() {
  return vp8_dx_algo_address;
}

}  // namespace media
