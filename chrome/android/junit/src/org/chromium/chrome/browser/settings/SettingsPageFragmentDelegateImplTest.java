// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.ViewGroup;

import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentActivity;
import androidx.fragment.app.FragmentManager;
import androidx.fragment.app.FragmentTransaction;
import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.base.ActivityResultTracker;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManager;

/** Unit tests for {@link SettingsPageFragmentDelegateImpl}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures(ChromeFeatureList.SETTINGS_IN_TAB)
public class SettingsPageFragmentDelegateImplTest {
    private static final int CONTAINER_ID = R.id.content;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private FragmentActivity mActivity;
    @Mock private FragmentManager mFragmentManager;
    @Mock private FragmentTransaction mFragmentTransaction;
    @Mock private Profile mProfile;
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private ActivityResultTracker mActivityResultTracker;
    @Mock private SnackbarManager mSnackbarManager;
    @Mock private BottomSheetController mBottomSheetController;
    @Mock private ModalDialogManager mModalDialogManager;
    @Mock private ViewGroup mContainerView;
    @Mock private SettingsHostFragment mMockSettingsHostFragment;

    private SettingsPageFragmentDelegateImpl mDelegate;

    @Before
    public void setUp() {
        when(mActivity.getSupportFragmentManager()).thenReturn(mFragmentManager);
        when(mFragmentManager.beginTransaction()).thenReturn(mFragmentTransaction);
        when(mFragmentTransaction.add(anyInt(), any(Fragment.class), anyString()))
                .thenReturn(mFragmentTransaction);
        when(mFragmentTransaction.remove(any(Fragment.class))).thenReturn(mFragmentTransaction);
        when(mContainerView.getId()).thenReturn(CONTAINER_ID);

        // Mock LayoutInflater with correct theme to support inflating settings_activity.
        Context context =
                new android.view.ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        R.style.Theme_Chromium_Settings);
        LayoutInflater layoutInflater = LayoutInflater.from(context);
        when(mActivity.getSystemService(Context.LAYOUT_INFLATER_SERVICE))
                .thenReturn(layoutInflater);

        mDelegate =
                new SettingsPageFragmentDelegateImpl(
                        mActivity,
                        mProfile,
                        mWindowAndroid,
                        mActivityResultTracker,
                        mSnackbarManager,
                        mBottomSheetController,
                        mModalDialogManager);
    }

    @Test
    public void testInitSettings_registersDependencyProviderAndAddsFragment() {
        when(mFragmentManager.findFragmentByTag("settings_native_page")).thenReturn(null);

        mDelegate.initSettings(mContainerView);

        // Verify FragmentDependencyProvider registration.
        ArgumentCaptor<FragmentManager.FragmentLifecycleCallbacks> callbackCaptor =
                ArgumentCaptor.forClass(FragmentManager.FragmentLifecycleCallbacks.class);
        verify(mFragmentManager)
                .registerFragmentLifecycleCallbacks(callbackCaptor.capture(), eq(true));
        assertTrue(
                "Lifecycle callbacks should be FragmentDependencyProvider",
                callbackCaptor.getValue() instanceof FragmentDependencyProvider);

        // Verify fragment creation and addition.
        verify(mFragmentTransaction)
                .add(eq(CONTAINER_ID), any(SettingsHostFragment.class), eq("settings_native_page"));
        verify(mFragmentTransaction).commitAllowingStateLoss();
    }

    @Test
    public void testInitSettings_reusesExistingFragment() {
        when(mFragmentManager.findFragmentByTag("settings_native_page"))
                .thenReturn(mMockSettingsHostFragment);

        mDelegate.initSettings(mContainerView);

        // Verify we registered the callback but did NOT add a new fragment
        verify(mFragmentManager).registerFragmentLifecycleCallbacks(any(), eq(true));
        verify(mFragmentTransaction, never()).add(anyInt(), any(), anyString());
    }

    @Test
    public void testDestroySettings_unregistersCallbacksAndRemovesFragment() {
        when(mFragmentManager.findFragmentByTag("settings_native_page")).thenReturn(null);

        // Initialize first so the delegate has callbacks and fragment references.
        mDelegate.initSettings(mContainerView);

        // Retrieve the registered callback to verify it gets unregistered.
        ArgumentCaptor<FragmentManager.FragmentLifecycleCallbacks> callbackCaptor =
                ArgumentCaptor.forClass(FragmentManager.FragmentLifecycleCallbacks.class);
        verify(mFragmentManager)
                .registerFragmentLifecycleCallbacks(callbackCaptor.capture(), eq(true));
        FragmentManager.FragmentLifecycleCallbacks registeredCallback = callbackCaptor.getValue();

        mDelegate.destroySettings();

        // Verify unregistration.
        verify(mFragmentManager).unregisterFragmentLifecycleCallbacks(registeredCallback);

        // Verify fragment removal.
        verify(mFragmentTransaction).remove(any(SettingsHostFragment.class));

        // Verify commits, 1 for init, 1 for destroy.
        verify(mFragmentTransaction, Mockito.times(2)).commitAllowingStateLoss();
    }
}
