// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/video_decoder_config.h"

#include "base/logging.h"

namespace media {

VideoDecoderConfig::VideoDecoderConfig(VideoCodec codec,
                                       int width,
                                       int height,
                                       int surface_width,
                                       int surface_height,
                                       int frame_rate_numerator,
                                       int frame_rate_denominator,
                                       const uint8* extra_data,
                                       size_t extra_data_size)
    : codec_(codec),
      width_(width),
      height_(height),
      surface_width_(surface_width),
      surface_height_(surface_height),
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

int VideoDecoderConfig::width() const {
  return width_;
}

int VideoDecoderConfig::height() const {
  return height_;
}

int VideoDecoderConfig::surface_width() const {
  return surface_width_;
}

int VideoDecoderConfig::surface_height() const {
  return surface_height_;
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
