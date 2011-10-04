// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_VIDEO_DECODER_CONFIG_H_
#define MEDIA_BASE_VIDEO_DECODER_CONFIG_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "media/base/media_export.h"
#include "media/base/video_frame.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"

namespace media {

enum VideoCodec {
  kUnknownVideoCodec,
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
  // Constructs an uninitialized object. Clients should call Initialize() with
  // appropriate values before using.
  VideoDecoderConfig();

  // Constructs an initialized object. It is acceptable to pass in NULL for
  // |extra_data|, otherwise the memory is copied.
  VideoDecoderConfig(VideoCodec codec,
                     VideoFrame::Format format,
                     const gfx::Size& coded_size,
                     const gfx::Rect& visible_rect,
                     int frame_rate_numerator, int frame_rate_denominator,
                     const uint8* extra_data, size_t extra_data_size);

  ~VideoDecoderConfig();

  // Resets the internal state of this object.
  void Initialize(VideoCodec codec,
                  VideoFrame::Format format,
                  const gfx::Size& coded_size,
                  const gfx::Rect& visible_rect,
                  int frame_rate_numerator, int frame_rate_denominator,
                  const uint8* extra_data, size_t extra_data_size);

  // Returns true if this object has appropriate configuration values, false
  // otherwise.
  bool IsValidConfig() const;

  VideoCodec codec() const;

  // Video format used to determine YUV buffer sizes.
  VideoFrame::Format format() const;

  // Width and height of video frame immediately post-decode. Not all pixels
  // in this region are valid.
  gfx::Size coded_size() const;

  // Region of |coded_size_| that is visible.
  gfx::Rect visible_rect() const;

  // Frame rate in seconds expressed as a fraction.
  // TODO(scherkus): fairly certain decoders don't require frame rates.
  int frame_rate_numerator() const;
  int frame_rate_denominator() const;

  // Optional byte data required to initialize video decoders, such as H.264
  // AAVC data.
  uint8* extra_data() const;
  size_t extra_data_size() const;

 private:
  VideoCodec codec_;

  VideoFrame::Format format_;

  gfx::Size coded_size_;
  gfx::Rect visible_rect_;

  int frame_rate_numerator_;
  int frame_rate_denominator_;

  scoped_array<uint8> extra_data_;
  size_t extra_data_size_;

  DISALLOW_COPY_AND_ASSIGN(VideoDecoderConfig);
};

}  // namespace media

#endif  // MEDIA_BASE_VIDEO_DECODER_CONFIG_H_
