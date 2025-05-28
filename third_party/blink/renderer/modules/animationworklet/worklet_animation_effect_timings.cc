// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/animationworklet/worklet_animation_effect_timings.h"

namespace blink {

WorkletAnimationEffectTimings::WorkletAnimationEffectTimings(
    scoped_refptr<base::RefCountedData<Vector<Timing>>> timings,
    scoped_refptr<base::RefCountedData<Vector<Timing::NormalizedTiming>>>
        normalized_timings)
    : timings_(timings), normalized_timings_(normalized_timings) {}

std::unique_ptr<cc::AnimationEffectTimings>
WorkletAnimationEffectTimings::Clone() const {
  return std::make_unique<WorkletAnimationEffectTimings>(timings_,
                                                         normalized_timings_);
}

WorkletAnimationEffectTimings::~WorkletAnimationEffectTimings() {}

}  // namespace blink
