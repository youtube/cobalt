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

// This class defines an interface for dispatching calls to the ffmpeg
// libraries.

#ifndef STARBOARD_SHARED_FFMPEG_FFMPEG_DISPATCH_H_
#define STARBOARD_SHARED_FFMPEG_FFMPEG_DISPATCH_H_

#include "starboard/common/mutex.h"
#include "starboard/types.h"

struct AVCodec;
struct AVCodecContext;
struct AVCodecParameters;
struct AVDictionary;
struct AVDictionaryEntry;
struct AVFormatContext;
struct AVFrame;
struct AVInputFormat;
struct AVIOContext;
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
#if LIBAVUTIL_VERSION_INT >= LIBAVUTIL_VERSION_52_8
  void (*av_frame_free)(AVFrame** frame);
#endif  // LIBAVUTIL_VERSION_INT >= LIBAVUTIL_VERSION_52_8
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
#if LIBAVUTIL_VERSION_INT >= LIBAVUTIL_VERSION_52_8
  void (*avcodec_free_context)(AVCodecContext** avctx);
#endif  // LIBAVUTIL_VERSION_INT >= LIBAVUTIL_VERSION_52_8
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
  void (*avcodec_align_dimensions2)(AVCodecContext* avctx,
                                    int* width,
                                    int* height,
                                    int linesize_align[]);

  unsigned (*avformat_version)(void);
  void (*av_register_all)(void);

  void (*av_free)(void* ptr);
  AVPacket* (*av_packet_alloc)(void);
  void (*av_packet_free)(AVPacket** pkt);
  void (*av_free_packet)(AVPacket* pkt);
  AVDictionaryEntry* (*av_dict_get)(const AVDictionary* m,
                                    const char* key,
                                    const AVDictionaryEntry* prev,
                                    int flags);
  // Note: |rnd| represents type enum AVRounding.
  int64_t (*av_rescale_rnd)(int64_t a, int64_t b, int64_t c, int rnd);
  int (*av_seek_frame)(AVFormatContext* s,
                       int stream_index,
                       int64_t timestamp,
                       int flags);
  int (*av_read_frame)(AVFormatContext* s, AVPacket* pkt);
  void (*av_packet_unref)(AVPacket* pkt);
  int (*avformat_open_input)(AVFormatContext** ps,
                             const char* filename,
                             AVInputFormat* fmt,
                             AVDictionary** options);
  void (*avformat_close_input)(AVFormatContext** s);
  AVFormatContext* (*avformat_alloc_context)(void);
  int (*avformat_find_stream_info)(AVFormatContext* ic, AVDictionary** options);
  AVIOContext* (*avio_alloc_context)(
      unsigned char* buffer,
      int buffer_size,
      int write_flag,
      void* opaque,
      int (*read_packet)(void* opaque, uint8_t* buf, int buf_size),
      int (*write_packet)(void* opaque, uint8_t* buf, int buf_size),
      int64_t (*seek)(void* opaque, int64_t offset, int whence));
  int (*avcodec_parameters_to_context)(AVCodecContext* codec,
                                       const AVCodecParameters* par);

  int specialization_version() const;

  // In Ffmpeg, the calls to avcodec_open2() and avcodec_close() are not
  // synchronized internally so it is the responsibility of its user to ensure
  // that these calls don't overlap.  The following functions acquires a lock
  // internally before calling avcodec_open2() and avcodec_close() to enforce
  // this.
  int OpenCodec(AVCodecContext* codec_context, const AVCodec* codec);
  void CloseCodec(AVCodecContext* codec_context);

  void FreeFrame(AVFrame** frame);
  void FreeContext(AVCodecContext** avctx);
};

}  // namespace ffmpeg
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_FFMPEG_FFMPEG_DISPATCH_H_
