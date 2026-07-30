// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.accessibility.testservice;

import org.chromium.ui.accessibility.testservice.EventMatcher;
import org.chromium.ui.accessibility.testservice.NodeMatcher;

parcelable WaitForParams {
    /** Expected parameters for the event to wait for. Null if not waiting for an event. */
    EventMatcher eventMatcher;
    /** Expected parameters for the node to wait for. Null if not waiting for a node. */
    NodeMatcher nodeMatcher;
    /** The maximum time to wait in milliseconds. */
    long timeoutMs;
}
