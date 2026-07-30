// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.accessibility.testservice;

import org.chromium.ui.accessibility.testservice.NodeMatcher;

parcelable EventMatcher {
    /** The type of event to wait for (e.g., AccessibilityEvent.TYPE_VIEW_FOCUSED). */
    int eventType;
    /** The event ContentChangeTypes (e.g., AccessibilityEvent.CONTENT_CHANGE_TYPE_ERROR).
    Optional - assign 0 to match any ContentChangeType or lack thereof. */
    int contentChangeTypes;
    /** Expected parameters for the source node linked to this event. */
    NodeMatcher sourceMatcher;
}
