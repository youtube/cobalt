// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Define all the command-line switches used by ui/accessibility.
#ifndef UI_ACCESSIBILITY_ACCESSIBILITY_SWITCHES_H_
#define UI_ACCESSIBILITY_ACCESSIBILITY_SWITCHES_H_

#include "build/build_config.h"
#include "ui/accessibility/ax_base_export.h"

namespace switches {

AX_BASE_EXPORT extern const char kEnableExperimentalAccessibilityAutoclick[];
AX_BASE_EXPORT extern const char
    kEnableExperimentalAccessibilityLabelsDebugging[];
AX_BASE_EXPORT extern const char
    kEnableExperimentalAccessibilityLanguageDetection[];
AX_BASE_EXPORT extern const char
    kEnableExperimentalAccessibilityLanguageDetectionDynamic[];
AX_BASE_EXPORT extern const char
    kEnableExperimentalAccessibilitySwitchAccessText[];

// Returns true if experimental accessibility language detection is enabled.
AX_BASE_EXPORT bool IsExperimentalAccessibilityLanguageDetectionEnabled();

// Returns true if experimental accessibility language detection support for
// dynamic content is enabled.
AX_BASE_EXPORT bool
IsExperimentalAccessibilityLanguageDetectionDynamicEnabled();

// Returns true if experimental accessibility Switch Access text is enabled.
AX_BASE_EXPORT bool IsExperimentalAccessibilitySwitchAccessTextEnabled();

#if BUILDFLAG(IS_WIN)
AX_BASE_EXPORT extern const char kEnableExperimentalUIAutomation[];
#endif

// Returns true if experimental support for UIAutomation is enabled.
AX_BASE_EXPORT bool IsExperimentalAccessibilityPlatformUIAEnabled();

// Returns true if Switch Access point scanning is enabled.
AX_BASE_EXPORT bool IsMagnifierDebugDrawRectEnabled();

// Optionally disable AXMenuList, which makes the internal pop-up menu
// UI for a select element directly accessible.
AX_BASE_EXPORT extern const char kDisableAXMenuList[];

// For development / testing only.
// When enabled the switch generates expectations files upon running an
// ax_inspect test. For example, when running content_browsertests, it saves
// output of failing accessibility tests to their expectations files in
// content/test/data/accessibility/, overwriting existing file content.
AX_BASE_EXPORT extern const char kGenerateAccessibilityTestExpectations[];

}  // namespace switches

#endif  // UI_ACCESSIBILITY_ACCESSIBILITY_SWITCHES_H_
