// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_streaming/browser/public/receiver_config.h"

#include <utility>

namespace cast_streaming {

ReceiverConfig::ReceiverConfig() = default;

ReceiverConfig::ReceiverConfig(std::vector<media::VideoCodec> video_codecs,
                               std::vector<media::AudioCodec> audio_codecs)
    : video_codecs(std::move(video_codecs)),
      audio_codecs(std::move(audio_codecs)) {}

ReceiverConfig::ReceiverConfig(std::vector<media::VideoCodec> video_codecs,
                               std::vector<media::AudioCodec> audio_codecs,
                               std::vector<AudioLimits> audio_limits,
                               std::vector<VideoLimits> video_limits,
                               absl::optional<Display> description)
    : video_codecs(std::move(video_codecs)),
      audio_codecs(std::move(audio_codecs)),
      audio_limits(std::move(audio_limits)),
      video_limits(std::move(video_limits)),
      display_description(std::move(description)) {}

ReceiverConfig::~ReceiverConfig() = default;

ReceiverConfig::ReceiverConfig(ReceiverConfig&&) noexcept = default;
ReceiverConfig& ReceiverConfig::operator=(ReceiverConfig&&) noexcept = default;

ReceiverConfig::ReceiverConfig(const ReceiverConfig& other) = default;

ReceiverConfig& ReceiverConfig::operator=(const ReceiverConfig& other) =
    default;

}  // namespace cast_streaming
