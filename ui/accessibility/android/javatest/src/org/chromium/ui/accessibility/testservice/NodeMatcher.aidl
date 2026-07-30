// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.accessibility.testservice;

parcelable NodeMatcher {
    /** The expected class name of the node. Null or empty string matches any class name. */
    String className;
    /** The expected text of the node. Null or empty string matches any text. */
    String text;
    /** Whether inputFocused check is enabled. */
    boolean hasInputFocused;
    /** Whether the node must be input focused. */
    boolean inputFocused;
    /** Whether accessibilityFocused check is enabled. */
    boolean hasAccessibilityFocused;
    /** Whether the node must be accessibility focused. */
    boolean accessibilityFocused;
}
