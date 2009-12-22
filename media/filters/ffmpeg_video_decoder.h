// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef MEDIA_FILTERS_FFMPEG_VIDEO_DECODER_H_
#define MEDIA_FILTERS_FFMPEG_VIDEO_DECODER_H_

#include "media/filters/video_decoder_impl.h"

namespace media {

class FFmpegVideoDecodeEngine;
class FilterFactory;
class MediaFormat;

class FFmpegVideoDecoder : public VideoDecoderImpl {
 public:
  static FilterFactory* CreateFactory();
  static bool IsMediaFormatSupported(const MediaFormat& media_format);

  FFmpegVideoDecoder(FFmpegVideoDecodeEngine* engine);
  virtual ~FFmpegVideoDecoder();
};

}  // namespace media

#endif  // MEDIA_FILTERS_FFMPEG_VIDEO_DECODER_H_
