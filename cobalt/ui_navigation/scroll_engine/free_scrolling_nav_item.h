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

#ifndef COBALT_UI_NAVIGATION_SCROLL_ENGINE_FREE_SCROLLING_NAV_ITEM_H_
#define COBALT_UI_NAVIGATION_SCROLL_ENGINE_FREE_SCROLLING_NAV_ITEM_H_

#include "cobalt/cssom/timing_function.h"
#include "cobalt/math/vector2d_f.h"
#include "cobalt/ui_navigation/nav_item.h"

namespace cobalt {
namespace ui_navigation {
namespace scroll_engine {

class FreeScrollingNavItem {
 public:
  FreeScrollingNavItem(scoped_refptr<NavItem> nav_item,
                       math::Vector2dF initial_offset,
                       math::Vector2dF target_offset,
                       base::TimeDelta animation_duration,
                       float animation_slope);

 public:
  scoped_refptr<NavItem> nav_item() { return nav_item_; }
  float GetFractionOfCurrentProgress();
  bool AnimationIsComplete();
  math::Vector2dF GetCurrentOffset();

 private:
  scoped_refptr<NavItem> nav_item_;
  math::Vector2dF initial_offset_;
  math::Vector2dF target_offset_;
  scoped_refptr<cssom::CubicBezierTimingFunction> animation_function_;
  base::TimeDelta animation_duration_;
  base::Time initial_change_;
};

}  // namespace scroll_engine
}  // namespace ui_navigation
}  // namespace cobalt

#endif  // COBALT_UI_NAVIGATION_SCROLL_ENGINE_FREE_SCROLLING_NAV_ITEM_H_
