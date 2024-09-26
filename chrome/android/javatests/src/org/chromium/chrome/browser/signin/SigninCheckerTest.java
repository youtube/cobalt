// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.when;

import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.chrome.test.util.browser.signin.SigninTestRule;
import org.chromium.chrome.test.util.browser.sync.SyncTestUtil;
import org.chromium.components.externalauth.ExternalAuthUtils;
import org.chromium.components.signin.AccountRenameChecker;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;

/**
 * This class tests the sign-in checks done at Chrome start-up or when accounts
 * change on device.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class SigninCheckerTest {
    private static final String CHILD_ACCOUNT_NAME =
            AccountManagerTestRule.generateChildEmail("test@gmail.com");

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Rule
    public final SigninTestRule mSigninTestRule = new SigninTestRule();

    @Rule
    public final ChromeTabbedActivityTestRule mActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Mock
    private ExternalAuthUtils mExternalAuthUtilsMock;

    @Mock
    private AccountRenameChecker.Delegate mAccountRenameCheckerDelegateMock;

    @Before
    public void setUp() {
        AccountRenameChecker.overrideDelegateForTests(mAccountRenameCheckerDelegateMock);
    }

    @Test
    @MediumTest
    @DisabledTest(message = "https://crbug.com/1205346")
    public void signinWhenPrimaryAccountIsRenamedToAKnownAccount() {
        mActivityTestRule.startMainActivityOnBlankPage();
        mSigninTestRule.addAccountAndWaitForSeeding("the.second.account@gmail.com");
        final CoreAccountInfo oldAccount = mSigninTestRule.addTestAccountThenSigninAndEnableSync();
        final String newAccountEmail = "test.new.account@gmail.com";
        when(mAccountRenameCheckerDelegateMock.getNewNameOfRenamedAccount(oldAccount.getEmail()))
                .thenReturn(newAccountEmail);
        final CoreAccountInfo expectedPrimaryAccount = mSigninTestRule.addAccount(newAccountEmail);

        mSigninTestRule.removeAccountAndWaitForSeeding(oldAccount.getEmail());

        CriteriaHelper.pollUiThread(() -> {
            return expectedPrimaryAccount.equals(
                    mSigninTestRule.getPrimaryAccount(ConsentLevel.SYNC));
        });
    }

    @Test
    @MediumTest
    @DisabledTest(message = "https://crbug.com/1205346")
    public void signoutWhenPrimaryAccountIsRenamedToAnUnknownAccount() {
        mActivityTestRule.startMainActivityOnBlankPage();
        mSigninTestRule.addAccountAndWaitForSeeding("the.second.account@gmail.com");
        final CoreAccountInfo oldAccount = mSigninTestRule.addTestAccountThenSigninAndEnableSync();
        final String newAccountEmail = "test.new.account@gmail.com";
        when(mAccountRenameCheckerDelegateMock.getNewNameOfRenamedAccount(oldAccount.getEmail()))
                .thenReturn(newAccountEmail);

        mSigninTestRule.removeAccountAndWaitForSeeding(oldAccount.getEmail());

        CriteriaHelper.pollUiThread(() -> {
            return !IdentityServicesProvider.get()
                            .getIdentityManager(Profile.getLastUsedRegularProfile())
                            .hasPrimaryAccount(ConsentLevel.SYNC);
        });
        Assert.assertNull(mSigninTestRule.getPrimaryAccount(ConsentLevel.SYNC));
    }

    @Test
    @MediumTest
    @DisabledTest(message = "https://crbug.com/1205346")
    public void signoutWhenPrimaryAccountIsRemoved() {
        mActivityTestRule.startMainActivityOnBlankPage();
        mSigninTestRule.addAccountAndWaitForSeeding("the.second.account@gmail.com");
        final CoreAccountInfo oldAccount = mSigninTestRule.addTestAccountThenSigninAndEnableSync();

        mSigninTestRule.removeAccountAndWaitForSeeding(oldAccount.getEmail());

        CriteriaHelper.pollUiThread(() -> {
            return !IdentityServicesProvider.get()
                            .getIdentityManager(Profile.getLastUsedRegularProfile())
                            .hasPrimaryAccount(ConsentLevel.SYNC);
        });
        Assert.assertNull(mSigninTestRule.getPrimaryAccount(ConsentLevel.SYNC));
    }

    @Test
    @MediumTest
    @DisabledTest(message = "https://crbug.com/1205346")
    public void signoutWhenPrimaryAccountWithoutSyncConsentIsRemoved() {
        mActivityTestRule.startMainActivityOnBlankPage();
        mSigninTestRule.addAccountAndWaitForSeeding("the.second.account@gmail.com");
        final CoreAccountInfo oldAccount = mSigninTestRule.addTestAccountThenSignin();

        mSigninTestRule.removeAccountAndWaitForSeeding(oldAccount.getEmail());

        CriteriaHelper.pollUiThread(() -> {
            return !IdentityServicesProvider.get()
                            .getIdentityManager(Profile.getLastUsedRegularProfile())
                            .hasPrimaryAccount(ConsentLevel.SIGNIN);
        });
    }

    @Test
    @MediumTest
    public void signinWhenChildAccountIsTheOnlyAccount() {
        mActivityTestRule.startMainActivityOnBlankPage();
        UserActionTester actionTester = new UserActionTester();

        final CoreAccountInfo expectedPrimaryAccount =
                mSigninTestRule.addAccountAndWaitForSeeding(CHILD_ACCOUNT_NAME);

        CriteriaHelper.pollUiThread(() -> {
            return expectedPrimaryAccount.equals(
                    mSigninTestRule.getPrimaryAccount(ConsentLevel.SIGNIN));
        });
        Assert.assertNull(mSigninTestRule.getPrimaryAccount(ConsentLevel.SYNC));
        Assert.assertEquals(
                3, SigninCheckerProvider.get().getNumOfChildAccountChecksDoneForTests());
        Assert.assertTrue(
                actionTester.getActions().contains("Signin_Signin_WipeDataOnChildAccountSignin2"));
        Assert.assertFalse(SyncTestUtil.isSyncFeatureEnabled());
    }

    @Test
    @MediumTest
    public void noSigninWhenChildAccountIsTheOnlyAccountButSigninIsNotAllowed() {
        mActivityTestRule.startMainActivityOnBlankPage();
        UserActionTester actionTester = new UserActionTester();
        when(mExternalAuthUtilsMock.isGooglePlayServicesMissing(any())).thenReturn(true);
        ExternalAuthUtils.setInstanceForTesting(mExternalAuthUtilsMock);

        mSigninTestRule.addAccountAndWaitForSeeding(CHILD_ACCOUNT_NAME);

        // The check should be done twice, once at activity start-up, the other when account
        // is added.
        CriteriaHelper.pollUiThread(() -> {
            return SigninCheckerProvider.get().getNumOfChildAccountChecksDoneForTests() == 2;
        });
        Assert.assertNull(mSigninTestRule.getPrimaryAccount(ConsentLevel.SYNC));
        Assert.assertFalse(
                actionTester.getActions().contains("Signin_Signin_WipeDataOnChildAccountSignin2"));
    }

    @Test
    @MediumTest
    public void noSigninWhenChildAccountIsTheSecondaryAccount() {
        // If a child account co-exists with another account on the device, then the child account
        // must be the first device (this is enforced by the Kids Module).  The behaviour in this
        // test case therefore is not currently hittable on a real device; however it is included
        // here for completeness.
        mSigninTestRule.addAccount("the.default.account@gmail.com");
        mSigninTestRule.addAccount(CHILD_ACCOUNT_NAME);

        mActivityTestRule.startMainActivityOnBlankPage();
        UserActionTester actionTester = new UserActionTester();

        // The check should be done once at activity start-up
        CriteriaHelper.pollUiThread(() -> {
            return SigninCheckerProvider.get().getNumOfChildAccountChecksDoneForTests() == 1;
        });
        Assert.assertNull(mSigninTestRule.getPrimaryAccount(ConsentLevel.SYNC));
        Assert.assertFalse(
                actionTester.getActions().contains("Signin_Signin_WipeDataOnChildAccountSignin2"));
    }

    @Test
    @MediumTest
    @DisabledTest(message = "https://crbug.com/1293942")
    public void signinWhenChildAccountIsFirstAccount() {
        final CoreAccountInfo childAccount = mSigninTestRule.addAccount(CHILD_ACCOUNT_NAME);
        mSigninTestRule.addAccount("the.second.account@gmail.com");

        mActivityTestRule.startMainActivityOnBlankPage();
        UserActionTester actionTester = new UserActionTester();

        CriteriaHelper.pollUiThread(() -> {
            return childAccount.equals(mSigninTestRule.getPrimaryAccount(ConsentLevel.SYNC));
        });

        // The check should be done once at account addition and once at activity start-up.
        Assert.assertEquals(
                2, SigninCheckerProvider.get().getNumOfChildAccountChecksDoneForTests());
        Assert.assertTrue(
                actionTester.getActions().contains("Signin_Signin_WipeDataOnChildAccountSignin2"));
    }
}
