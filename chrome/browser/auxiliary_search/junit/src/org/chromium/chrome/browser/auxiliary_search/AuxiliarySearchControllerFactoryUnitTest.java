// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.auxiliary_search;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.content.res.Resources;
import android.os.Build.VERSION_CODES;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper;
import org.chromium.chrome.browser.ui.favicon.FaviconHelperJni;

/** Unit tests for {@link AuxiliarySearchControllerFactory}. */
@RunWith(BaseRobolectricTestRunner.class)
public class AuxiliarySearchControllerFactoryUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Context mContext;
    @Mock private Resources mResources;
    @Mock private Profile mProfile;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private AuxiliarySearchBridge.Natives mMockAuxiliarySearchBridgeJni;
    @Mock private FaviconHelper.Natives mMockFaviconHelperJni;
    @Mock private AuxiliarySearchHooks mHooks;

    private AuxiliarySearchControllerFactory mFactory;

    @Before
    public void setUp() {
        ProfileManager.setLastUsedProfileForTesting(mProfile);
        when(mContext.getResources()).thenReturn(mResources);

        AuxiliarySearchBridgeJni.setInstanceForTesting(mMockAuxiliarySearchBridgeJni);
        when(mMockFaviconHelperJni.init()).thenReturn(1L);
        FaviconHelperJni.setInstanceForTesting(mMockFaviconHelperJni);
        AuxiliarySearchDonor.setSkipInitializationForTesting(true);

        mFactory = AuxiliarySearchControllerFactory.getInstance();
        mFactory.setHooksForTesting(mHooks);
    }

    @Test
    @SmallTest
    public void testIsEnabled() {
        mFactory.setHooksForTesting(null);
        assertFalse(mFactory.isEnabled());

        when(mHooks.isEnabled()).thenReturn(false);
        mFactory.setHooksForTesting(mHooks);
        assertFalse(mFactory.isEnabled());

        when(mHooks.isEnabled()).thenReturn(true);
        assertTrue(mFactory.isEnabled());
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.ANDROID_APP_INTEGRATION_V2)
    @Config(sdk = VERSION_CODES.S)
    public void testCreateAuxiliarySearchController() {
        when(mHooks.isEnabled()).thenReturn(false);
        assertFalse(mFactory.isEnabled());
        assertNull(mFactory.createAuxiliarySearchController(mContext, mProfile, mTabModelSelector));

        when(mHooks.isEnabled()).thenReturn(true);
        assertTrue(mFactory.isEnabled());
        when(mProfile.isOffTheRecord()).thenReturn(false);

        AuxiliarySearchController controller =
                mFactory.createAuxiliarySearchController(mContext, mProfile, mTabModelSelector);
        assertTrue(controller instanceof AuxiliarySearchControllerImpl);
    }

    @Test
    @SmallTest
    public void testIsSettingDefaultEnabledByOs() {
        when(mHooks.isEnabled()).thenReturn(false);
        when(mHooks.isSettingDefaultEnabledByOs()).thenReturn(true);

        assertTrue(mFactory.isSettingDefaultEnabledByOs());

        when(mHooks.isSettingDefaultEnabledByOs()).thenReturn(false);
        assertFalse(mFactory.isSettingDefaultEnabledByOs());
    }

    @Test
    @SmallTest
    public void testSetIsTablet() {
        mFactory.resetIsTabletForTesting();
        mFactory.setIsTablet(false);
        assertFalse(mFactory.isTablet());

        mFactory.setIsTablet(true);
        assertTrue(mFactory.isTablet());

        // Verifies the isTablet() never goes from true to false.
        mFactory.setIsTablet(false);
        assertTrue(mFactory.isTablet());
    }

    @Test
    @SmallTest
    public void testGetSupportedPackageName() {
        String packageName = "name";
        when(mHooks.getSupportedPackageName()).thenReturn(packageName);

        assertEquals(packageName, mFactory.getSupportedPackageName());

        mFactory.setHooksForTesting(null);
        assertNull(mFactory.getSupportedPackageName());
    }
}
