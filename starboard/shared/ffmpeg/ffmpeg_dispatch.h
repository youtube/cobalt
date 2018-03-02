// Copyright 2018 Google Inc. All Rights Reserved.
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

// This class defines an interface for dispatching calls to the ffmpeg
// libraries.

#ifndef STARBOARD_SHARED_FFMPEG_FFMPEG_DISPATCH_H_
#define STARBOARD_SHARED_FFMPEG_FFMPEG_DISPATCH_H_

#include "starboard/mutex.h"
#include "starboard/types.h"

struct AVCodec;
struct AVCodecContext;
struct AVDictionary;
struct AVFrame;
struct AVPacket;

namespace starboard {
namespace shared {
namespace ffmpeg {

class FFMPEGDispatch {
 public:
  FFMPEGDispatch();
  ~FFMPEGDispatch();

  static FFMPEGDispatch* GetInstance();

  // Register an ffmpeg decoder specialization for the given combination of
  // major versions of the libavcodec, libavformat, and libavutil libraries.
  // Returns true if the specialization did not exist yet, or if it matched
  // an already existing registration.
  static bool RegisterSpecialization(int specialization,
                                     int avcodec,
                                     int avformat,
                                     int avutil);

  bool is_valid() const;

  unsigned (*avutil_version)(void);
  void* (*av_malloc)(size_t size);
  void (*av_freep)(void* ptr);
  AVFrame* (*av_frame_alloc)(void);
  void (*av_frame_unref)(AVFrame* frame);
  int (*av_samples_get_buffer_size)(int* linesize,
                                    int nb_channels,
                                    int nb_samples,
                                    int sample_fmt,
                                    int align);
  int (*av_opt_set_int)(void* obj,
                        const char* name,
                        int64_t val,
                        int search_flags);
  int (*av_image_check_size)(unsigned int w,
                             unsigned int h,
                             int log_offset,
                             void* log_ctx);
  void* (*av_buffer_create)(uint8_t* data,
                            int size,
                            void (*free)(void* opaque, uint8_t* data),
                            void* opaque,
                            int flags);

  unsigned (*avcodec_version)(void);
  AVCodecContext* (*avcodec_alloc_context3)(const AVCodec* codec);
  AVCodec* (*avcodec_find_decoder)(int id);
  int (*avcodec_close)(AVCodecContext* avctx);
  int (*avcodec_open2)(AVCodecContext* avctx,
                       const AVCodec* codec,
                       AVDictionary** options);
  void (*av_init_packet)(AVPacket* pkt);
  int (*avcodec_decode_audio4)(AVCodecContext* avctx,
                               AVFrame* frame,
                               int* got_frame_ptr,
                               const AVPacket* avpkt);
  int (*avcodec_decode_video2)(AVCodecContext* avctx,
                               AVFrame* picture,
                               int* got_picture_ptr,
                               const AVPacket* avpkt);
  void (*avcodec_flush_buffers)(AVCodecContext* avctx);
  AVFrame* (*avcodec_alloc_frame)(void);
  void (*avcodec_get_frame_defaults)(AVFrame* frame);

  unsigned (*avformat_version)(void);
  void (*av_register_all)(void);

  int specialization_version() const;

  // In Ffmpeg, the calls to avcodec_open2() and avcodec_close() are not
  // synchronized internally so it is the responsibility of its user to ensure
  // that these calls don't overlap.  The following functions acquires a lock
  // internally before calling avcodec_open2() and avcodec_close() to enforce
  // this.
  int OpenCodec(AVCodecContext* codec_context, const AVCodec* codec);
  void CloseCodec(AVCodecContext* codec_context);
};

}  // namespace ffmpeg
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_FFMPEG_FFMPEG_DISPATCH_H_
