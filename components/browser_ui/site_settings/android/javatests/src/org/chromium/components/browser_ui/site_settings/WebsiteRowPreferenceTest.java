// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.site_settings;

import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.view.LayoutInflater;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.JniMocker;
import org.chromium.components.browser_ui.settings.BlankUiTestActivitySettingsTestRule;
import org.chromium.components.browser_ui.settings.PlaceholderSettingsForTest;

import java.util.Arrays;

/** Tests for WebsiteRowPreference. */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class WebsiteRowPreferenceTest {
    @Rule
    public final BlankUiTestActivitySettingsTestRule mSettingsRule =
            new BlankUiTestActivitySettingsTestRule();

    @Rule public JniMocker mJniMocker = new JniMocker();

    @Mock private WebsitePreferenceBridge.Natives mBridgeMock;

    private WebsiteRowPreference mPreference;
    private Activity mActivity;

    @Mock private SiteSettingsDelegate mDelegate;

    @Mock private Runnable mOnDeleteCallback;

    @BeforeClass
    public static void setupSuite() {
        LibraryLoader.getInstance().setLibraryProcessType(LibraryProcessType.PROCESS_BROWSER);
        LibraryLoader.getInstance().ensureInitialized();
    }

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mJniMocker.mock(WebsitePreferenceBridgeJni.TEST_HOOKS, mBridgeMock);
        mSettingsRule.launchPreference(PlaceholderSettingsForTest.class);
        mActivity = mSettingsRule.getActivity();
    }

    @Test
    @SmallTest
    public void testClearCallbackWebsite() {
        Website website = new Website(WebsiteAddress.create("https://test.com"), null);
        mPreference =
                new WebsiteRowPreference(
                        mActivity, mDelegate, website, LayoutInflater.from(mActivity));
        mPreference.setOnDeleteCallback(mOnDeleteCallback);
        mPreference.resetEntry();
        verify(mOnDeleteCallback).run();
    }

    @Test
    @SmallTest
    public void testClearCallbackWebsiteGroup() {
        Website origin1 = new Website(WebsiteAddress.create("https://one.test.com"), null);
        Website origin2 = new Website(WebsiteAddress.create("https://two.test.com"), null);
        WebsiteGroup group =
                new WebsiteGroup(
                        origin1.getAddress().getDomainAndRegistry(),
                        Arrays.asList(origin1, origin2));
        mPreference =
                new WebsiteRowPreference(
                        mActivity, mDelegate, group, LayoutInflater.from(mActivity));
        mPreference.setOnDeleteCallback(mOnDeleteCallback);
        mPreference.resetEntry();
        verify(mOnDeleteCallback).run();
    }
}
