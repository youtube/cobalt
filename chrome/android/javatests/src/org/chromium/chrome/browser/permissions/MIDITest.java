// Copyright 2020 The Chromium Authors
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

/**
 * Test suite for MIDI permissions requests.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class MIDITest {
    @Rule
    public PermissionTestRule mPermissionRule = new PermissionTestRule(true /* useHttpsServer */);

    private static final String TEST_FILE = "/content/test/data/android/midi_permissions.html";

    @Before
    public void setUp() throws Exception {
        mPermissionRule.setUpActivity();
    }

    @Test
    @MediumTest
    @Feature({"MIDI"})
    public void testMIDIDialog() throws Exception {
        Tab tab = mPermissionRule.getActivity().getActivityTab();
        PermissionUpdateWaiter updateWaiter =
                new PermissionUpdateWaiter("pass", mPermissionRule.getActivity());
        TestThreadUtils.runOnUiThreadBlocking(() -> tab.addObserver(updateWaiter));
        mPermissionRule.runAllowTest(updateWaiter, TEST_FILE, "", 0, false, true);
        TestThreadUtils.runOnUiThreadBlocking(() -> tab.removeObserver(updateWaiter));
    }
}
