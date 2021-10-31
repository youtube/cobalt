// Copyright 2020 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/shared/starboard/media/video_capabilities.h"

#include "starboard/common/log.h"
#include "starboard/common/media.h"
#include "starboard/common/string.h"
#include "starboard/shared/starboard/media/media_util.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace media {

VideoCapabilities::Rule::Rule(SbMediaVideoCodec codec,
                              int width,
                              int height,
                              int fps)
    : codec_(codec), width_(width), height_(height), fps_(fps) {
  SB_DCHECK(width >= 0);
  SB_DCHECK(height >= 0);
  SB_DCHECK(fps >= 0);
  SB_DCHECK(codec_ != kSbMediaVideoCodecNone);
}

std::string VideoCapabilities::Rule::AsString() const {
  return FormatString("(%s, %d x %d @ %d fps)", GetMediaVideoCodecName(codec_),
                      width_, height_, fps_);
}

bool VideoCapabilities::Rule::CanBeInferredBy(const Rule& that) const {
  return codec_ == that.codec_ && width_ <= that.width_ &&
         height_ <= that.height_ && fps_ <= that.fps_;
}

void VideoCapabilities::AddSdrRule(SbMediaVideoCodec codec,
                                   int width,
                                   int height,
                                   int fps) {
  SB_DCHECK(width > 0);
  SB_DCHECK(height > 0);
  SB_DCHECK(fps > 0);
  AddRule(Rule(codec, width, height, fps), &sdr_rules_);
}

void VideoCapabilities::AddHdrRule(SbMediaVideoCodec codec,
                                   int width,
                                   int height,
                                   int fps) {
  SB_DCHECK(width > 0);
  SB_DCHECK(height > 0);
  SB_DCHECK(fps > 0);
  AddRule(Rule(codec, width, height, fps), &hdr_rules_);
}

bool VideoCapabilities::IsSupported(SbMediaVideoCodec codec,
                                    SbMediaTransferId transfer_id,
                                    int width,
                                    int height,
                                    int fps) const {
  Rule rule(codec, width, height, fps);
  if (IsSDRVideo(8, kSbMediaPrimaryIdBt709, transfer_id,
                 kSbMediaMatrixIdBt709)) {
    return IsSupported(rule, sdr_rules_);
  }
  return IsSupported(rule, hdr_rules_);
}

// static
void VideoCapabilities::AddRule(const Rule& new_rule, Rules* rules) {
  SB_DCHECK(rules);

#if !defined(COBALT_BUILD_TYPE_GOLD)
  for (auto& rule : *rules) {
    if (rule.CanBeInferredBy(new_rule)) {
      SB_LOG(WARNING) << "Existing rule " << rule.AsString()
                      << " can be inferred by new rule " << new_rule.AsString();
    } else if (new_rule.CanBeInferredBy(rule)) {
      SB_LOG(WARNING) << "New rule " << new_rule.AsString()
                      << " can be inferred by existing rule "
                      << rule.AsString();
    }
  }
#endif  // defined(COBALT_BUILD_TYPE_GOLD)

  rules->push_back(new_rule);
}

// static
bool VideoCapabilities::IsSupported(const Rule& rule_to_check,
                                    const Rules& rules) {
  for (auto& rule : rules) {
    if (rule_to_check.CanBeInferredBy(rule)) {
      return true;
    }
  }

  return false;
}

}  // namespace media
}  // namespace starboard
}  // namespace shared
}  // namespace starboard
