// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/muxers/muxer.h"

#include "media/base/video_codecs.h"
#include "media/base/video_frame.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/size.h"

namespace media {

Muxer::VideoParameters::VideoParameters(const VideoFrame& frame)
    : visible_rect_size(frame.visible_rect().size()),
      frame_rate(frame.metadata().frame_rate.value_or(0.0)),
      codec(VideoCodec::kUnknown),
      color_space(frame.ColorSpace()) {}

Muxer::VideoParameters::VideoParameters(
    gfx::Size visible_rect_size,
    double frame_rate,
    VideoCodec codec,
    absl::optional<gfx::ColorSpace> color_space)
    : visible_rect_size(visible_rect_size),
      frame_rate(frame_rate),
      codec(codec),
      color_space(color_space) {}

Muxer::VideoParameters::VideoParameters(const VideoParameters&) = default;

Muxer::VideoParameters::~VideoParameters() = default;

std::string Muxer::VideoParameters::AsHumanReadableString() const {
  std::ostringstream s;
  s << "size: width (" << visible_rect_size.width() << ") height ("
    << visible_rect_size.height() << ")"
    << ", frame_rate: " << frame_rate << ", video_codec: " << codec;
  return s.str();
}

Muxer::EncodedFrame::EncodedFrame() = default;
Muxer::EncodedFrame::EncodedFrame(
    absl::variant<AudioParameters, VideoParameters> params,
    absl::optional<media::AudioEncoder::CodecDescription> codec_description,
    std::string data,
    std::string alpha_data,
    bool is_keyframe)
    : params(std::move(params)),
      codec_description(std::move(codec_description)),
      data(std::move(data)),
      alpha_data(std::move(alpha_data)),
      is_keyframe(is_keyframe) {}
Muxer::EncodedFrame::~EncodedFrame() = default;
Muxer::EncodedFrame::EncodedFrame(EncodedFrame&&) = default;

}  // namespace media
