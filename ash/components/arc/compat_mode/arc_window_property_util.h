// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_COMPAT_MODE_ARC_WINDOW_PROPERTY_UTIL_H_
#define ASH_COMPONENTS_ARC_COMPAT_MODE_ARC_WINDOW_PROPERTY_UTIL_H_

#include "third_party/abseil-cpp/absl/types/optional.h"

namespace views {
class Widget;
}  // namespace views

namespace aura {
class Window;
}  // namespace aura

namespace arc {

absl::optional<std::string> GetAppId(const aura::Window* window);
absl::optional<std::string> GetAppId(const views::Widget* widget);

}  // namespace arc

#endif  // ASH_COMPONENTS_ARC_COMPAT_MODE_ARC_WINDOW_PROPERTY_UTIL_H_
