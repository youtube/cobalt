// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/video_decoder_config.h"

#include "base/logging.h"

namespace media {

VideoDecoderConfig::VideoDecoderConfig(VideoCodec codec,
                                       const gfx::Size& coded_size,
                                       const gfx::Rect& visible_rect,
                                       const gfx::Size& natural_size,
                                       int frame_rate_numerator,
                                       int frame_rate_denominator,
                                       const uint8* extra_data,
                                       size_t extra_data_size)
    : codec_(codec),
      coded_size_(coded_size),
      visible_rect_(visible_rect),
      natural_size_(natural_size),
      frame_rate_numerator_(frame_rate_numerator),
      frame_rate_denominator_(frame_rate_denominator),
      extra_data_size_(extra_data_size) {
  CHECK(extra_data_size_ == 0 || extra_data);
  if (extra_data_size_ > 0) {
    extra_data_.reset(new uint8[extra_data_size_]);
    memcpy(extra_data_.get(), extra_data, extra_data_size_);
  }
}

VideoDecoderConfig::~VideoDecoderConfig() {}

VideoCodec VideoDecoderConfig::codec() const {
  return codec_;
}

gfx::Size VideoDecoderConfig::coded_size() const {
  return coded_size_;
}

gfx::Rect VideoDecoderConfig::visible_rect() const {
  return visible_rect_;
}

gfx::Size VideoDecoderConfig::natural_size() const {
  return natural_size_;
}

int VideoDecoderConfig::frame_rate_numerator() const {
  return frame_rate_numerator_;
}

int VideoDecoderConfig::frame_rate_denominator() const {
  return frame_rate_denominator_;
}

uint8* VideoDecoderConfig::extra_data() const {
  return extra_data_.get();
}

size_t VideoDecoderConfig::extra_data_size() const {
  return extra_data_size_;
}

}  // namespace media
