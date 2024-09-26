// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COLOR_WIN_ACCENT_COLOR_OBSERVER_H_
#define UI_COLOR_WIN_ACCENT_COLOR_OBSERVER_H_

#include <memory>

#include "base/callback_list.h"
#include "base/component_export.h"
#include "base/win/registry.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/skia/include/core/SkColor.h"

namespace ui {

// Monitors the HKCU\SOFTWARE\Microsoft\Windows\DWM reg key for changes and
// provides access to the accent color (and related colors), as well as support
// for firing callbacks when changes occur.
class COMPONENT_EXPORT(COLOR) AccentColorObserver {
 public:
  static AccentColorObserver* Get();

  AccentColorObserver();
  AccentColorObserver(const AccentColorObserver&) = delete;
  AccentColorObserver& operator=(const AccentColorObserver&) = delete;
  ~AccentColorObserver();

  // Registers `callback` to be called whenever the accent color changes.
  base::CallbackListSubscription Subscribe(base::RepeatingClosure callback);

  absl::optional<SkColor> accent_color() const { return accent_color_; }
  absl::optional<SkColor> accent_color_inactive() const {
    return accent_color_inactive_;
  }
  absl::optional<SkColor> accent_border_color() const {
    return accent_border_color_;
  }

 private:
  void OnDwmKeyUpdated();

  // Registry key containing the params that determine the accent color.
  std::unique_ptr<base::win::RegKey> dwm_key_;

  base::RepeatingClosureList callbacks_;
  absl::optional<SkColor> accent_color_;
  absl::optional<SkColor> accent_color_inactive_;
  absl::optional<SkColor> accent_border_color_;
};

}  // namespace ui

#endif  // UI_COLOR_WIN_ACCENT_COLOR_OBSERVER_H_
