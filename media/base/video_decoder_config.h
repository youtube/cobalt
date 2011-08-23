// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_VIDEO_DECODER_CONFIG_H_
#define MEDIA_BASE_VIDEO_DECODER_CONFIG_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"

namespace media {

enum VideoCodec {
  kUnknown,
  kCodecH264,
  kCodecVC1,
  kCodecMPEG2,
  kCodecMPEG4,
  kCodecTheora,
  kCodecVP8,

  // DO NOT ADD RANDOM VIDEO CODECS!
  //
  // The only acceptable time to add a new codec is if there is production code
  // that uses said codec in the same CL.
};

class VideoDecoderConfig {
 public:
  VideoDecoderConfig(VideoCodec codec, int width, int height,
                     int surface_width, int surface_height,
                     int frame_rate_numerator, int frame_rate_denominator,
                     const uint8* extra_data, size_t extra_data_size);
  ~VideoDecoderConfig();

  VideoCodec codec() const;
  int width() const;
  int height() const;
  int surface_width() const;
  int surface_height() const;
  int frame_rate_numerator() const;
  int frame_rate_denominator() const;
  uint8* extra_data() const;
  size_t extra_data_size() const;

 private:
  VideoCodec codec_;

  // Container's concept of width and height of this video.
  int width_;
  int height_;

  // Width and height of the display surface for this video.
  int surface_width_;
  int surface_height_;

  // Frame rate in seconds expressed as a fraction.
  // TODO(scherkus): fairly certain decoders don't require frame rates.
  int frame_rate_numerator_;
  int frame_rate_denominator_;

  // Optional byte data required to initialize video decoders.
  scoped_array<uint8> extra_data_;
  size_t extra_data_size_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(VideoDecoderConfig);
};

}  // namespace media

#endif  // MEDIA_BASE_VIDEO_DECODER_CONFIG_H_
