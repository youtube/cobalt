// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FFMPEG_FFMPEG_COMMON_H_
#define MEDIA_FFMPEG_FFMPEG_COMMON_H_

// Used for FFmpeg error codes.
#include <cerrno>

#include "base/compiler_specific.h"
#include "base/singleton.h"

// Include FFmpeg header files.
extern "C" {
// Temporarily disable possible loss of data warning.
// TODO(scherkus): fix and upstream the compiler warnings.
MSVC_PUSH_DISABLE_WARNING(4244);
#include <libavcodec/avcodec.h>
#include <libavcore/samplefmt.h>
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libavutil/avutil.h>
#include <libavutil/log.h>
MSVC_POP_WARNING();
}  // extern "C"

namespace media {

// Wraps FFmpeg's av_free() in a class that can be passed as a template argument
// to scoped_ptr_malloc.
class ScopedPtrAVFree {
 public:
  inline void operator()(void* x) const {
    av_free(x);
  }
};

// This assumes that the AVPacket being captured was allocated outside of
// FFmpeg via the new operator.  Do not use this with AVPacket instances that
// are allocated via malloc() or av_malloc().
class ScopedPtrAVFreePacket {
 public:
  inline void operator()(void* x) const {
    AVPacket* packet = static_cast<AVPacket*>(x);
    av_free_packet(packet);
    delete packet;
  }
};

}  // namespace media

#endif  // MEDIA_FFMPEG_FFMPEG_COMMON_H_
