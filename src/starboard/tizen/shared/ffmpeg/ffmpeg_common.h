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

#ifndef STARBOARD_TIZEN_SHARED_FFMPEG_FFMPEG_COMMON_H_
#define STARBOARD_TIZEN_SHARED_FFMPEG_FFMPEG_COMMON_H_

// Include FFmpeg header files.
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
}  // extern "C"

#include "starboard/shared/internal_only.h"

namespace starboard {
namespace shared {
namespace ffmpeg {

void InitializeFfmpeg();

// In Ffmpeg, the calls to avcodec_open2() and avcodec_close() are not
// synchronized internally so it is the responsibility of its user to ensure
// that these calls don't overlap.  The following functions acquires a lock
// internally before calling avcodec_open2() and avcodec_close() to enforce
// this.
int OpenCodec(AVCodecContext* codec_context, const AVCodec* codec);
void CloseCodec(AVCodecContext* codec_context);

}  // namespace ffmpeg
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_TIZEN_SHARED_FFMPEG_FFMPEG_COMMON_H_
