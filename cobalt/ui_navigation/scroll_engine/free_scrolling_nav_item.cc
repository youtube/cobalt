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

#include "cobalt/ui_navigation/scroll_engine/free_scrolling_nav_item.h"

#include <algorithm>

#include "base/logging.h"

namespace cobalt {
namespace ui_navigation {
namespace scroll_engine {

FreeScrollingNavItem::FreeScrollingNavItem(scoped_refptr<NavItem> nav_item,
                                           math::Vector2dF initial_offset,
                                           math::Vector2dF target_offset,
                                           base::TimeDelta animation_duration,
                                           float animation_slope)
    : nav_item_(nav_item),
      initial_offset_(initial_offset),
      target_offset_(target_offset),
      animation_duration_(animation_duration) {
  initial_change_ = base::Time::Now();

  // Constants are derived from the ease-in-out curve definition here:
  // https://www.w3.org/TR/2023/CRD-css-easing-1-20230213/#typedef-cubic-bezier-easing-function
  animation_function_ =
      base::WrapRefCounted(new cssom::CubicBezierTimingFunction(
          0.42f, 0.42f * animation_slope, 0.58f, 1.f));
}

float FreeScrollingNavItem::GetFractionOfCurrentProgress() {
  if (animation_duration_.is_zero()) {
    return 1.f;
  }

  auto now = base::Time::Now();
  auto time_delta = now - initial_change_;
  auto fraction_of_progress =
      time_delta.InMillisecondsF() / animation_duration_.InMillisecondsF();
  return std::min<float>(static_cast<float>(fraction_of_progress), 1.f);
}

bool FreeScrollingNavItem::AnimationIsComplete() {
  auto fraction_of_current_progress = GetFractionOfCurrentProgress();
  return fraction_of_current_progress >= 1.f;
}

math::Vector2dF FreeScrollingNavItem::GetCurrentOffset() {
  auto fraction_of_current_progress = GetFractionOfCurrentProgress();
  float progress = animation_function_->Evaluate(fraction_of_current_progress);
  auto distance_delta = target_offset_ - initial_offset_;
  distance_delta.Scale(progress);
  return initial_offset_ + distance_delta;
}

}  // namespace scroll_engine
}  // namespace ui_navigation
}  // namespace cobalt
