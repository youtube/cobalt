// Copyright 2016 Google Inc. All Rights Reserved.
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

#ifndef STARBOARD_SHARED_FFMPEG_FFMPEG_COMMON_H_
#define STARBOARD_SHARED_FFMPEG_FFMPEG_COMMON_H_

// Include FFmpeg header files.
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
}  // extern "C"

#include "starboard/shared/internal_only.h"

#if !defined(LIBAVUTIL_VERSION_MAJOR)
#error "LIBAVUTIL_VERSION_MAJOR not defined"
#endif  // !defined(LIBAVUTIL_VERSION_MAJOR)

#if LIBAVUTIL_VERSION_MAJOR > 52
#define PIX_FMT_NONE AV_PIX_FMT_NONE
#define PIX_FMT_YUV420P AV_PIX_FMT_YUV420P
#define PIX_FMT_YUVJ420P AV_PIX_FMT_YUVJ420P
#endif  // LIBAVUTIL_VERSION_MAJOR > 52

#if !defined(LIBAVCODEC_VERSION_MAJOR)
#error "LIBAVCODEC_VERSION_MAJOR not defined"
#endif  // !defined(LIBAVCODEC_VERSION_MAJOR)

#if !defined(LIBAVCODEC_VERSION_MICRO)
#error "LIBAVCODEC_VERSION_MICRO not defined"
#endif  // !defined(LIBAVCODEC_VERSION_MICRO)

#if LIBAVCODEC_VERSION_MICRO >= 100
#define LIBAVCODEC_LIBRARY_IS_FFMPEG 1
#else
#define LIBAVCODEC_LIBRARY_IS_FFMPEG 0
#endif  // LIBAVCODEC_VERSION_MICRO >= 100

// Use the major version number of libavcodec plus a single digit distinguishing
// between ffmpeg and libav as the template parameter for the
// explicit specialization of the audio and video decoder classes.
#define FFMPEG ((LIBAVCODEC_VERSION_MAJOR * 10) + LIBAVCODEC_LIBRARY_IS_FFMPEG)

#endif  // STARBOARD_SHARED_FFMPEG_FFMPEG_COMMON_H_
