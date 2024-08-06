// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_CHROMIUM_MEDIA_MOJO_SERVICES_TEST_HELPERS_H_
#define THIRD_PARTY_CHROMIUM_MEDIA_MOJO_SERVICES_TEST_HELPERS_H_

#include <string>

#include "third_party/chromium/media/mojo/mojom/media_types.mojom.h"

namespace media_m96 {

mojom::PredictionFeatures MakeFeatures(VideoCodecProfile profile,
                                       const gfx::Size& video_size,
                                       double frames_per_sec,
                                       const std::string& key_system = "",
                                       bool use_hw_secure_codecs = false);

mojom::PredictionFeaturesPtr MakeFeaturesPtr(VideoCodecProfile profile,
                                             const gfx::Size& video_size,
                                             double frames_per_sec,
                                             const std::string& key_system = "",
                                             bool use_hw_secure_codecs = false);

mojom::PredictionTargets MakeTargets(uint32_t frames_decoded,
                                     uint32_t frames_dropped,
                                     uint32_t frames_power_efficient);

mojom::PredictionTargetsPtr MakeTargetsPtr(uint32_t frames_decoded,
                                           uint32_t frames_dropped,
                                           uint32_t frames_power_efficient);

}  // namespace media_m96

#endif  // THIRD_PARTY_CHROMIUM_MEDIA_MOJO_SERVICES_TEST_HELPERS_H_
