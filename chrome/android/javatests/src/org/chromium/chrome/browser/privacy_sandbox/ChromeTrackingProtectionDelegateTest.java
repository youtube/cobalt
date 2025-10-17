// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import androidx.test.annotation.UiThreadTest;
import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.components.privacy_sandbox.TrackingProtectionDelegate;
import org.chromium.components.user_prefs.UserPrefs;

@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class ChromeTrackingProtectionDelegateTest {
    @ClassRule
    public static final ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public final BlankCTATabInitialStateRule mInitialStateRule =
            new BlankCTATabInitialStateRule(sActivityTestRule, false);

    private TrackingProtectionDelegate mDelegate;

    @Before
    public void setUp() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mDelegate =
                            new ChromeTrackingProtectionDelegate(
                                    ProfileManager.getLastUsedRegularProfile());
                });
    }

    @Test
    @UiThreadTest
    @MediumTest
    public void blockAll3pc() {
        UserPrefs.get(ProfileManager.getLastUsedRegularProfile())
                .setBoolean(Pref.BLOCK_ALL3PC_TOGGLE_ENABLED, false);
        assertFalse(mDelegate.isBlockAll3pcEnabled());
        mDelegate.setBlockAll3pc(true);
        assertTrue(mDelegate.isBlockAll3pcEnabled());
        mDelegate.setBlockAll3pc(false);
        assertFalse(mDelegate.isBlockAll3pcEnabled());
    }

    @Test
    @UiThreadTest
    @MediumTest
    public void doNotTrack() {
        UserPrefs.get(ProfileManager.getLastUsedRegularProfile())
                .setBoolean(Pref.ENABLE_DO_NOT_TRACK, false);
        assertFalse(mDelegate.isDoNotTrackEnabled());
        mDelegate.setDoNotTrack(true);
        assertTrue(mDelegate.isDoNotTrackEnabled());
        mDelegate.setDoNotTrack(false);
        assertFalse(mDelegate.isDoNotTrackEnabled());
    }

    @Test
    @UiThreadTest
    @MediumTest
    public void ipProtection() {
        UserPrefs.get(ProfileManager.getLastUsedRegularProfile())
                .setBoolean(Pref.IP_PROTECTION_ENABLED, false);
        assertFalse(mDelegate.isIpProtectionEnabled());
        mDelegate.setIpProtection(true);
        assertTrue(mDelegate.isIpProtectionEnabled());
        mDelegate.setIpProtection(false);
        assertFalse(mDelegate.isIpProtectionEnabled());
    }

    @Test
    @UiThreadTest
    @MediumTest
    public void fingerprintingProtection() {
        UserPrefs.get(ProfileManager.getLastUsedRegularProfile())
                .setBoolean(Pref.FINGERPRINTING_PROTECTION_ENABLED, false);
        assertFalse(mDelegate.isFingerprintingProtectionEnabled());
        mDelegate.setFingerprintingProtection(true);
        assertTrue(mDelegate.isFingerprintingProtectionEnabled());
        mDelegate.setFingerprintingProtection(false);
        assertFalse(mDelegate.isFingerprintingProtectionEnabled());
    }

    @Test
    @UiThreadTest
    @MediumTest
    public void contextGetter() {
        var context = mDelegate.getBrowserContext();
        assertEquals(ProfileManager.getLastUsedRegularProfile(), context);
    }
}
