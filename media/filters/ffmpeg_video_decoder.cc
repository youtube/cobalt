// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "media/filters/ffmpeg_video_decoder.h"

#include "media/base/media_format.h"
#include "media/filters/ffmpeg_common.h"  // For kFFmpegVideo.
#include "media/filters/ffmpeg_video_decode_engine.h"

namespace media {

FFmpegVideoDecoder::FFmpegVideoDecoder(FFmpegVideoDecodeEngine* engine)
    : VideoDecoderImpl(engine) {
}

FFmpegVideoDecoder::~FFmpegVideoDecoder() {
}

// static
FilterFactory* FFmpegVideoDecoder::CreateFactory() {
  return new FilterFactoryImpl1<FFmpegVideoDecoder, FFmpegVideoDecodeEngine*>(
      new FFmpegVideoDecodeEngine());
}

// static
bool FFmpegVideoDecoder::IsMediaFormatSupported(const MediaFormat& format) {
  std::string mime_type;
  return format.GetAsString(MediaFormat::kMimeType, &mime_type) &&
      mime_type::kFFmpegVideo == mime_type;
}

}  // namespace media
