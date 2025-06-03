// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/native_theme/native_theme_features.h"

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"

namespace features {

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS_ASH) ||    \
    BUILDFLAG(IS_FUCHSIA) || BUILDFLAG(IS_CHROMEOS_LACROS) || \
    BUILDFLAG(IS_IOS)
constexpr base::FeatureState kOverlayScrollbarFeatureState =
    base::FEATURE_ENABLED_BY_DEFAULT;
#else
constexpr base::FeatureState kOverlayScrollbarFeatureState =
    base::FEATURE_DISABLED_BY_DEFAULT;
#endif

// Enables or disables overlay scrollbars in Blink (i.e. web content) on Aura
// or Linux.  The status of native UI overlay scrollbars is determined in
// PlatformStyle::CreateScrollBar. Does nothing on Mac.
BASE_FEATURE(kOverlayScrollbar,
             "OverlayScrollbar",
             kOverlayScrollbarFeatureState);

// Fluent scrollbars aim to modernize the Chromium scrollbars (both overlay
// and non-overlay) to fit the Windows 11 Fluent design language. For now,
// the feature will only support Windows platform and can be later available
// on Linux as well. The feature is currently in development and disabled
// by default.
BASE_FEATURE(kFluentScrollbar,
             "FluentScrollbar",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Makes all native scrollbars behave as overlay scrollbars styled to fit the
// Windows 11 Fluent design language.
// TODO(crbug.com/1479156): Right now this feature flag will force Fluent
// overlay scrollbars on. We have yet to decide how we will expose this feature
// once it is complete.
BASE_FEATURE(kFluentOverlayScrollbar,
             "FluentOverlayScrollbar",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace features

namespace ui {

bool IsOverlayScrollbarEnabled() {
  return base::FeatureList::IsEnabled(features::kOverlayScrollbar) ||
         IsFluentOverlayScrollbarEnabled();
}

bool IsFluentScrollbarEnabled() {
// Currently, the feature is only supported on Windows. Please see more details
// here: https://crbug.com/1353432.
#if BUILDFLAG(IS_WIN)
  return base::FeatureList::IsEnabled(features::kFluentScrollbar) ||
         IsFluentOverlayScrollbarEnabled();
#else
  return false;
#endif
}
bool IsFluentOverlayScrollbarEnabled() {
// Currently, the feature is only supported on Windows. Please see more details
// here: https://crbug.com/1353432.
#if BUILDFLAG(IS_WIN)
  return base::FeatureList::IsEnabled(features::kFluentOverlayScrollbar);
#else
  return false;
#endif
}

}  // namespace ui
