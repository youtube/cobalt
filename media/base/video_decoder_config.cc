// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/video_decoder_config.h"

#include <cmath>

#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "media/base/limits.h"

namespace media {

VideoDecoderConfig::VideoDecoderConfig()
    : codec_(kUnknownVideoCodec),
      format_(VideoFrame::INVALID),
      frame_rate_numerator_(0),
      frame_rate_denominator_(0),
      aspect_ratio_numerator_(0),
      aspect_ratio_denominator_(0),
      extra_data_size_(0) {
}

VideoDecoderConfig::VideoDecoderConfig(VideoCodec codec,
                                       VideoFrame::Format format,
                                       const gfx::Size& coded_size,
                                       const gfx::Rect& visible_rect,
                                       int frame_rate_numerator,
                                       int frame_rate_denominator,
                                       int aspect_ratio_numerator,
                                       int aspect_ratio_denominator,
                                       const uint8* extra_data,
                                       size_t extra_data_size) {
  Initialize(codec, format, coded_size, visible_rect,
             frame_rate_numerator, frame_rate_denominator,
             aspect_ratio_numerator, aspect_ratio_denominator,
             extra_data, extra_data_size);
}

VideoDecoderConfig::~VideoDecoderConfig() {}

// Common aspect ratios (multiplied by 100 and truncated) used for histogramming
// video sizes.  These were taken on 20111103 from
// http://wikipedia.org/wiki/Aspect_ratio_(image)#Previous_and_currently_used_aspect_ratios
static const int kCommonAspectRatios100[] = {
  100, 115, 133, 137, 143, 150, 155, 160, 166, 175, 177, 185, 200, 210, 220,
  221, 235, 237, 240, 255, 259, 266, 276, 293, 400, 1200,
};

template<class T>  // T has int width() & height() methods.
static void UmaHistogramAspectRatio(const char* name, const T& size) {
  UMA_HISTOGRAM_CUSTOM_ENUMERATION(
      name,
      // Intentionally use integer division to truncate the result.
      (size.width() * 100) / size.height(),
      base::CustomHistogram::ArrayToCustomRanges(
          kCommonAspectRatios100, arraysize(kCommonAspectRatios100)));
}

void VideoDecoderConfig::Initialize(VideoCodec codec, VideoFrame::Format format,
                                    const gfx::Size& coded_size,
                                    const gfx::Rect& visible_rect,
                                    int frame_rate_numerator,
                                    int frame_rate_denominator,
                                    int aspect_ratio_numerator,
                                    int aspect_ratio_denominator,
                                    const uint8* extra_data,
                                    size_t extra_data_size) {
  CHECK((extra_data_size != 0) == (extra_data != NULL));

  UMA_HISTOGRAM_ENUMERATION("Media.VideoCodec", codec, kVideoCodecMax + 1);
  UMA_HISTOGRAM_COUNTS_10000("Media.VideoCodedWidth", coded_size.width());
  UmaHistogramAspectRatio("Media.VideoCodedAspectRatio", coded_size);
  UMA_HISTOGRAM_COUNTS_10000("Media.VideoVisibleWidth", visible_rect.width());
  UmaHistogramAspectRatio("Media.VideoVisibleAspectRatio", visible_rect);

  codec_ = codec;
  format_ = format;
  coded_size_ = coded_size;
  visible_rect_ = visible_rect;
  frame_rate_numerator_ = frame_rate_numerator;
  frame_rate_denominator_ = frame_rate_denominator;
  aspect_ratio_numerator_ = aspect_ratio_numerator;
  aspect_ratio_denominator_ = aspect_ratio_denominator;
  extra_data_size_ = extra_data_size;

  if (extra_data_size_ > 0) {
    extra_data_.reset(new uint8[extra_data_size_]);
    memcpy(extra_data_.get(), extra_data, extra_data_size_);
  } else {
    extra_data_.reset();
  }

  // Calculate the natural size given the aspect ratio and visible rect.
  if (aspect_ratio_denominator == 0) {
    natural_size_.SetSize(0, 0);
    return;
  }

  double aspect_ratio = aspect_ratio_numerator /
      static_cast<double>(aspect_ratio_denominator);

  int width = floor(visible_rect.width() * aspect_ratio + 0.5);
  int height = visible_rect.height();

  // An even width makes things easier for YV12 and appears to be the behavior
  // expected by WebKit layout tests.
  natural_size_.SetSize(width & ~1, height);
}

bool VideoDecoderConfig::IsValidConfig() const {
  return codec_ != kUnknownVideoCodec &&
      format_ != VideoFrame::INVALID &&
      frame_rate_numerator_ > 0 &&
      frame_rate_denominator_ > 0 &&
      aspect_ratio_numerator_ > 0 &&
      aspect_ratio_denominator_ > 0 &&
      natural_size_.width() <= Limits::kMaxDimension &&
      natural_size_.height() <= Limits::kMaxDimension &&
      natural_size_.GetArea() <= Limits::kMaxCanvas;
}

VideoCodec VideoDecoderConfig::codec() const {
  return codec_;
}

VideoFrame::Format VideoDecoderConfig::format() const {
  return format_;
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

int VideoDecoderConfig::aspect_ratio_numerator() const {
  return aspect_ratio_numerator_;
}

int VideoDecoderConfig::aspect_ratio_denominator() const {
  return aspect_ratio_denominator_;
}

uint8* VideoDecoderConfig::extra_data() const {
  return extra_data_.get();
}

size_t VideoDecoderConfig::extra_data_size() const {
  return extra_data_size_;
}

}  // namespace media
