// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/browser/headless_screen_orientation_delegate.h"

#include "content/public/browser/web_contents.h"
#include "headless/lib/browser/headless_screen.h"
#include "services/device/public/mojom/screen_orientation_lock_types.mojom.h"
#include "ui/display/mojom/screen_orientation.mojom-shared.h"
#include "ui/gfx/geometry/rect_f.h"

namespace headless {

namespace {

display::mojom::ScreenOrientation GetScreenOrientationFromLockOrientation(
    device::mojom::ScreenOrientationLockType lock_orientation) {
  switch (lock_orientation) {
    case device::mojom::ScreenOrientationLockType::PORTRAIT:
    case device::mojom::ScreenOrientationLockType::PORTRAIT_PRIMARY:
      return display::mojom::ScreenOrientation::kPortraitPrimary;
    case device::mojom::ScreenOrientationLockType::PORTRAIT_SECONDARY:
      return display::mojom::ScreenOrientation::kPortraitSecondary;

    case device::mojom::ScreenOrientationLockType::LANDSCAPE:
    case device::mojom::ScreenOrientationLockType::LANDSCAPE_PRIMARY:
      return display::mojom::ScreenOrientation::kLandscapePrimary;
    case device::mojom::ScreenOrientationLockType::LANDSCAPE_SECONDARY:
      return display::mojom::ScreenOrientation::kLandscapeSecondary;

    case device::mojom::ScreenOrientationLockType::ANY:
      return display::mojom::ScreenOrientation::kUndefined;

    case device::mojom::ScreenOrientationLockType::NATURAL:
    case device::mojom::ScreenOrientationLockType::DEFAULT:
      return display::mojom::ScreenOrientation::kPortraitPrimary;
  }

  return display::mojom::ScreenOrientation::kUndefined;
}

}  // namespace

HeadlessScreenOrientationDelegate::HeadlessScreenOrientationDelegate() {
  content::WebContents::SetScreenOrientationDelegate(this);
}

HeadlessScreenOrientationDelegate::~HeadlessScreenOrientationDelegate() {
  content::WebContents::SetScreenOrientationDelegate(nullptr);
}

bool HeadlessScreenOrientationDelegate::FullScreenRequired(
    content::WebContents* web_contents) {
  return false;
}

void HeadlessScreenOrientationDelegate::Lock(
    content::WebContents* web_contents,
    device::mojom::ScreenOrientationLockType lock_orientation) {
  display::mojom::ScreenOrientation screen_orientation =
      GetScreenOrientationFromLockOrientation(lock_orientation);
  HeadlessScreen::UpdateScreenSizeForScreenOrientation(screen_orientation);
}

bool HeadlessScreenOrientationDelegate::ScreenOrientationProviderSupported(
    content::WebContents* web_contentss) {
  return true;
}

void HeadlessScreenOrientationDelegate::Unlock(
    content::WebContents* web_contents) {}

}  // namespace headless
