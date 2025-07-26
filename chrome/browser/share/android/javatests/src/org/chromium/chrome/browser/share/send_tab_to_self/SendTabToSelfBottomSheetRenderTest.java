// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.send_tab_to_self;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.mockito.Mockito.when;

import android.app.Activity;
import android.view.View;

import androidx.test.filters.MediumTest;

import org.junit.BeforeClass;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.signin.base.AccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.test.util.TestAccounts;
import org.chromium.components.sync_device_info.FormFactor;
import org.chromium.ui.test.util.BlankUiTestActivity;
import org.chromium.ui.test.util.RenderTestRule;
import org.chromium.url.JUnitTestGURLs;

import java.util.Arrays;
import java.util.Calendar;
import java.util.List;
import java.util.concurrent.TimeUnit;

/** Render tests for the send-tab-to-self bottom sheets. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class SendTabToSelfBottomSheetRenderTest {
    @ClassRule
    public static BaseActivityTestRule<BlankUiTestActivity> sActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    private static Activity sActivity;

    @Rule
    public final RenderTestRule mRenderTestRule =
            RenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(RenderTestRule.Component.UI_BROWSER_SHARING)
                    .setRevision(5)
                    .build();

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Profile mProfile;
    @Mock private IdentityServicesProvider mIdentityServicesProvider;
    @Mock private IdentityManager mIdentityManager;
    @Mock private BottomSheetController mBottomSheetController;

    @BeforeClass
    public static void setupSuite() {
        sActivity = sActivityTestRule.launchActivity(null);
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    public void testDevicePickerBottomSheet() throws Throwable {
        setUpAccountData(TestAccounts.ACCOUNT1);
        long todayTimestamp = Calendar.getInstance().getTimeInMillis();
        List<TargetDeviceInfo> devices =
                Arrays.asList(
                        new TargetDeviceInfo("My Phone", "guid1", FormFactor.PHONE, todayTimestamp),
                        new TargetDeviceInfo(
                                "My Computer",
                                "guid2",
                                FormFactor.DESKTOP,
                                todayTimestamp - TimeUnit.DAYS.toMillis(1)));
        View view =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            DevicePickerBottomSheetContent sheetContent =
                                    new DevicePickerBottomSheetContent(
                                            sActivity,
                                            JUnitTestGURLs.HTTP_URL.getSpec(),
                                            "Title",
                                            mBottomSheetController,
                                            devices,
                                            mProfile);
                            sActivity.setContentView(sheetContent.getContentView());
                            return sheetContent.getContentView();
                        });
        mRenderTestRule.render(view, "device_picker");
    }

    @Test
    @MediumTest
    public void testDevicePickerBottomSheetWithNonDisplayableAccountEmail() throws Throwable {
        AccountInfo account = TestAccounts.CHILD_ACCOUNT_NON_DISPLAYABLE_EMAIL;
        setUpAccountData(account);
        long todayTimestamp = Calendar.getInstance().getTimeInMillis();
        List<TargetDeviceInfo> devices =
                Arrays.asList(
                        new TargetDeviceInfo("My Phone", "guid1", FormFactor.PHONE, todayTimestamp),
                        new TargetDeviceInfo(
                                "My Computer",
                                "guid2",
                                FormFactor.DESKTOP,
                                todayTimestamp - TimeUnit.DAYS.toMillis(1)));
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    DevicePickerBottomSheetContent sheetContent =
                            new DevicePickerBottomSheetContent(
                                    sActivity,
                                    JUnitTestGURLs.HTTP_URL.getSpec(),
                                    "Title",
                                    mBottomSheetController,
                                    devices,
                                    mProfile);
                    sActivity.setContentView(sheetContent.getContentView());
                });
        onView(withText(account.getEmail())).check(doesNotExist());
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    public void testNoTargetDeviceBottomSheet() throws Throwable {
        setUpAccountData(TestAccounts.ACCOUNT1);
        View view =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            NoTargetDeviceBottomSheetContent sheetContent =
                                    new NoTargetDeviceBottomSheetContent(sActivity, mProfile);
                            sActivity.setContentView(sheetContent.getContentView());
                            return sheetContent.getContentView();
                        });
        mRenderTestRule.render(view, "no_target_device_with_account");
    }

    @Test
    @MediumTest
    public void testNoTargetDeviceBottomSheetWithNonDisplayableAccountEmail() throws Throwable {
        AccountInfo account = TestAccounts.CHILD_ACCOUNT_NON_DISPLAYABLE_EMAIL;
        setUpAccountData(account);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    NoTargetDeviceBottomSheetContent sheetContent =
                            new NoTargetDeviceBottomSheetContent(sActivity, mProfile);
                    sActivity.setContentView(sheetContent.getContentView());
                });
        onView(withText(account.getEmail())).check(doesNotExist());
    }

    /** Set up account data to be shown by the UI. */
    private void setUpAccountData(AccountInfo account) {
        // Set up account data to be shown by the UI.
        when(mIdentityManager.getPrimaryAccountInfo(ConsentLevel.SIGNIN)).thenReturn(account);
        when(mIdentityManager.findExtendedAccountInfoByEmailAddress(account.getEmail()))
                .thenReturn(account);
        when(mIdentityServicesProvider.getIdentityManager(mProfile)).thenReturn(mIdentityManager);
        IdentityServicesProvider.setInstanceForTests(mIdentityServicesProvider);
    }
}
