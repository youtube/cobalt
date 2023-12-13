// Copyright 2023 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "cobalt/media/base/mock_filters.h"

namespace cobalt {
namespace media {

MockDemuxer::MockDemuxer() = default;
MockDemuxer::~MockDemuxer() = default;

std::string MockDemuxer::GetDisplayName() const { return "MockDemuxer"; }


MockDemuxerStream::MockDemuxerStream(DemuxerStream::Type type) : type_(type) {}

MockDemuxerStream::~MockDemuxerStream() = default;

DemuxerStream::Type MockDemuxerStream::type() const { return type_; }

// StreamLiveness MockDemuxerStream::liveness() const {
//   return liveness_;
// }
//
AudioDecoderConfig MockDemuxerStream::audio_decoder_config() {
  DCHECK_EQ(type_, DemuxerStream::AUDIO);
  return audio_decoder_config_;
}

VideoDecoderConfig MockDemuxerStream::video_decoder_config() {
  DCHECK_EQ(type_, DemuxerStream::VIDEO);
  return video_decoder_config_;
}
//
// void MockDemuxerStream::set_audio_decoder_config(
//    const AudioDecoderConfig& config) {
//  DCHECK_EQ(type_, DemuxerStream::AUDIO);
//  audio_decoder_config_ = config;
//}
//
// void MockDemuxerStream::set_video_decoder_config(
//    const VideoDecoderConfig& config) {
//  DCHECK_EQ(type_, DemuxerStream::VIDEO);
//  video_decoder_config_ = config;
//}
//
// void MockDemuxerStream::set_liveness(StreamLiveness liveness) {
//  liveness_ = liveness;
//}


}  // namespace media
}  // namespace cobalt
