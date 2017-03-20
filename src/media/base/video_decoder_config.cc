// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/video_decoder_config.h"

#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "media/base/video_types.h"

namespace media {

VideoDecoderConfig::VideoDecoderConfig()
    : codec_(kUnknownVideoCodec),
      profile_(VIDEO_CODEC_PROFILE_UNKNOWN),
      format_(VideoFrame::INVALID),
      extra_data_size_(0),
      is_encrypted_(false),
      color_space_(COLOR_SPACE_UNSPECIFIED) {}

VideoDecoderConfig::VideoDecoderConfig(VideoCodec codec,
                                       VideoCodecProfile profile,
                                       VideoFrame::Format format,
                                       ColorSpace color_space,
                                       const gfx::Size& coded_size,
                                       const gfx::Rect& visible_rect,
                                       const gfx::Size& natural_size,
                                       const uint8* extra_data,
                                       size_t extra_data_size,
                                       bool is_encrypted) {
  Initialize(codec, profile, format, color_space, coded_size, visible_rect,
             natural_size, extra_data, extra_data_size, is_encrypted, true);
}

VideoDecoderConfig::~VideoDecoderConfig() {}

// Some videos just want to watch the world burn, with a height of 0; cap the
// "infinite" aspect ratio resulting.
static const int kInfiniteRatio = 99999;

// Common aspect ratios (multiplied by 100 and truncated) used for histogramming
// video sizes.  These were taken on 20111103 from
// http://wikipedia.org/wiki/Aspect_ratio_(image)#Previous_and_currently_used_aspect_ratios
static const int kCommonAspectRatios100[] = {
  100, 115, 133, 137, 143, 150, 155, 160, 166, 175, 177, 185, 200, 210, 220,
  221, 235, 237, 240, 255, 259, 266, 276, 293, 400, 1200, kInfiniteRatio,
};

template<class T>  // T has int width() & height() methods.
static void UmaHistogramAspectRatio(const char* name, const T& size) {
  UMA_HISTOGRAM_CUSTOM_ENUMERATION(
      name,
      // Intentionally use integer division to truncate the result.
      size.height() ? (size.width() * 100) / size.height() : kInfiniteRatio,
      base::CustomHistogram::ArrayToCustomRanges(
          kCommonAspectRatios100, arraysize(kCommonAspectRatios100)));
}

void VideoDecoderConfig::Initialize(VideoCodec codec,
                                    VideoCodecProfile profile,
                                    VideoFrame::Format format,
                                    ColorSpace color_space,
                                    const gfx::Size& coded_size,
                                    const gfx::Rect& visible_rect,
                                    const gfx::Size& natural_size,
                                    const uint8* extra_data,
                                    size_t extra_data_size,
                                    bool is_encrypted,
                                    bool record_stats) {
  CHECK((extra_data_size != 0) == (extra_data != NULL));

  if (record_stats) {
    UMA_HISTOGRAM_ENUMERATION("Media.VideoCodec", codec, kVideoCodecMax + 1);
    // Drop UNKNOWN because U_H_E() uses one bucket for all values less than 1.
    if (profile >= 0) {
      UMA_HISTOGRAM_ENUMERATION("Media.VideoCodecProfile", profile,
                                VIDEO_CODEC_PROFILE_MAX + 1);
    }
    UMA_HISTOGRAM_COUNTS_10000("Media.VideoCodedWidth", coded_size.width());
    UmaHistogramAspectRatio("Media.VideoCodedAspectRatio", coded_size);
    UMA_HISTOGRAM_COUNTS_10000("Media.VideoVisibleWidth", visible_rect.width());
    UmaHistogramAspectRatio("Media.VideoVisibleAspectRatio", visible_rect);
  }

  codec_ = codec;
  profile_ = profile;
  format_ = format;
  color_space_ = color_space;
  coded_size_ = coded_size;
  visible_rect_ = visible_rect;
  natural_size_ = natural_size;
  extra_data_size_ = extra_data_size;

  if (extra_data_size_ > 0) {
    extra_data_.reset(new uint8[extra_data_size_]);
    memcpy(extra_data_.get(), extra_data, extra_data_size_);
  } else {
    extra_data_.reset();
  }

  is_encrypted_ = is_encrypted;

  switch (color_space) {
    case COLOR_SPACE_JPEG:
      webm_color_metadata_.color_space = gfx::ColorSpace::CreateJpeg();
      break;
    case COLOR_SPACE_HD_REC709:
      webm_color_metadata_.color_space = gfx::ColorSpace::CreateREC709();
      break;
    case COLOR_SPACE_SD_REC601:
      webm_color_metadata_.color_space = gfx::ColorSpace::CreateREC601();
      break;
    case COLOR_SPACE_UNSPECIFIED:
      break;
    default:
      NOTREACHED();
      break;
  }
}

void VideoDecoderConfig::CopyFrom(const VideoDecoderConfig& video_config) {
  Initialize(video_config.codec(), video_config.profile(),
             video_config.format(), video_config.color_space_,
             video_config.coded_size(), video_config.visible_rect(),
             video_config.natural_size(), video_config.extra_data(),
             video_config.extra_data_size(), video_config.is_encrypted(),
             false);
  webm_color_metadata_ = video_config.webm_color_metadata_;
}

bool VideoDecoderConfig::IsValidConfig() const {
  return codec_ != kUnknownVideoCodec &&
      natural_size_.width() > 0 &&
      natural_size_.height() > 0 &&
      VideoFrame::IsValidConfig(format_, coded_size_, visible_rect_,
          natural_size_);
}

bool VideoDecoderConfig::Matches(const VideoDecoderConfig& config) const {
  return ((codec() == config.codec()) && (format() == config.format()) &&
          (webm_color_metadata_ == config.webm_color_metadata()) &&
          (profile() == config.profile()) &&
          (coded_size() == config.coded_size()) &&
          (visible_rect() == config.visible_rect()) &&
          (natural_size() == config.natural_size()) &&
          (extra_data_size() == config.extra_data_size()) &&
          (!extra_data() ||
           !memcmp(extra_data(), config.extra_data(), extra_data_size())) &&
          (is_encrypted() == config.is_encrypted()));
}

std::string VideoDecoderConfig::AsHumanReadableString() const {
  std::ostringstream s;
  s << "codec: " << codec()
    << " format: " << format()
    << " profile: " << profile()
    << " coded size: [" << coded_size().width()
    << "," << coded_size().height() << "]"
    << " visible rect: [" << visible_rect().x()
    << "," << visible_rect().y()
    << "," << visible_rect().width()
    << "," << visible_rect().height() << "]"
    << " natural size: [" << natural_size().width()
    << "," << natural_size().height() << "]"
    << " has extra data? " << (extra_data() ? "true" : "false")
    << " encrypted? " << (is_encrypted() ? "true" : "false");
  return s.str();
}

VideoCodec VideoDecoderConfig::codec() const {
  return codec_;
}

VideoCodecProfile VideoDecoderConfig::profile() const {
  return profile_;
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

uint8* VideoDecoderConfig::extra_data() const {
  return extra_data_.get();
}

size_t VideoDecoderConfig::extra_data_size() const {
  return extra_data_size_;
}

bool VideoDecoderConfig::is_encrypted() const {
  return is_encrypted_;
}

}  // namespace media
