// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_BROWSER_ACCESSIBILITY_COCOA_TEST_HELPERS_H_
#define UI_ACCESSIBILITY_PLATFORM_BROWSER_ACCESSIBILITY_COCOA_TEST_HELPERS_H_

#include "base/component_export.h"

// C++-only test-infrastructure entry points for BrowserAccessibilityCocoa.
// This header is intentionally separate from browser_accessibility_cocoa.h
// (which is Objective-C++) so plain C++ test runners can opt the
// production wrapper into test-only surface without including Cocoa.

namespace ui {

// Enables the @"AXCustomActionNamesForTesting" projection attribute on
// BrowserAccessibilityCocoa for dump-test inspection of aria-actions
// custom-action names across the AXUIElementCopyAttributeValue marshaling
// boundary (NSAccessibilityCustomAction objects are not marshalable).
//
// Test infrastructure only. Production code never calls this function,
// so the attribute is invisible in shipped Chrome: it is not advertised
// by -accessibilityAttributeNames AND the underlying selector returns nil
// for any same-process direct -accessibilityAttributeValue: query.
COMPONENT_EXPORT(AX_PLATFORM)
void EnableAXCustomActionNamesForTestingProjection();

// Returns whether the test-only projection is currently enabled. Used by
// the implementation to gate both enumeration and dispatch on a single
// flag.
COMPONENT_EXPORT(AX_PLATFORM)
bool IsAXCustomActionNamesForTestingProjectionEnabled();

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_BROWSER_ACCESSIBILITY_COCOA_TEST_HELPERS_H_
