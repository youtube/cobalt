// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_VIDEO_DECODER_CONFIG_H_
#define MEDIA_BASE_VIDEO_DECODER_CONFIG_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "media/base/media_export.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"

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

class MEDIA_EXPORT VideoDecoderConfig {
 public:
  VideoDecoderConfig(VideoCodec codec, const gfx::Size& coded_size,
                     const gfx::Rect& visible_rect,
                     const gfx::Size& natural_size,
                     int frame_rate_numerator, int frame_rate_denominator,
                     const uint8* extra_data, size_t extra_data_size);
  ~VideoDecoderConfig();

  VideoCodec codec() const;
  gfx::Size coded_size() const;
  gfx::Rect visible_rect() const;
  gfx::Size natural_size() const;
  int frame_rate_numerator() const;
  int frame_rate_denominator() const;
  uint8* extra_data() const;
  size_t extra_data_size() const;

 private:
  VideoCodec codec_;

  // Width and height of video frame immediately post-decode. Not all pixels
  // in this region are valid.
  gfx::Size coded_size_;

  // Region of |coded_size_| that is visible.
  gfx::Rect visible_rect_;

  // Natural width and height of the video, i.e. the visible dimensions
  // after aspect ratio is applied.
  gfx::Size natural_size_;

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
