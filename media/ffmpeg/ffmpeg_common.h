// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FFMPEG_FFMPEG_COMMON_H_
#define MEDIA_FFMPEG_FFMPEG_COMMON_H_

// Used for FFmpeg error codes.
#include <cerrno>

#include "base/compiler_specific.h"
#include "base/memory/singleton.h"
#include "base/time.h"
#include "media/base/channel_layout.h"
#include "media/base/media_export.h"
#include "media/video/video_decode_engine.h"

// Include FFmpeg header files.
extern "C" {
// Temporarily disable possible loss of data warning.
// TODO(scherkus): fix and upstream the compiler warnings.
MSVC_PUSH_DISABLE_WARNING(4244);
#include <libavcodec/avcodec.h>
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

VideoCodec CodecIDToVideoCodec(CodecID codec_id);
CodecID VideoCodecToCodecID(VideoCodec video_codec);

// Converts FFmpeg's channel layout to chrome's ChannelLayout.  |channels| can
// be used when FFmpeg's channel layout is not informative in order to make a
// good guess about the plausible channel layout based on number of channels.
ChannelLayout ChannelLayoutToChromeChannelLayout(int64_t layout,
                                                 int channels);

// Calculates duration of one frame in the |stream| based on its frame rate.
base::TimeDelta GetFrameDuration(AVStream* stream);

// Get the timestamp of the next seek point after |timestamp|.
// Returns true if a valid seek point was found after |timestamp| and
// |seek_time| was set. Returns false if a seek point could not be
// found or the parameters are invalid.
MEDIA_EXPORT bool GetSeekTimeAfter(AVStream* stream,
                                   const base::TimeDelta& timestamp,
                                   base::TimeDelta* seek_time);

// Get the number of bytes required to play the stream over a specified
// time range. This is an estimate based on the available index data.
// Returns true if input time range was valid and |bytes|, |range_start|,
// and |range_end|, were set. Returns false if the range was invalid or we don't
// have enough index data to make an estimate.
//
// |bytes| - The number of bytes in the stream for the specified range.
// |range_start| - The start time for the range covered by |bytes|. This
//                 may be different than |start_time| if the index doesn't
//                 have data for that exact time. |range_start| <= |start_time|
// |range_end| - The end time for the range covered by |bytes|. This may be
//               different than |end_time| if the index doesn't have data for
//               that exact time. |range_end| >= |end_time|
MEDIA_EXPORT bool GetStreamByteCountOverRange(AVStream* stream,
                                              const base::TimeDelta& start_time,
                                              const base::TimeDelta& end_time,
                                              int64* bytes,
                                              base::TimeDelta* range_start,
                                              base::TimeDelta* range_end);

// Calculates the width and height of the video surface using the video's
// encoded dimensions and sample_aspect_ratio.
int GetSurfaceHeight(AVStream* stream);
int GetSurfaceWidth(AVStream* stream);

// Closes & destroys all AVStreams in the context and then closes &
// destroys the AVFormatContext.
void DestroyAVFormatContext(AVFormatContext* format_context);

}  // namespace media

#endif  // MEDIA_FFMPEG_FFMPEG_COMMON_H_
