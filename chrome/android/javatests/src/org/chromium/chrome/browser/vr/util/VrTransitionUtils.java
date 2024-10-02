// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr.util;

import static org.chromium.chrome.browser.vr.XrTestFramework.POLL_CHECK_INTERVAL_SHORT_MS;

import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.chrome.browser.vr.VrShellDelegate;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/**
 * Class containing utility functions for transitioning between different
 * states in VR, such as fullscreen, WebVR presentation, and the VR browser.
 *
 * Methods in this class are applicable to any form of VR, e.g. they will work for both WebXR for VR
 * and the VR Browser.
 *
 * All the transitions in this class are performed directly through Chrome,
 * as opposed to NFC tag simulation which involves receiving an intent from
 * an outside application (VR Services).
 */
public class VrTransitionUtils {
    /**
     * Forces Chrome out of VR mode.
     */
    public static void forceExitVr() {
        TestThreadUtils.runOnUiThreadBlocking(() -> { VrShellDelegate.forceExitVrImmediately(); });
    }

    /**
     * Waits until Chrome believes it is in VR.
     *
     * @param timeout How long to wait for VR entry before timing out and failing.
     */
    public static void waitForVrEntry(final int timeout) {
        // Relatively long timeout because sometimes GVR takes a while to enter VR
        CriteriaHelper.pollUiThread(() -> {
            return VrShellDelegateUtils.getDelegateInstance().isVrEntryComplete();
        }, "VR not entered in allotted time", timeout, POLL_CHECK_INTERVAL_SHORT_MS);
    }
}
