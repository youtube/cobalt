// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import org.chromium.build.annotations.NullMarked;

/**
 * Interface to expose package-private internal state of {@link TabImpl} to other classes in the
 * {@code org.chromium.chrome.browser.tab} package without exposing them on the public {@link Tab}
 * interface.
 */
@NullMarked
interface TabInternal {
    /** Returns the native pointer representing the native side of this tab. */
    long getNativePtr();
}
