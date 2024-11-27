// Copyright 2018 The Cobalt Authors. All Rights Reserved.
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

// This file implements the FFMPEGDispatch interface with dynamic loading of
// the libraries.

#include "starboard/shared/ffmpeg/ffmpeg_dispatch.h"

#include <pthread.h>

#include "starboard/common/log.h"

namespace starboard {
namespace shared {
namespace ffmpeg {

namespace {
pthread_mutex_t g_codec_mutex = PTHREAD_MUTEX_INITIALIZER;
}  // namespace

int FFMPEGDispatch::OpenCodec(AVCodecContext* codec_context,
                              const AVCodec* codec) {
  pthread_mutex_lock(&g_codec_mutex);
  int result = avcodec_open2(codec_context, codec, NULL);
  pthread_mutex_unlock(&g_codec_mutex);
  return result;
}

void FFMPEGDispatch::CloseCodec(AVCodecContext* codec_context) {
  pthread_mutex_lock(&g_codec_mutex);
  avcodec_close(codec_context);
  pthread_mutex_unlock(&g_codec_mutex);
}

void FFMPEGDispatch::FreeFrame(AVFrame** frame) {
  if (avcodec_version() > kAVCodecSupportsAvFrameAlloc) {
    av_frame_free(frame);
  } else {
    av_freep(frame);
  }
}

void FFMPEGDispatch::FreeContext(AVCodecContext** avctx) {
  if (avcodec_version() > kAVCodecSupportsAvcodecFreeContext) {
    avcodec_free_context(avctx);
  } else {
    av_freep(avctx);
  }
}

}  // namespace ffmpeg
}  // namespace shared
}  // namespace starboard
