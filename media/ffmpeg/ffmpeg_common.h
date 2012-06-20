// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FFMPEG_FFMPEG_COMMON_H_
#define MEDIA_FFMPEG_FFMPEG_COMMON_H_

// Used for FFmpeg error codes.
#include <cerrno>

#include "base/compiler_specific.h"
#include "base/time.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/channel_layout.h"
#include "media/base/media_export.h"
#include "media/base/video_frame.h"
#include "media/base/video_decoder_config.h"

// Include FFmpeg header files.
extern "C" {
// Temporarily disable possible loss of data warning.
// TODO(scherkus): fix and upstream the compiler warnings.
MSVC_PUSH_DISABLE_WARNING(4244);
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libavformat/url.h>
#include <libavutil/avutil.h>
#include <libavutil/mathematics.h>
#include <libavutil/log.h>
MSVC_POP_WARNING();
}  // extern "C"

namespace media {

class AudioDecoderConfig;
class VideoDecoderConfig;

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

// Converts an int64 timestamp in |time_base| units to a base::TimeDelta.
// For example if |timestamp| equals 11025 and |time_base| equals {1, 44100}
// then the return value will be a base::TimeDelta for 0.25 seconds since that
// is how much time 11025/44100ths of a second represents.
MEDIA_EXPORT base::TimeDelta ConvertFromTimeBase(const AVRational& time_base,
                                                 int64 timestamp);

// Converts a base::TimeDelta into an int64 timestamp in |time_base| units.
// For example if |timestamp| is 0.5 seconds and |time_base| is {1, 44100}, then
// the return value will be 22050 since that is how many 1/44100ths of a second
// represent 0.5 seconds.
MEDIA_EXPORT int64 ConvertToTimeBase(const AVRational& time_base,
                                     const base::TimeDelta& timestamp);

void AVCodecContextToAudioDecoderConfig(
    const AVCodecContext* codec_context,
    AudioDecoderConfig* config);
void AudioDecoderConfigToAVCodecContext(
    const AudioDecoderConfig& config,
    AVCodecContext* codec_context);

void AVStreamToVideoDecoderConfig(
    const AVStream* stream,
    VideoDecoderConfig* config);
void VideoDecoderConfigToAVCodecContext(
    const VideoDecoderConfig& config,
    AVCodecContext* codec_context);

// Converts FFmpeg's channel layout to chrome's ChannelLayout.  |channels| can
// be used when FFmpeg's channel layout is not informative in order to make a
// good guess about the plausible channel layout based on number of channels.
ChannelLayout ChannelLayoutToChromeChannelLayout(int64_t layout,
                                                 int channels);

// Converts FFmpeg's pixel formats to its corresponding supported video format.
VideoFrame::Format PixelFormatToVideoFormat(PixelFormat pixel_format);

// Converts video formats to its corresponding FFmpeg's pixel formats.
PixelFormat VideoFormatToPixelFormat(VideoFrame::Format video_format);

// Converts an FFmpeg video codec ID into its corresponding supported codec id.
VideoCodec CodecIDToVideoCodec(CodecID codec_id);

// Converts an FFmpeg audio codec ID into its corresponding supported codec id.
AudioCodec CodecIDToAudioCodec(CodecID codec_id);

// Calculates the duration of one frame based on the frame rate specified by
// |config|.
base::TimeDelta GetFrameDuration(const VideoDecoderConfig& config);

// Closes & destroys all AVStreams in the context and then closes &
// destroys the AVFormatContext.
void DestroyAVFormatContext(AVFormatContext* format_context);

}  // namespace media

#endif  // MEDIA_FFMPEG_FFMPEG_COMMON_H_
