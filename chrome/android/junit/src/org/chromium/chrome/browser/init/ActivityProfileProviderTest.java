// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.init;

import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;

import android.app.Activity;

import androidx.annotation.Nullable;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.chrome.browser.profiles.OtrProfileId;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.profiles.ProfileProvider;

/** Tests for ActivityProfileProviderInitializer. */
@RunWith(BaseRobolectricTestRunner.class)
public class ActivityProfileProviderTest {
    @Mock private Profile mOriginalProfile;
    @Mock private Activity mActivity;

    private TestActivityLifecycleDispatcherImpl mLifecycleDispatcher;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        ProfileManager.setLastUsedProfileForTesting(mOriginalProfile);
        mLifecycleDispatcher = new TestActivityLifecycleDispatcherImpl(mActivity);
    }

    @After
    public void tearDown() {
        ProfileManager.resetForTesting();
    }

    @Test
    public void testProfileManager_AlreadyInitialized() {
        Assert.assertFalse(ProfileManager.isInitialized());
        ProfileManager.onProfileAdded(mOriginalProfile);
        Assert.assertTrue(ProfileManager.isInitialized());

        ActivityProfileProvider provider = new ActivityProfileProvider(mLifecycleDispatcher);
        Assert.assertNotNull(provider.get());
        Assert.assertEquals(mOriginalProfile, provider.get().getOriginalProfile());
    }

    @Test
    public void testProfileManager_DeferredInitialization() {
        ActivityProfileProvider provider = new ActivityProfileProvider(mLifecycleDispatcher);
        Assert.assertFalse(ProfileManager.isInitialized());
        ProfileManager.onProfileAdded(mOriginalProfile);
        Assert.assertTrue(ProfileManager.isInitialized());

        Assert.assertNotNull(provider.get());
        Assert.assertEquals(mOriginalProfile, provider.get().getOriginalProfile());
    }

    @Test
    public void testDestroyedBeforeProfileManagerInitialized() {
        ActivityProfileProvider provider = new ActivityProfileProvider(mLifecycleDispatcher);
        Assert.assertFalse(ProfileManager.isInitialized());
        mLifecycleDispatcher.dispatchOnDestroy();

        ProfileManager.onProfileAdded(mOriginalProfile);
        Assert.assertTrue(ProfileManager.isInitialized());

        Assert.assertNull(provider.get());
    }

    @Test
    public void testIncognito_NoOtrProfileId() {
        ActivityProfileProvider providerSupplier =
                new ActivityProfileProvider(mLifecycleDispatcher);

        ProfileManager.onProfileAdded(mOriginalProfile);
        Assert.assertTrue(ProfileManager.isInitialized());

        ProfileProvider provider = providerSupplier.get();
        Assert.assertNotNull(provider);

        provider.hasOffTheRecordProfile();
        verify(mOriginalProfile).hasPrimaryOtrProfile();

        provider.getOffTheRecordProfile(false);
        verify(mOriginalProfile).getPrimaryOtrProfile(eq(false));

        provider.getOffTheRecordProfile(true);
        verify(mOriginalProfile).getPrimaryOtrProfile(eq(true));
    }

    @Test
    public void testIncognito_WithOtrProfileId() {
        OtrProfileId otrProfileId = new OtrProfileId("blah");
        CallbackHelper otrProfileIdHelper = new CallbackHelper();

        ActivityProfileProvider providerSupplier =
                new ActivityProfileProvider(mLifecycleDispatcher) {
                    @Nullable
                    @Override
                    protected OtrProfileId createOffTheRecordProfileId() {
                        otrProfileIdHelper.notifyCalled();
                        return otrProfileId;
                    }
                };

        ProfileManager.onProfileAdded(mOriginalProfile);
        Assert.assertTrue(ProfileManager.isInitialized());

        ProfileProvider provider = providerSupplier.get();
        Assert.assertNotNull(provider);

        provider.getOriginalProfile();
        Assert.assertEquals(0, otrProfileIdHelper.getCallCount());

        provider.hasOffTheRecordProfile();
        verify(mOriginalProfile).hasOffTheRecordProfile(eq(otrProfileId));

        provider.getOffTheRecordProfile(false);
        verify(mOriginalProfile).getOffTheRecordProfile(eq(otrProfileId), eq(false));

        provider.getOffTheRecordProfile(true);
        verify(mOriginalProfile).getOffTheRecordProfile(eq(otrProfileId), eq(true));
        Assert.assertEquals(1, otrProfileIdHelper.getCallCount());
    }

    private static class TestActivityLifecycleDispatcherImpl
            extends ActivityLifecycleDispatcherImpl {
        public TestActivityLifecycleDispatcherImpl(Activity activity) {
            super(activity);
        }

        @Override
        public void dispatchOnDestroy() {
            super.dispatchOnDestroy();
        }
    }
}
