// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_POINTER_POINTER_DEVICE_H_
#define UI_BASE_POINTER_POINTER_DEVICE_H_

#include <tuple>

#include "base/component_export.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_ANDROID)
#include <jni.h>
#endif

namespace ui {

enum class TouchScreensAvailability {
  NONE,      // No touch screens are present.
  ENABLED,   // Touch screens are present and enabled.
  DISABLED,  // Touch screens are present and disabled.
};

COMPONENT_EXPORT(UI_BASE)
TouchScreensAvailability GetTouchScreensAvailability();

// Returns the maximum number of simultaneous touch contacts supported
// by the device. In the case of devices with multiple digitizers (e.g.
// multiple touchscreens), the value MUST be the maximum of the set of
// maximum supported contacts by each individual digitizer.
// For example, suppose a device has 3 touchscreens, which support 2, 5,
// and 10 simultaneous touch contacts, respectively. This returns 10.
// http://www.w3.org/TR/pointerevents/#widl-Navigator-maxTouchPoints
COMPONENT_EXPORT(UI_BASE) int MaxTouchPoints();

// Bit field values indicating available pointer types. Identical to
// blink::PointerType enums, enforced by compile-time assertions in
// third_party/blink/public/common/web_preferences/web_preferences.cc.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.ui.base
// GENERATED_JAVA_PREFIX_TO_STRIP: POINTER_TYPE_
enum PointerType {
  POINTER_TYPE_NONE = 1 << 0,
  POINTER_TYPE_FIRST = POINTER_TYPE_NONE,
  POINTER_TYPE_COARSE = 1 << 1,
  POINTER_TYPE_FINE = 1 << 2,
  POINTER_TYPE_LAST = POINTER_TYPE_FINE
};

// Bit field values indicating available hover types. Identical to
// blink::HoverType enums, enforced by compile-time assertions in
// third_party/blink/public/common/web_preferences/web_preferences.cc.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.ui.base
// GENERATED_JAVA_PREFIX_TO_STRIP: HOVER_TYPE_
enum HoverType {
  HOVER_TYPE_NONE = 1 << 0,
  HOVER_TYPE_FIRST = HOVER_TYPE_NONE,
  HOVER_TYPE_HOVER = 1 << 1,
  HOVER_TYPE_LAST = HOVER_TYPE_HOVER
};

int GetAvailablePointerTypes();
int GetAvailableHoverTypes();
COMPONENT_EXPORT(UI_BASE)
std::pair<int, int> GetAvailablePointerAndHoverTypes();
COMPONENT_EXPORT(UI_BASE)
void SetAvailablePointerAndHoverTypesForTesting(int available_pointer_types,
                                                int available_hover_types);
COMPONENT_EXPORT(UI_BASE)
PointerType GetPrimaryPointerType(int available_pointer_types);
COMPONENT_EXPORT(UI_BASE)
HoverType GetPrimaryHoverType(int available_hover_types);

}  // namespace ui

#endif  // UI_BASE_POINTER_POINTER_DEVICE_H_
