// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_BLINK_BLINK_FEATURES_H_
#define UI_EVENTS_BLINK_BLINK_FEATURES_H_

#include "base/component_export.h"
#include "base/feature_list.h"

namespace features {

// This feature allows native ET_MOUSE_EXIT events to be passed
// through to blink as mouse leave events. Traditionally these events were
// converted to mouse move events due to a number of inconsistencies on
// the native platforms. crbug.com/450631
COMPONENT_EXPORT(BLINK_FEATURES) BASE_DECLARE_FEATURE(kSendMouseLeaveEvents);

// When enabled, this feature prevent blink sending key event to web unless it
// is on installed PWA.
COMPONENT_EXPORT(BLINK_FEATURES)
BASE_DECLARE_FEATURE(kDontSendKeyEventsToJavascript);

// Reduces the velocity of horizontal flings. This is an experiment to test
// a simple means of making flings in mandatory scroll snap areas feel more
// natural. See https://crbug.com/1189696 for details.
COMPONENT_EXPORT(BLINK_FEATURES)
BASE_DECLARE_FEATURE(kReduceHorizontalFlingVelocity);
}

#endif  // UI_EVENTS_BLINK_BLINK_FEATURES_H_
