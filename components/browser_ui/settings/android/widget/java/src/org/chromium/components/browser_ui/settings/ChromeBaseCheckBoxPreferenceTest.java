// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.settings;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.isEnabled;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.allOf;
import static org.hamcrest.Matchers.not;
import static org.hamcrest.Matchers.stringContainsInOrder;

import android.app.Activity;

import androidx.preference.PreferenceFragmentCompat;
import androidx.preference.PreferenceScreen;
import androidx.test.filters.LargeTest;

import com.google.common.collect.ImmutableList;

import org.junit.Assert;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.components.browser_ui.settings.test.R;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.DisableAnimationsTestRule;

/** Tests of {@link ChromeBaseCheckBoxPreference}. */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class ChromeBaseCheckBoxPreferenceTest {
    @ClassRule
    public static final DisableAnimationsTestRule disableAnimationsRule =
            new DisableAnimationsTestRule();

    @Rule
    public final BlankUiTestActivitySettingsTestRule mSettingsRule =
            new BlankUiTestActivitySettingsTestRule();

    private static final String TITLE = "Preference Title";
    private static final String SUMMARY = "This is a summary.";

    private static final String CUSTOM_LAYOUT_PREF_NAME = "preference_with_custom_layout";

    private Activity mActivity;
    private PreferenceScreen mPreferenceScreen;

    @Before
    public void setUp() {
        mSettingsRule.launchPreference(PlaceholderSettingsForTest.class);
        mActivity = mSettingsRule.getActivity();
        mPreferenceScreen = mSettingsRule.getPreferenceScreen();
    }

    @Test
    @LargeTest
    public void testUnmanagedPreference() {
        ChromeBaseCheckBoxPreference preference = new ChromeBaseCheckBoxPreference(mActivity);
        preference.setTitle(TITLE);
        preference.setSummary(SUMMARY);
        preference.setManagedPreferenceDelegate(ManagedPreferenceTestDelegates.UNMANAGED_DELEGATE);
        mPreferenceScreen.addPreference(preference);

        Assert.assertTrue(preference.isEnabled());

        onView(withId(android.R.id.title)).check(matches(allOf(withText(TITLE), isDisplayed())));
        onView(withId(android.R.id.summary))
                .check(matches(allOf(withText(SUMMARY), isDisplayed())));
        // Unmanaged preferences do not use {@code chrome_managed_preference}, so a disclaimer text
        // view does not exist.
        onView(withId(R.id.managed_disclaimer_text)).check(doesNotExist());
        onView(withId(android.R.id.icon)).check(matches(not(isDisplayed())));
        onView(withId(android.R.id.checkbox)).check(matches(allOf(isEnabled(), isDisplayed())));
    }

    @Test
    @LargeTest
    public void testPolicyManagedPreferenceWithoutSummary() {
        ChromeBaseCheckBoxPreference preference = new ChromeBaseCheckBoxPreference(mActivity);
        preference.setTitle(TITLE);
        preference.setManagedPreferenceDelegate(ManagedPreferenceTestDelegates.POLICY_DELEGATE);
        mPreferenceScreen.addPreference(preference);

        Assert.assertFalse(preference.isEnabled());

        onView(withId(android.R.id.title)).check(matches(allOf(withText(TITLE), isDisplayed())));
        onView(withId(android.R.id.summary)).check(matches(not(isDisplayed())));
        onView(withId(R.id.managed_disclaimer_text))
                .check(
                        matches(
                                allOf(
                                        withText(R.string.managed_by_your_organization),
                                        Matchers.hasDrawableStart(),
                                        isDisplayed())));
        onView(withId(android.R.id.icon)).check(matches(not(isDisplayed())));
        onView(withId(android.R.id.checkbox))
                .check(matches(allOf(not(isEnabled()), isDisplayed())));
    }

    @Test
    @LargeTest
    public void testPolicyManagedPreferenceWithSummary() {
        ChromeBaseCheckBoxPreference preference = new ChromeBaseCheckBoxPreference(mActivity);
        preference.setTitle(TITLE);
        preference.setSummary(SUMMARY);
        preference.setManagedPreferenceDelegate(ManagedPreferenceTestDelegates.POLICY_DELEGATE);
        mPreferenceScreen.addPreference(preference);

        Assert.assertFalse(preference.isEnabled());

        onView(withId(android.R.id.title)).check(matches(allOf(withText(TITLE), isDisplayed())));
        onView(withId(android.R.id.summary))
                .check(matches(allOf(withText(SUMMARY), isDisplayed())));
        onView(withId(R.id.managed_disclaimer_text))
                .check(
                        matches(
                                allOf(
                                        withText(R.string.managed_by_your_organization),
                                        Matchers.hasDrawableStart(),
                                        isDisplayed())));
        onView(withId(android.R.id.icon)).check(matches(not(isDisplayed())));
        onView(withId(android.R.id.checkbox))
                .check(matches(allOf(not(isEnabled()), isDisplayed())));
    }

    @Test
    @LargeTest
    public void testSingleCustodianManagedPreference() {
        ChromeBaseCheckBoxPreference preference = new ChromeBaseCheckBoxPreference(mActivity);
        preference.setTitle(TITLE);
        preference.setManagedPreferenceDelegate(
                ManagedPreferenceTestDelegates.SINGLE_CUSTODIAN_DELEGATE);
        mPreferenceScreen.addPreference(preference);

        Assert.assertFalse(preference.isEnabled());

        onView(withId(android.R.id.title)).check(matches(allOf(withText(TITLE), isDisplayed())));
        onView(withId(android.R.id.summary))
                .check(matches(allOf(withText(R.string.managed_by_your_parent), isDisplayed())));
        onView(withId(R.id.managed_disclaimer_text)).check(doesNotExist());
        onView(withId(android.R.id.icon)).check(matches(isDisplayed()));
        onView(withId(android.R.id.checkbox))
                .check(matches(allOf(not(isEnabled()), isDisplayed())));
    }

    @Test
    @LargeTest
    public void testMultipleCustodianManagedPreference() {
        ChromeBaseCheckBoxPreference preference = new ChromeBaseCheckBoxPreference(mActivity);
        preference.setTitle(TITLE);
        preference.setManagedPreferenceDelegate(
                ManagedPreferenceTestDelegates.MULTI_CUSTODIAN_DELEGATE);
        mPreferenceScreen.addPreference(preference);

        Assert.assertFalse(preference.isEnabled());

        onView(withId(android.R.id.title)).check(matches(allOf(withText(TITLE), isDisplayed())));
        onView(withId(android.R.id.summary))
                .check(matches(allOf(withText(R.string.managed_by_your_parents), isDisplayed())));
        onView(withId(R.id.managed_disclaimer_text)).check(doesNotExist());
        onView(withId(android.R.id.icon)).check(matches(isDisplayed()));
        onView(withId(android.R.id.checkbox))
                .check(matches(allOf(not(isEnabled()), isDisplayed())));
    }

    @Test
    @LargeTest
    public void testUnmanagedPreferenceWithCustomLayout() throws Exception {
        PreferenceFragmentCompat fragment = mSettingsRule.getPreferenceFragment();
        SettingsUtils.addPreferencesFromResource(
                fragment, R.xml.test_chrome_base_checkbox_preference_screen);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ChromeBaseCheckBoxPreference preference =
                            fragment.findPreference(CUSTOM_LAYOUT_PREF_NAME);
                    preference.setTitle(TITLE);
                    preference.setSummary(SUMMARY);
                    preference.setManagedPreferenceDelegate(
                            ManagedPreferenceTestDelegates.UNMANAGED_DELEGATE);
                });

        ChromeBaseCheckBoxPreference preference = fragment.findPreference(CUSTOM_LAYOUT_PREF_NAME);
        Assert.assertEquals(
                preference.getLayoutResource(),
                R.layout.chrome_managed_preference_with_custom_layout);
        Assert.assertTrue(preference.isEnabled());
        onView(withId(android.R.id.title)).check(matches(allOf(withText(TITLE), isDisplayed())));
        onView(withId(android.R.id.summary))
                .check(matches(allOf(withText(SUMMARY), isDisplayed())));
        onView(withId(R.id.managed_disclaimer_text)).check(doesNotExist());
        onView(withId(android.R.id.icon)).check(matches(not(isDisplayed())));
        onView(withId(android.R.id.checkbox)).check(matches(allOf(isEnabled(), isDisplayed())));
    }

    @Test
    @LargeTest
    public void testPolicyManagedPreferenceWithSummaryAndCustomLayout() {
        PreferenceFragmentCompat fragment = mSettingsRule.getPreferenceFragment();
        SettingsUtils.addPreferencesFromResource(
                fragment, R.xml.test_chrome_base_checkbox_preference_screen);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ChromeBaseCheckBoxPreference preference =
                            fragment.findPreference(CUSTOM_LAYOUT_PREF_NAME);
                    preference.setTitle(TITLE);
                    preference.setSummary(SUMMARY);
                    preference.setManagedPreferenceDelegate(
                            ManagedPreferenceTestDelegates.POLICY_DELEGATE);
                });

        ChromeBaseCheckBoxPreference preference = fragment.findPreference(CUSTOM_LAYOUT_PREF_NAME);
        Assert.assertEquals(
                preference.getLayoutResource(),
                R.layout.chrome_managed_preference_with_custom_layout);
        Assert.assertFalse(preference.isEnabled());
        onView(withId(android.R.id.title)).check(matches(allOf(withText(TITLE), isDisplayed())));
        onView(withId(android.R.id.summary))
                .check(
                        matches(
                                allOf(
                                        withText(
                                                stringContainsInOrder(
                                                        ImmutableList.of(
                                                                SUMMARY,
                                                                mActivity.getString(
                                                                        R.string
                                                                                .managed_by_your_organization)))),
                                        isDisplayed())));
        onView(withId(R.id.managed_disclaimer_text)).check(doesNotExist());
        onView(withId(android.R.id.icon)).check(matches(isDisplayed()));
        onView(withId(android.R.id.checkbox))
                .check(matches(allOf(not(isEnabled()), isDisplayed())));
    }

    @Test
    @LargeTest
    public void testPolicyManagedPreferenceWithoutSummaryWithCustomLayout() {
        PreferenceFragmentCompat fragment = mSettingsRule.getPreferenceFragment();
        SettingsUtils.addPreferencesFromResource(
                fragment, R.xml.test_chrome_base_checkbox_preference_screen);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ChromeBaseCheckBoxPreference preference =
                            fragment.findPreference(CUSTOM_LAYOUT_PREF_NAME);
                    preference.setTitle(TITLE);
                    preference.setManagedPreferenceDelegate(
                            ManagedPreferenceTestDelegates.POLICY_DELEGATE);
                });

        ChromeBaseCheckBoxPreference preference = fragment.findPreference(CUSTOM_LAYOUT_PREF_NAME);
        Assert.assertEquals(
                preference.getLayoutResource(),
                R.layout.chrome_managed_preference_with_custom_layout);
        Assert.assertFalse(preference.isEnabled());
        onView(withId(android.R.id.title)).check(matches(allOf(withText(TITLE), isDisplayed())));
        onView(withId(android.R.id.summary))
                .check(
                        matches(
                                allOf(
                                        withText(R.string.managed_by_your_organization),
                                        isDisplayed())));
        onView(withId(R.id.managed_disclaimer_text)).check(doesNotExist());
        onView(withId(android.R.id.icon)).check(matches(isDisplayed()));
        onView(withId(android.R.id.checkbox))
                .check(matches(allOf(not(isEnabled()), isDisplayed())));
    }

    @Test
    @LargeTest
    public void testSingleCustodianManagedPreferenceWithCustomLayout() {
        PreferenceFragmentCompat fragment = mSettingsRule.getPreferenceFragment();
        SettingsUtils.addPreferencesFromResource(
                fragment, R.xml.test_chrome_base_checkbox_preference_screen);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ChromeBaseCheckBoxPreference preference =
                            fragment.findPreference(CUSTOM_LAYOUT_PREF_NAME);
                    preference.setTitle(TITLE);
                    preference.setManagedPreferenceDelegate(
                            ManagedPreferenceTestDelegates.SINGLE_CUSTODIAN_DELEGATE);
                });

        ChromeBaseCheckBoxPreference preference = fragment.findPreference(CUSTOM_LAYOUT_PREF_NAME);
        Assert.assertEquals(
                preference.getLayoutResource(),
                R.layout.chrome_managed_preference_with_custom_layout);
        Assert.assertFalse(preference.isEnabled());
        onView(withId(android.R.id.title)).check(matches(allOf(withText(TITLE), isDisplayed())));
        onView(withId(android.R.id.summary))
                .check(matches(allOf(withText(R.string.managed_by_your_parent), isDisplayed())));
        onView(withId(R.id.managed_disclaimer_text)).check(doesNotExist());
        onView(withId(android.R.id.icon)).check(matches(isDisplayed()));
        onView(withId(android.R.id.checkbox))
                .check(matches(allOf(not(isEnabled()), isDisplayed())));
    }
}
