// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabbed_mode;

import static org.hamcrest.Matchers.is;
import static org.hamcrest.Matchers.notNullValue;
import static org.hamcrest.Matchers.nullValue;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertThat;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import androidx.annotation.Nullable;
import androidx.test.annotation.UiThreadTest;
import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.fullscreen.BrowserControlsManagerSupplier;
import org.chromium.chrome.browser.privacy_sandbox.ActivityTypeMapper;
import org.chromium.chrome.browser.privacy_sandbox.PrivacySandboxBridgeJni;
import org.chromium.chrome.test.ChromeBrowserTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.components.search_engines.SearchEngineChoiceService;
import org.chromium.ui.base.DeviceFormFactor;

/** Tests for {@link TabbedRootUiCoordinator}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class TabbedRootUiCoordinatorTest {
    @Rule public ChromeBrowserTestRule mBrowserTestRule = new ChromeBrowserTestRule();

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule public MockitoRule mockito = MockitoJUnit.rule();

    private TabbedRootUiCoordinator mTabbedRootUiCoordinator;

    @Mock private PrivacySandboxBridgeJni mPrivacySandboxBridgeJni;
    @Mock private SearchEngineChoiceService mSearchEngineChoiceService;

    @Before
    public void setUp() {
        PrivacySandboxBridgeJni.setInstanceForTesting(mPrivacySandboxBridgeJni);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    SearchEngineChoiceService.setInstanceForTests(mSearchEngineChoiceService);
                    doReturn(false).when(mSearchEngineChoiceService).isDeviceChoiceDialogEligible();
                });

        TabbedRootUiCoordinator.setDisableTopControlsAnimationsForTesting(true);
        mActivityTestRule.startMainActivityOnBlankPage();
        mTabbedRootUiCoordinator =
                (TabbedRootUiCoordinator)
                        mActivityTestRule.getActivity().getRootUiCoordinatorForTesting();
    }

    // TODO(crbug.com/40112282): Enable for tablets once we support them.
    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.PRIVACY_SANDBOX_ACTIVITY_TYPE_STORAGE)
    @Restriction({DeviceFormFactor.PHONE})
    public void testRecordPrivacySandboxActivityTypeIncrementsRecordWhenFlagIsEnabled() {
        verify(mPrivacySandboxBridgeJni)
                .recordActivityType(
                        any(),
                        eq(
                                ActivityTypeMapper.toPrivacySandboxStorageActivityType(
                                        ActivityType.TABBED)));
    }

    // TODO(crbug.com/40112282): Enable for tablets once we support them.
    @Test
    @MediumTest
    @DisableFeatures(ChromeFeatureList.PRIVACY_SANDBOX_ACTIVITY_TYPE_STORAGE)
    @Restriction({DeviceFormFactor.PHONE})
    public void testRecordPrivacySandboxActivityTypeDoesNotIncrementRecordWhenFlagIsDisabled() {
        verify(mPrivacySandboxBridgeJni, never()).recordActivityType(any(), anyInt());
    }

    @Test
    @MediumTest
    @UiThreadTest
    @DisableFeatures(ChromeFeatureList.ANDROID_BOOKMARK_BAR)
    @Restriction({DeviceFormFactor.PHONE})
    public void testTopControlsHeightWithBookmarkBarWhenFlagIsDisabledOnPhone() {
        testTopControlsHeightWithBookmarkBar(/* expectBookmarkBar= */ false);
    }

    @Test
    @MediumTest
    @UiThreadTest
    @DisableFeatures(ChromeFeatureList.ANDROID_BOOKMARK_BAR)
    @Restriction({DeviceFormFactor.TABLET})
    public void testTopControlsHeightWithBookmarkBarWhenFlagIsDisabledOnTablet() {
        testTopControlsHeightWithBookmarkBar(/* expectBookmarkBar= */ false);
    }

    @Test
    @MediumTest
    @UiThreadTest
    @EnableFeatures(ChromeFeatureList.ANDROID_BOOKMARK_BAR)
    @Restriction({DeviceFormFactor.PHONE})
    public void testTopControlsHeightWithBookmarkBarWhenFlagIsEnabledOnPhone() {
        testTopControlsHeightWithBookmarkBar(/* expectBookmarkBar= */ false);
    }

    @Test
    @MediumTest
    @UiThreadTest
    @EnableFeatures(ChromeFeatureList.ANDROID_BOOKMARK_BAR)
    @Restriction({DeviceFormFactor.TABLET})
    public void testTopControlsHeightWithBookmarkBarWhenFlagIsEnabledOnTablet() {
        testTopControlsHeightWithBookmarkBar(/* expectBookmarkBar= */ true);
    }

    private void testTopControlsHeightWithBookmarkBar(boolean expectBookmarkBar) {
        // Verify bookmark bar (in-)existence.
        final var activity = mActivityTestRule.getActivity();
        final @Nullable var bookmarkBar = activity.findViewById(R.id.bookmark_bar);
        assertThat(bookmarkBar, is(expectBookmarkBar ? notNullValue() : nullValue()));

        // Verify browser controls manager existence.
        final var browserControlsManager =
                BrowserControlsManagerSupplier.getValueOrNullFrom(activity.getWindowAndroid());
        assertNotNull(browserControlsManager);

        // Verify toolbar existence.
        final var toolbarManager = mTabbedRootUiCoordinator.getToolbarManager();
        assertNotNull(toolbarManager);
        final var toolbar = toolbarManager.getToolbar();
        assertNotNull(toolbar);

        // Verify top controls height.
        final int tabStripHeight = toolbar.getTabStripHeight();
        final int toolbarHeight = toolbar.getHeight();
        final int bookmarkBarHeight = bookmarkBar != null ? bookmarkBar.getHeight() : 0;
        assertEquals(
                "Verify top controls height.",
                tabStripHeight + toolbarHeight + bookmarkBarHeight,
                browserControlsManager.getTopControlsHeight());
    }
}
