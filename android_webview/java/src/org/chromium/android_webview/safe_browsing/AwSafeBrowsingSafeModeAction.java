// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.safe_browsing;

import androidx.annotation.NonNull;

import org.chromium.android_webview.common.SafeModeAction;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

/**
 * A {@link SafeModeAction} to disable safe browsing.
 *
 * This action itself is a NOOP. The actual work is done in 2 places:
 * AwUrlCheckerDelegateImpl.ShouldSkipRequestCheck skips safe browsing checks for URL loads.
 * AwContentsStatics.initSafeBrowsing skips GMSCore communication in safe browsing initialization.
 */

@JNINamespace("android_webview")
public class AwSafeBrowsingSafeModeAction implements SafeModeAction {
    // This ID should not be changed or reused.
    public static final String ID = "disable_safe_browsing";

    private static boolean sSafeBrowsingDisabled;

    @Override
    @NonNull
    public String getId() {
        return ID;
    }

    @Override
    public boolean execute() {
        sSafeBrowsingDisabled = true;
        return true;
    }

    @CalledByNative
    public static boolean isSafeBrowsingDisabled() {
        return sSafeBrowsingDisabled;
    }
}
