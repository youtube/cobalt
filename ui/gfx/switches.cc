// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/switches.h"

#include "base/command_line.h"
#include "build/build_config.h"

namespace switches {

// Scale factor to apply to every animation duration. Must be >= 0.0. This will
// only apply to LinearAnimation and its subclasses.
const char kAnimationDurationScale[] = "animation-duration-scale";

// Force disables font subpixel positioning. This affects the character glyph
// sharpness, kerning, hinting and layout.
const char kDisableFontSubpixelPositioning[] =
    "disable-font-subpixel-positioning";

// Disables new code to run SharedImages for NaCL swapchain. This overrides
// value of kPPAPISharedImagesSwapChain feature flag.
const char kDisablePPAPISharedImagesSwapChain[] =
    "disable-ppapi-shared-images-swapchain";

// Enable native CPU-mappable GPU memory buffer support on Linux.
const char kEnableNativeGpuMemoryBuffers[] = "enable-native-gpu-memory-buffers";

// Forces whether the user desires reduced motion, regardless of system
// settings.
const char kForcePrefersReducedMotion[] = "force-prefers-reduced-motion";

// Run in headless mode, i.e., without a UI or display server dependencies.
const char kHeadless[] = "headless";

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
// Which X11 display to connect to. Emulates the GTK+ "--display=" command line
// argument. In use only with Ozone/X11.
const char kX11Display[] = "display";
// Disables MIT-SHM extension. In use only with Ozone/X11.
const char kNoXshm[] = "no-xshm";
#endif

}  // namespace switches

namespace features {
BASE_FEATURE(kOddHeightMultiPlanarBuffers,
             "OddHeightMultiPlanarBuffers",
#if BUILDFLAG(IS_APPLE)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

BASE_FEATURE(kOddWidthMultiPlanarBuffers,
             "OddWidthMultiPlanarBuffers",
#if BUILDFLAG(IS_APPLE)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

BASE_FEATURE(kPPAPISharedImagesSwapChain,
             "PPAPISharedImagesSwapChain",
             base::FEATURE_ENABLED_BY_DEFAULT);

#if BUILDFLAG(IS_CHROMEOS)
BASE_FEATURE(kVariableGoogleSansFont,
             "VariableGoogleSansFont",
             base::FEATURE_DISABLED_BY_DEFAULT);

GFX_SWITCHES_EXPORT bool UseVariableGoogleSansFont() {
  return base::FeatureList::IsEnabled(kVariableGoogleSansFont);
}
#endif

}  // namespace features
