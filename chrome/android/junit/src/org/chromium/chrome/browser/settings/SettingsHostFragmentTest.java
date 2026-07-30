// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import android.os.Bundle;

import androidx.fragment.app.Fragment;
import androidx.preference.Preference;
import androidx.preference.PreferenceFragmentCompat;
import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.base.TestActivity;

/** Unit tests for {@link SettingsHostFragment}. */
@RunWith(BaseRobolectricTestRunner.class)
public class SettingsHostFragmentTest {
    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarios =
            new ActivityScenarioRule<>(TestActivity.class);

    private TestActivity mActivity;
    private SettingsHostFragment mSettingsHostFragment;

    /** Subclass SettingsHostFragment to mock initial fragment instantiation. */
    public static class TestSettingsHostFragment extends SettingsHostFragment {
        @Override
        protected Fragment createInitialFragment() {
            return new FakeSettingsFragment();
        }
    }

    @Before
    public void setUp() {
        mActivityScenarios
                .getScenario()
                .onActivity(activity -> mActivity = (TestActivity) activity);

        mSettingsHostFragment = new TestSettingsHostFragment();
        mActivity
                .getSupportFragmentManager()
                .beginTransaction()
                .add(android.R.id.content, mSettingsHostFragment)
                .commitNow();
    }

    @Test
    public void testInitialFragmentAttached() {
        Fragment current =
                mSettingsHostFragment
                        .getChildFragmentManager()
                        .findFragmentById(mSettingsHostFragment.getView().getId());
        assertNotNull("Initial fragment should be attached", current);
        assertTrue(
                "Initial fragment should be FakeSettingsFragment",
                current instanceof FakeSettingsFragment);
    }

    @Test
    public void testOnPreferenceStartFragment() {
        Preference preference = mock(Preference.class);
        when(preference.getFragment()).thenReturn(SecondFakeSettingsFragment.class.getName());
        Bundle extras = new Bundle();
        extras.putString("test_key", "test_value");
        when(preference.getExtras()).thenReturn(extras);

        PreferenceFragmentCompat caller = mock(PreferenceFragmentCompat.class);
        boolean handled = mSettingsHostFragment.onPreferenceStartFragment(caller, preference);

        assertTrue("Preference start fragment should be handled", handled);
        mSettingsHostFragment.getChildFragmentManager().executePendingTransactions();

        Fragment current =
                mSettingsHostFragment
                        .getChildFragmentManager()
                        .findFragmentById(mSettingsHostFragment.getView().getId());
        assertNotNull("New fragment should be attached", current);
        assertTrue(
                "New fragment should be SecondFakeSettingsFragment",
                current instanceof SecondFakeSettingsFragment);
        assertEquals("test_value", current.getArguments().getString("test_key"));
        assertEquals(1, mSettingsHostFragment.getChildFragmentManager().getBackStackEntryCount());
    }

    /** Fake settings fragment for testing. */
    public static class FakeSettingsFragment extends Fragment {
        public FakeSettingsFragment() {}
    }

    /** Another fake settings fragment for testing transitions. */
    public static class SecondFakeSettingsFragment extends Fragment {
        public SecondFakeSettingsFragment() {}
    }
}
