// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.accessibility.testservice;

import android.os.Bundle;
import org.chromium.ui.accessibility.testservice.NodeMatcher;
import org.chromium.ui.accessibility.testservice.WaitForParams;

interface IAccessibilityTestHelperService {
    /**
     * Waits for an accessibility event/node matching the given query parameters.
     * Returns true if the condition is met within the timeout, false otherwise.
     *
     * @param params The wait parameters.
     */
    boolean waitFor(in WaitForParams params);

    /**
     * Finds a node matching the matcher and performs the given action on it.
     *
     * @param matcher The node matching criteria.
     * @param action The action to perform (e.g., AccessibilityNodeInfo.ACTION_HOVER_ENTER).
     * @param arguments The arguments bundle, which can be null.
     * @return true if the action was performed successfully.
     */
    boolean performActionOnNode(
            in NodeMatcher matcher, int action, in @nullable Bundle arguments);

    /**
     * Dumps the accessibility tree to a String.
     *
     * @return The accessibility tree as a String.
     */
    String dumpWebContentsAccessibilityTree();
}
