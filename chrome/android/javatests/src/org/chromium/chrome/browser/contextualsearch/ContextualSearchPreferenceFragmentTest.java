// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextualsearch;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.UiThreadTest;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/**
 * Tests for {@link ContextualSearchPreferenceFragment}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
public class ContextualSearchPreferenceFragmentTest {
    @Rule
    public SettingsActivityTestRule<ContextualSearchPreferenceFragment> mSettingsActivityTestRule =
            new SettingsActivityTestRule<>(ContextualSearchPreferenceFragment.class);

    private ContextualSearchPreferenceFragment mSettings;
    private ChromeSwitchPreference mContextualSearchSwitchPreference;
    private ChromeSwitchPreference mSeeBetterResultsSwitchPreference;

    @Before
    public void setUp() {
        mSettingsActivityTestRule.startSettingsActivity();
        mSettings = mSettingsActivityTestRule.getFragment();
        mContextualSearchSwitchPreference = (ChromeSwitchPreference) mSettings.findPreference(
                ContextualSearchPreferenceFragment.PREF_CONTEXTUAL_SEARCH_SWITCH);
        mSeeBetterResultsSwitchPreference = (ChromeSwitchPreference) mSettings.findPreference(
                ContextualSearchPreferenceFragment.PREF_WAS_FULLY_ENABLED_SWITCH);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            PrefService prefService = UserPrefs.get(Profile.getLastUsedRegularProfile());
            prefService.clearPref(Pref.CONTEXTUAL_SEARCH_ENABLED);
            prefService.clearPref(Pref.CONTEXTUAL_SEARCH_WAS_FULLY_PRIVACY_ENABLED);
        });
    }

    @After
    public void tearDown() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            PrefService prefService = UserPrefs.get(Profile.getLastUsedRegularProfile());
            prefService.clearPref(Pref.CONTEXTUAL_SEARCH_ENABLED);
            prefService.clearPref(Pref.CONTEXTUAL_SEARCH_WAS_FULLY_PRIVACY_ENABLED);
        });
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testContextualSearchSwitch() {
        // Default is on.
        Assert.assertTrue("The Contextual Search default value for the switch should be on.",
                mContextualSearchSwitchPreference.isChecked());
        Assert.assertTrue("The Contextual Search default state should be uninitialized",
                ContextualSearchPolicy.isContextualSearchUninitialized());

        mContextualSearchSwitchPreference.performClick();
        Assert.assertFalse("The Contextual Search switch should be turned off.",
                mContextualSearchSwitchPreference.isChecked());
        Assert.assertTrue("The state should be disabled.",
                ContextualSearchPolicy.isContextualSearchDisabled());
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testSeeBetterResultsSwitch() {
        // "See Better Results" Switch is visible when Contextual Search Switch is on.
        Assert.assertTrue("The Contextual Search default value for the switch should be on.",
                mContextualSearchSwitchPreference.isChecked());
        Assert.assertTrue("See Better Results Switch should be shown by default.",
                mSeeBetterResultsSwitchPreference.isVisible());
        // Default is off.
        Assert.assertFalse("See Better Results Switch default value should be off.",
                mSeeBetterResultsSwitchPreference.isChecked());
        Assert.assertFalse("The Contextual Search default value should not be fully opted in.",
                ContextualSearchPolicy.isContextualSearchPrefFullyOptedIn());

        mSeeBetterResultsSwitchPreference.performClick();
        Assert.assertTrue("See Better Results Switch should be on.",
                mSeeBetterResultsSwitchPreference.isChecked());
        Assert.assertTrue("The Contextual Search default value should be fully opted in.",
                ContextualSearchPolicy.isContextualSearchPrefFullyOptedIn());

        mContextualSearchSwitchPreference.performClick();
        //"See Better Results" Switch is not visible when Contextual Search Switch is off.
        Assert.assertFalse("See Better Results Switch should not be shown.",
                mSeeBetterResultsSwitchPreference.isVisible());
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testSeeBetterResultsSwitch_SavePreviousOptInStatus() {
        // "See Better Results" Switch is visible when Contextual Search Switch is on.
        Assert.assertTrue("The Contextual Search default value for the switch should be on.",
                mContextualSearchSwitchPreference.isChecked());
        Assert.assertTrue("See Better Results Switch should be shown by default.",
                mSeeBetterResultsSwitchPreference.isVisible());
        // Default is off.
        Assert.assertFalse("See Better Results Switch default value should be off.",
                mSeeBetterResultsSwitchPreference.isChecked());
        Assert.assertFalse("The Contextual Search default value should not be fully opted in.",
                ContextualSearchPolicy.isContextualSearchPrefFullyOptedIn());

        mSeeBetterResultsSwitchPreference.performClick();
        Assert.assertTrue("See Better Results Switch should be on.",
                mSeeBetterResultsSwitchPreference.isChecked());
        Assert.assertTrue("The Contextual Search state should be fully opted in.",
                ContextualSearchPolicy.isContextualSearchPrefFullyOptedIn());

        mContextualSearchSwitchPreference.performClick();
        //"See Better Results" Switch is not visible when Contextual Search Switch is off.
        Assert.assertFalse("See Better Results Switch should not be shown.",
                mSeeBetterResultsSwitchPreference.isVisible());

        mContextualSearchSwitchPreference.performClick();
        //"See Better Results" Switch is visible and is on.
        Assert.assertTrue("See Better Results Switch should be shown.",
                mSeeBetterResultsSwitchPreference.isVisible());
        Assert.assertTrue("See Better Results Switch should be on since previously is on.",
                mSeeBetterResultsSwitchPreference.isChecked());
        Assert.assertTrue("The Contextual Search state should be fully opted in.",
                ContextualSearchPolicy.isContextualSearchPrefFullyOptedIn());
    }
}