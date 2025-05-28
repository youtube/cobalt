// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_guide;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.os.Bundle;

import androidx.annotation.NonNull;
import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentFactory;
import androidx.fragment.app.testing.FragmentScenario;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.UnifiedConsentServiceBridge;
import org.chromium.chrome.browser.signin.services.UnifiedConsentServiceBridgeJni;
import org.chromium.components.browser_ui.widget.MaterialSwitchWithText;

/** JUnit tests of the class {@link MSBBFragment}. */
@RunWith(BaseRobolectricTestRunner.class)
public class MSBBFragmentTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Profile mProfile;
    @Mock private UnifiedConsentServiceBridge.Natives mNativeMock;

    private FragmentScenario mScenario;
    private MaterialSwitchWithText mMSBBButton;
    private final UserActionTester mActionTester = new UserActionTester();

    @Before
    public void setUp() {
        UnifiedConsentServiceBridgeJni.setInstanceForTesting(mNativeMock);
    }

    @After
    public void tearDown() {
        if (mScenario != null) {
            mScenario.close();
        }
        mActionTester.tearDown();
    }

    private void initFragmentWithMSBBState(boolean isMSBBOn) {
        Mockito.when(mNativeMock.isUrlKeyedAnonymizedDataCollectionEnabled(mProfile))
                .thenReturn(isMSBBOn);
        mScenario =
                FragmentScenario.launchInContainer(
                        MSBBFragment.class,
                        Bundle.EMPTY,
                        R.style.Theme_MaterialComponents,
                        new FragmentFactory() {
                            @NonNull
                            @Override
                            public Fragment instantiate(
                                    @NonNull ClassLoader classLoader, @NonNull String className) {
                                Fragment fragment = super.instantiate(classLoader, className);
                                if (fragment instanceof MSBBFragment) {
                                    ((MSBBFragment) fragment).setProfile(mProfile);
                                }
                                return fragment;
                            }
                        });
        mScenario.onFragment(
                fragment -> mMSBBButton = fragment.getView().findViewById(R.id.msbb_switch));
    }

    @Test
    public void testIsSwitchOffWhenMSBBOff() {
        initFragmentWithMSBBState(false);
        assertFalse(mMSBBButton.isChecked());
    }

    @Test
    public void testIsSwitchOnWhenMSBBOn() {
        initFragmentWithMSBBState(true);
        assertTrue(mMSBBButton.isChecked());
    }

    @Test
    public void testTurnMSBBOn() {
        initFragmentWithMSBBState(false);
        mMSBBButton.performClick();
        Mockito.verify(mNativeMock).setUrlKeyedAnonymizedDataCollectionEnabled(mProfile, true);
    }

    @Test
    public void testTurnMSBBOff() {
        initFragmentWithMSBBState(true);
        mMSBBButton.performClick();
        Mockito.verify(mNativeMock).setUrlKeyedAnonymizedDataCollectionEnabled(mProfile, false);
    }

    @Test
    public void testTurnMSBBOff_changeMSBBOffUserAction() {
        initFragmentWithMSBBState(true);
        mMSBBButton.performClick();
        assertTrue(mActionTester.getActions().contains("Settings.PrivacyGuide.ChangeMSBBOff"));
    }

    @Test
    public void testTurnMSBBOn_changeMSBBOnUserAction() {
        initFragmentWithMSBBState(false);
        mMSBBButton.performClick();
        assertTrue(mActionTester.getActions().contains("Settings.PrivacyGuide.ChangeMSBBOn"));
    }
}
