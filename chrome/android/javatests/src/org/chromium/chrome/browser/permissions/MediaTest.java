// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.permissions;

import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.permissions.PermissionTestRule.PermissionUpdateWaiter;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.common.ContentSwitches;

/**
 * Test suite for media permissions requests.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class MediaTest {
    @Rule
    public PermissionTestRule mPermissionRule = new PermissionTestRule();

    private static final String FAKE_DEVICE = ContentSwitches.USE_FAKE_DEVICE_FOR_MEDIA_STREAM;
    private static final String TEST_FILE = "/content/test/data/android/media_permissions.html";

    public MediaTest() {}

    @Before
    public void setUp() throws Exception {
        mPermissionRule.setUpActivity();
    }

    private void testMediaPermissionsPlumbing(String prefix, String script, int numUpdates,
            boolean withGesture, boolean isDialog) throws Exception {
        Tab tab = mPermissionRule.getActivity().getActivityTab();
        PermissionUpdateWaiter updateWaiter =
                new PermissionUpdateWaiter(prefix, mPermissionRule.getActivity());
        TestThreadUtils.runOnUiThreadBlocking(() -> tab.addObserver(updateWaiter));
        mPermissionRule.runAllowTest(
                updateWaiter, TEST_FILE, script, numUpdates, withGesture, isDialog);
        TestThreadUtils.runOnUiThreadBlocking(() -> tab.removeObserver(updateWaiter));
    }

    /**
     * Verify asking for microphone creates a dialog and works when the permission is granted.
     * @throws Exception
     */
    @Test
    @MediumTest
    @Feature({"MediaPermissions", "Main"})
    @CommandLineFlags.Add({FAKE_DEVICE})
    public void testMicrophoneMediaPermissionsPlumbingDialog() throws Exception {
        testMediaPermissionsPlumbing("Mic count:", "initiate_getMicrophone()", 1, true, true);
    }

    /**
     * Verify asking for camera with no gesture creates a dialog and works when the permission is
     * granted.
     * @throws Exception
     */
    @Test
    @MediumTest
    @Feature({"MediaPermissions", "Main"})
    @CommandLineFlags.Add({FAKE_DEVICE})
    public void testCameraPermissionsPlumbingDialog() throws Exception {
        testMediaPermissionsPlumbing("Camera count:", "initiate_getCamera()", 1, false, true);
    }

    /**
     * Verify asking for both mic and camera creates a combined dialog and works when the
     * permissions are granted.
     * @throws Exception
     */
    @Test
    @MediumTest
    @Feature({"MediaPermissions", "Main"})
    @CommandLineFlags.Add({FAKE_DEVICE})
    public void testCombinedPermissionsPlumbingDialog() throws Exception {
        testMediaPermissionsPlumbing("Combined count:", "initiate_getCombined()", 1, true, true);
    }
}
