// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ContextUtils;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Features;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.signin.BottomSheetSigninAndHistorySyncConfig;
import org.chromium.chrome.browser.ui.signin.BottomSheetSigninAndHistorySyncConfig.NoAccountSigninMode;
import org.chromium.chrome.browser.ui.signin.BottomSheetSigninAndHistorySyncConfig.WithAccountSigninMode;
import org.chromium.chrome.browser.ui.signin.SigninAndHistorySyncActivityLauncher;
import org.chromium.chrome.browser.ui.signin.history_sync.HistorySyncConfig;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.ui.modaldialog.ModalDialogManager;

/** Tests {@link SafetyHubModuleDelegate} */
@RunWith(BaseRobolectricTestRunner.class)
@Batch(Batch.PER_CLASS)
// TODO(crbug.com/390442009): Update the tests when the logic starts taking the flag into account
// explicitly. For now the flag is checked in PasswordManagerHelper which gets indirectly
// invoked by these tests.
@Features.DisableFeatures(ChromeFeatureList.LOGIN_DB_DEPRECATION_ANDROID)
public class SafetyHubModuleDelegateTest {

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule public SafetyHubTestRule mSafetyHubTestRule = new SafetyHubTestRule();

    @Mock private Supplier<ModalDialogManager> mModalDialogManagerSupplier;
    @Mock private SigninAndHistorySyncActivityLauncher mSigninLauncher;
    @Mock private Intent mSigninIntent;
    @Mock private Context mContext;

    private SafetyHubModuleDelegate mSafetyHubModuleDelegate;
    private Profile mProfile;
    private PendingIntent mPasswordCheckIntentForAccountCheckup;

    @Before
    public void setUp() {
        mProfile = mSafetyHubTestRule.getProfile();
        mPasswordCheckIntentForAccountCheckup =
                mSafetyHubTestRule.getIntentForAccountPasswordCheckup();

        ModalDialogManager modalDialogManager =
                new ModalDialogManager(
                        mock(ModalDialogManager.Presenter.class),
                        ModalDialogManager.ModalDialogType.APP);
        when(mModalDialogManagerSupplier.get()).thenReturn(modalDialogManager);

        mSafetyHubModuleDelegate =
                new SafetyHubModuleDelegateImpl(
                        mProfile, mModalDialogManagerSupplier, mSigninLauncher);
    }

    @Test
    public void testOpenPasswordCheckUi() throws PendingIntent.CanceledException {
        mSafetyHubTestRule.setSignedInState(true);
        mSafetyHubTestRule.setUPMStatus(true);

        Context context = ContextUtils.getApplicationContext();
        mSafetyHubModuleDelegate.showPasswordCheckUi(context);
        verify(mPasswordCheckIntentForAccountCheckup, times(1)).send();
    }

    @Test
    public void testLaunchSigninPromo() {
        mSafetyHubTestRule.setSignedInState(false);
        when(mSigninLauncher.createBottomSheetSigninIntentOrShowError(
                        eq(mContext), eq(mProfile), any(), eq(SigninAccessPoint.SAFETY_CHECK)))
                .thenReturn(mSigninIntent);

        mSafetyHubModuleDelegate.launchSigninPromo(mContext);

        ArgumentCaptor<BottomSheetSigninAndHistorySyncConfig> configCaptor =
                ArgumentCaptor.forClass(BottomSheetSigninAndHistorySyncConfig.class);
        verify(mSigninLauncher)
                .createBottomSheetSigninIntentOrShowError(
                        eq(mContext),
                        eq(mProfile),
                        configCaptor.capture(),
                        eq(SigninAccessPoint.SAFETY_CHECK));
        BottomSheetSigninAndHistorySyncConfig config = configCaptor.getValue();
        assertEquals(NoAccountSigninMode.BOTTOM_SHEET, config.noAccountSigninMode);
        assertEquals(
                WithAccountSigninMode.DEFAULT_ACCOUNT_BOTTOM_SHEET, config.withAccountSigninMode);
        assertEquals(HistorySyncConfig.OptInMode.NONE, config.historyOptInMode);
        assertNull(config.selectedCoreAccountId);
        verify(mContext).startActivity(mSigninIntent);
    }
}
