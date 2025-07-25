// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.os.UserManager;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;
import org.mockito.stubbing.Answer;
import org.robolectric.annotation.LooperMode;
import org.robolectric.shadows.ShadowApplication;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.signin.services.SigninPreferencesManager;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.components.externalauth.ExternalAuthUtils;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.AccountUtils;
import org.chromium.components.signin.base.AccountCapabilities;
import org.chromium.components.signin.base.AccountInfo;
import org.chromium.components.signin.base.CoreAccountId;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.AccountInfoServiceProvider;
import org.chromium.components.signin.identitymanager.AccountTrackerService;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.identitymanager.IdentityManagerJni;
import org.chromium.components.signin.identitymanager.IdentityMutator;
import org.chromium.components.signin.identitymanager.PrimaryAccountChangeEvent;
import org.chromium.components.signin.identitymanager.PrimaryAccountError;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.signin.metrics.SignoutReason;
import org.chromium.components.signin.test.util.FakeAccountManagerFacade;
import org.chromium.components.sync.SyncService;
import org.chromium.components.sync.UserSelectableType;

import java.util.HashMap;
import java.util.Set;
import java.util.concurrent.atomic.AtomicInteger;

/** Tests for {@link SigninManagerImpl}. */
@RunWith(BaseRobolectricTestRunner.class)
@LooperMode(LooperMode.Mode.LEGACY)
public class SigninManagerImplTest {
    private static final long NATIVE_SIGNIN_MANAGER = 10001L;
    private static final long NATIVE_IDENTITY_MANAGER = 10002L;
    private static final AccountInfo ACCOUNT_INFO =
            new AccountInfo(
                    new CoreAccountId("gaia-id-user"),
                    "user@domain.com",
                    "gaia-id-user",
                    "full name",
                    "given name",
                    null,
                    new AccountCapabilities(new HashMap<>()));
    private static final CoreAccountInfo CHILD_CORE_ACCOUNT_INFO =
            CoreAccountInfo.createFromEmailAndGaiaId(
                    FakeAccountManagerFacade.generateChildEmail("user@domain.com"),
                    "child-gaia-id-user");

    @Rule public final TestRule mFeaturesProcessorRule = new Features.JUnitProcessor();

    @Rule public final JniMocker mocker = new JniMocker();

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.LENIENT);

    @Mock private SigninManagerImpl.Natives mNativeMock;
    @Mock private IdentityManager.Natives mIdentityManagerNativeMock;
    @Mock private AccountTrackerService mAccountTrackerService;
    @Mock private IdentityMutator mIdentityMutator;
    @Mock private ExternalAuthUtils mExternalAuthUtils;
    @Mock private SyncService mSyncService;
    @Mock private Profile mProfile;
    @Mock private SigninManager.SignInStateObserver mSignInStateObserver;

    private final IdentityManager mIdentityManager =
            IdentityManager.create(NATIVE_IDENTITY_MANAGER, null /* OAuth2TokenService */);
    private final FakeAccountManagerFacade mFakeAccountManagerFacade =
            new FakeAccountManagerFacade();
    private SigninManagerImpl mSigninManager;

    @Before
    public void setUp() {
        mocker.mock(SigninManagerImplJni.TEST_HOOKS, mNativeMock);
        mocker.mock(IdentityManagerJni.TEST_HOOKS, mIdentityManagerNativeMock);
        ExternalAuthUtils.setInstanceForTesting(mExternalAuthUtils);
        Profile.setLastUsedProfileForTesting(mProfile);
        when(mNativeMock.isSigninAllowedByPolicy(NATIVE_SIGNIN_MANAGER)).thenReturn(true);
        // Pretend Google Play services are available as it is required for the sign-in
        when(mExternalAuthUtils.isGooglePlayServicesMissing(any())).thenReturn(false);
        doAnswer(
                        invocation -> {
                            Runnable runnable = invocation.getArgument(0);
                            runnable.run();
                            return null;
                        })
                .when(mAccountTrackerService)
                .legacySeedAccountsIfNeeded(any(Runnable.class));
        // Suppose that the accounts are already seeded
        when(mIdentityManagerNativeMock.findExtendedAccountInfoByEmailAddress(
                        NATIVE_IDENTITY_MANAGER, ACCOUNT_INFO.getEmail()))
                .thenReturn(ACCOUNT_INFO);
        when(mIdentityManagerNativeMock.isClearPrimaryAccountAllowed(NATIVE_IDENTITY_MANAGER))
                .thenReturn(true);

        AccountManagerFacadeProvider.setInstanceForTests(mFakeAccountManagerFacade);

        mSigninManager =
                (SigninManagerImpl)
                        SigninManagerImpl.create(
                                NATIVE_SIGNIN_MANAGER,
                                mAccountTrackerService,
                                mIdentityManager,
                                mIdentityMutator,
                                mSyncService);
        mSigninManager.addSignInStateObserver(mSignInStateObserver);
    }

    @After
    public void tearDown() {
        mSigninManager.removeSignInStateObserver(mSignInStateObserver);
        mSigninManager.destroy();
        AccountInfoServiceProvider.resetForTests();
    }

    @Test
    public void signinAndTurnSyncOn() {
        when(mIdentityMutator.setPrimaryAccount(any(), anyInt(), anyInt()))
                .thenReturn(PrimaryAccountError.NO_ERROR);
        when(mSyncService.getSelectedTypes()).thenReturn(Set.of(UserSelectableType.BOOKMARKS));

        // There is no signed in account. Sign in is allowed.
        assertTrue(mSigninManager.isSigninAllowed());
        assertTrue(mSigninManager.isSyncOptInAllowed());
        // Sign out is not allowed.
        assertFalse(mSigninManager.isSignOutAllowed());

        SigninManager.SignInCallback callback = mock(SigninManager.SignInCallback.class);
        mSigninManager.signinAndEnableSync(
                AccountUtils.createAccountFromName(ACCOUNT_INFO.getEmail()),
                SigninAccessPoint.START_PAGE,
                callback);

        verify(mNativeMock)
                .fetchAndApplyCloudPolicy(eq(NATIVE_SIGNIN_MANAGER), eq(ACCOUNT_INFO), any());
        // A sign in operation is in progress, so we do not allow a new sign in/out operation.
        assertFalse(mSigninManager.isSigninAllowed());
        assertFalse(mSigninManager.isSyncOptInAllowed());
        assertFalse(mSigninManager.isSignOutAllowed());

        mSigninManager.finishSignInAfterPolicyEnforced();
        verify(mIdentityMutator)
                .setPrimaryAccount(
                        ACCOUNT_INFO.getId(), ConsentLevel.SYNC, SigninAccessPoint.START_PAGE);
        verify(mSyncService).setSyncRequested();
        // Signin should be complete and callback should be invoked.
        verify(callback).onSignInComplete();
        verify(callback, never()).onSignInAborted();

        // The primary account is now present and consented to sign in and sync.  We do not allow
        // another account to be signed in.
        when(mIdentityManagerNativeMock.getPrimaryAccountInfo(
                        eq(NATIVE_IDENTITY_MANAGER), anyInt()))
                .thenReturn(ACCOUNT_INFO);
        assertFalse(mSigninManager.isSigninAllowed());
        assertFalse(mSigninManager.isSyncOptInAllowed());
        // Signing out is allowed.
        assertTrue(mSigninManager.isSignOutAllowed());
    }

    @Test
    public void signinNoTurnSyncOn() {
        when(mIdentityMutator.setPrimaryAccount(any(), anyInt(), anyInt()))
                .thenReturn(PrimaryAccountError.NO_ERROR);

        assertTrue(mSigninManager.isSigninAllowed());
        assertTrue(mSigninManager.isSyncOptInAllowed());

        SigninManager.SignInCallback callback = mock(SigninManager.SignInCallback.class);
        mSigninManager.signin(
                AccountUtils.createAccountFromName(ACCOUNT_INFO.getEmail()),
                SigninAccessPoint.START_PAGE,
                callback);

        // Signin without turning on sync shouldn't apply policies.
        verify(mNativeMock, never()).fetchAndApplyCloudPolicy(anyLong(), any(), any());
        verify(mIdentityMutator)
                .setPrimaryAccount(
                        ACCOUNT_INFO.getId(), ConsentLevel.SIGNIN, SigninAccessPoint.START_PAGE);

        verify(mSyncService, never()).setSyncRequested();
        // Signin should be complete qand callback should be invoked.
        verify(callback).onSignInComplete();
        verify(callback, never()).onSignInAborted();

        // The primary account is now present and consented to sign in.
        when(mIdentityManagerNativeMock.getPrimaryAccountInfo(
                        eq(NATIVE_IDENTITY_MANAGER), eq(ConsentLevel.SIGNIN)))
                .thenReturn(ACCOUNT_INFO);
        assertFalse(mSigninManager.isSigninAllowed());
        assertTrue(mSigninManager.isSyncOptInAllowed());
    }

    @Test
    public void signOutNonSyncingAccountFromJavaWithManagedDomain() {
        when(mNativeMock.getManagementDomain(NATIVE_SIGNIN_MANAGER)).thenReturn("TestDomain");

        // Trigger the sign out flow!
        mSigninManager.signOut(SignoutReason.TEST);

        // The primary account should be cleared *before* clearing any account data.
        // For more information see crbug.com/589028.
        InOrder inOrder = inOrder(mNativeMock, mIdentityMutator);
        inOrder.verify(mIdentityMutator).clearPrimaryAccount(eq(SignoutReason.TEST), anyInt());

        // Sign-out should only clear the profile when the user is managed.
        inOrder.verify(mNativeMock).wipeProfileData(eq(NATIVE_SIGNIN_MANAGER), any());
        verify(mNativeMock, never()).wipeGoogleServiceWorkerCaches(anyLong(), any());
    }

    @Test
    public void signOutSyncingAccountFromJavaWithManagedDomain() {
        when(mNativeMock.getManagementDomain(NATIVE_SIGNIN_MANAGER)).thenReturn("TestDomain");

        // Trigger the sign out flow!
        mSigninManager.signOut(SignoutReason.TEST);

        // The primary account should be cleared *before* clearing any account data.
        // For more information see crbug.com/589028.
        InOrder inOrder = inOrder(mNativeMock, mIdentityMutator);
        inOrder.verify(mIdentityMutator).clearPrimaryAccount(eq(SignoutReason.TEST), anyInt());

        // Sign-out should only clear the profile when the user is managed.
        inOrder.verify(mNativeMock).wipeProfileData(eq(NATIVE_SIGNIN_MANAGER), any());
        verify(mNativeMock, never()).wipeGoogleServiceWorkerCaches(anyLong(), any());
    }

    @Test
    public void signOutNonSyncingAccountFromJavaWithNullDomain() {
        mSigninManager.signOut(SignoutReason.TEST);

        // The primary account should be cleared *before* clearing any account data.
        // For more information see crbug.com/589028.
        InOrder inOrder = inOrder(mNativeMock, mIdentityMutator);
        inOrder.verify(mIdentityMutator).clearPrimaryAccount(eq(SignoutReason.TEST), anyInt());

        // Sign-out should only clear the service worker cache when the user is neither managed or
        // syncing.
        verify(mNativeMock, never()).wipeProfileData(anyLong(), any());
        inOrder.verify(mNativeMock).wipeGoogleServiceWorkerCaches(eq(NATIVE_SIGNIN_MANAGER), any());
    }

    @Test
    public void signOutSyncingAccountFromJavaWithNullDomain() {
        // Simulate sign-out with non-managed account.
        when(mIdentityManagerNativeMock.getPrimaryAccountInfo(
                        eq(NATIVE_IDENTITY_MANAGER), anyInt()))
                .thenReturn(ACCOUNT_INFO);

        mSigninManager.signOut(SignoutReason.TEST);

        // The primary account should be cleared *before* clearing any account data.
        // For more information see crbug.com/589028.
        InOrder inOrder = inOrder(mNativeMock, mIdentityMutator);
        inOrder.verify(mIdentityMutator).clearPrimaryAccount(eq(SignoutReason.TEST), anyInt());

        // Sign-out should only clear the service worker cache when the user has decided not to
        // wipe data.
        verify(mNativeMock, never()).wipeProfileData(anyLong(), any());
        inOrder.verify(mNativeMock).wipeGoogleServiceWorkerCaches(eq(NATIVE_SIGNIN_MANAGER), any());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.SYNC_ANDROID_LIMIT_NTP_PROMO_IMPRESSIONS)
    public void syncPromoShowCountResetWhenSignOutSyncingAccount() {
        ChromeSharedPreferences.getInstance()
                .writeInt(
                        ChromePreferenceKeys.SYNC_PROMO_SHOW_COUNT.createKey(
                                SigninPreferencesManager.SyncPromoAccessPointId.NTP),
                        1);

        // Simulate sign-out with non-managed account.
        when(mIdentityManagerNativeMock.getPrimaryAccountInfo(
                        eq(NATIVE_IDENTITY_MANAGER), anyInt()))
                .thenReturn(ACCOUNT_INFO);

        mSigninManager.signOut(SignoutReason.TEST);

        ArgumentCaptor<Runnable> callback = ArgumentCaptor.forClass(Runnable.class);
        verify(mNativeMock)
                .wipeGoogleServiceWorkerCaches(eq(NATIVE_SIGNIN_MANAGER), callback.capture());
        assertNotNull(callback.getValue());

        callback.getValue().run();
        assertEquals(
                0,
                ChromeSharedPreferences.getInstance()
                        .readInt(
                                ChromePreferenceKeys.SYNC_PROMO_SHOW_COUNT.createKey(
                                        SigninPreferencesManager.SyncPromoAccessPointId.NTP)));
    }

    @Test
    public void signOutSyncingAccountFromJavaWithNullDomainAndForceWipe() {
        when(mIdentityManagerNativeMock.getPrimaryAccountInfo(
                        eq(NATIVE_IDENTITY_MANAGER), anyInt()))
                .thenReturn(ACCOUNT_INFO);

        mSigninManager.signOut(SignoutReason.TEST, null, true);

        // The primary account should be cleared *before* clearing any account data.
        // For more information see crbug.com/589028.
        InOrder inOrder = inOrder(mNativeMock, mIdentityMutator);
        inOrder.verify(mIdentityMutator).clearPrimaryAccount(eq(SignoutReason.TEST), anyInt());

        // Sign-out should only clear the profile when the user is syncing and has decided to
        // wipe data.
        inOrder.verify(mNativeMock).wipeProfileData(eq(NATIVE_SIGNIN_MANAGER), any());
        verify(mNativeMock, never()).wipeGoogleServiceWorkerCaches(anyLong(), any());
    }

    // TODO(crbug.com/1294761): add test for revokeSyncConsentFromJavaWithManagedDomain() and
    // revokeSyncConsentFromJavaWipeData() - this requires making the BookmarkModel mockable in
    // SigninManagerImpl.

    @Test
    public void revokeSyncConsentFromJavaWithNullDomain() {
        SigninManager.SignOutCallback callback = mock(SigninManager.SignOutCallback.class);
        when(mIdentityManagerNativeMock.getPrimaryAccountInfo(
                        eq(NATIVE_IDENTITY_MANAGER), anyInt()))
                .thenReturn(ACCOUNT_INFO);

        mSigninManager.revokeSyncConsent(SignoutReason.TEST, callback, false);

        // The primary account should be cleared *before* clearing any account data.
        // For more information see crbug.com/589028.
        InOrder inOrder = inOrder(mNativeMock, mIdentityMutator);
        inOrder.verify(mIdentityMutator).revokeSyncConsent(eq(SignoutReason.TEST), anyInt());

        // Disabling sync should only clear the service worker cache when the user is neither
        // managed or syncing.
        verify(mNativeMock, never()).wipeProfileData(anyLong(), any());
        inOrder.verify(mNativeMock).wipeGoogleServiceWorkerCaches(anyLong(), any());
    }

    @Test
    public void clearingAccountCookieDoesNotTriggerSignoutWhenUserIsSignedOut() {
        mFakeAccountManagerFacade.addAccount(
                AccountUtils.createAccountFromName(ACCOUNT_INFO.getEmail()));

        mIdentityManager.onAccountsCookieDeletedByUserAction();

        verify(mIdentityMutator, never()).clearPrimaryAccount(anyInt(), anyInt());
        verify(mNativeMock, never()).wipeProfileData(anyLong(), any());
        verify(mNativeMock, never()).wipeGoogleServiceWorkerCaches(anyLong(), any());
    }

    @Test
    public void clearingAccountCookieDoesNotTriggerSignoutWhenUserIsSignedInAndSync() {
        when(mIdentityManagerNativeMock.getPrimaryAccountInfo(
                        eq(NATIVE_IDENTITY_MANAGER), anyInt()))
                .thenReturn(ACCOUNT_INFO);
        mFakeAccountManagerFacade.addAccount(
                AccountUtils.createAccountFromName(ACCOUNT_INFO.getEmail()));

        mIdentityManager.onAccountsCookieDeletedByUserAction();

        verify(mIdentityMutator, never()).clearPrimaryAccount(anyInt(), anyInt());
        verify(mNativeMock, never()).wipeProfileData(anyLong(), any());
        verify(mNativeMock, never()).wipeGoogleServiceWorkerCaches(anyLong(), any());
    }

    @Test
    public void clearingAccountCookieDoesNotTriggerSignoutWhenUserIsSignedInWithoutSync() {
        when(mIdentityManagerNativeMock.getPrimaryAccountInfo(
                        NATIVE_IDENTITY_MANAGER, ConsentLevel.SIGNIN))
                .thenReturn(ACCOUNT_INFO);
        mFakeAccountManagerFacade.addAccount(
                AccountUtils.createAccountFromName(ACCOUNT_INFO.getEmail()));

        mIdentityManager.onAccountsCookieDeletedByUserAction();

        verify(mIdentityMutator, never()).clearPrimaryAccount(anyInt(), anyInt());
        verify(mNativeMock, never()).wipeProfileData(anyLong(), any());
        verify(mNativeMock, never()).wipeGoogleServiceWorkerCaches(anyLong(), any());
    }

    @Test
    public void callbackNotifiedWhenNoOperationIsInProgress() {
        AtomicInteger callCount = new AtomicInteger(0);

        mSigninManager.runAfterOperationInProgress(callCount::incrementAndGet);
        assertEquals(1, callCount.get());
    }

    @Test
    // TODO(crbug.com/1353777): Disabling the feature explicitly, because native is not available to
    // provide a default value. This should be enabled if the feature is enabled by default or
    // removed if the flag is removed.
    @DisableFeatures(ChromeFeatureList.SYNC_ANDROID_LIMIT_NTP_PROMO_IMPRESSIONS)
    public void callbackNotifiedOnSignout() {
        doAnswer(
                        invocation -> {
                            mIdentityManager.onPrimaryAccountChanged(
                                    new PrimaryAccountChangeEvent(
                                            PrimaryAccountChangeEvent.Type.CLEARED,
                                            PrimaryAccountChangeEvent.Type.NONE));
                            return null;
                        })
                .when(mIdentityMutator)
                .clearPrimaryAccount(anyInt(), anyInt());

        mSigninManager.signOut(SignoutReason.TEST);
        AtomicInteger callCount = new AtomicInteger(0);
        mSigninManager.runAfterOperationInProgress(callCount::incrementAndGet);
        assertEquals(0, callCount.get());

        mSigninManager.finishSignOut();
        assertEquals(1, callCount.get());
    }

    @Test
    public void callbackNotifiedOnSignin() {
        final Answer<Integer> setPrimaryAccountAnswer =
                invocation -> {
                    // From now on getPrimaryAccountInfo should return account.
                    when(mIdentityManagerNativeMock.getPrimaryAccountInfo(
                                    eq(NATIVE_IDENTITY_MANAGER), anyInt()))
                            .thenReturn(ACCOUNT_INFO);
                    return PrimaryAccountError.NO_ERROR;
                };
        doAnswer(setPrimaryAccountAnswer)
                .when(mIdentityMutator)
                .setPrimaryAccount(
                        ACCOUNT_INFO.getId(), ConsentLevel.SYNC, SigninAccessPoint.UNKNOWN);

        mSigninManager.signinAndEnableSync(
                AccountUtils.createAccountFromName(ACCOUNT_INFO.getEmail()),
                SigninAccessPoint.UNKNOWN,
                null);

        AtomicInteger callCount = new AtomicInteger(0);
        mSigninManager.runAfterOperationInProgress(callCount::incrementAndGet);
        assertEquals(0, callCount.get());

        mSigninManager.finishSignInAfterPolicyEnforced();
        assertEquals(1, callCount.get());
    }

    @Test(expected = AssertionError.class)
    public void signinfailsWhenAlreadySignedIn() {
        when(mIdentityManagerNativeMock.getPrimaryAccountInfo(
                        eq(NATIVE_IDENTITY_MANAGER), anyInt()))
                .thenReturn(ACCOUNT_INFO);
        mSigninManager.signinAndEnableSync(
                AccountUtils.createAccountFromName(ACCOUNT_INFO.getEmail()),
                SigninAccessPoint.UNKNOWN,
                null);
    }

    @Test
    public void signInStateObserverCallOnSignIn() {
        final Answer<Integer> setPrimaryAccountAnswer =
                invocation -> {
                    // From now on getPrimaryAccountInfo should return account.
                    when(mIdentityManagerNativeMock.getPrimaryAccountInfo(
                                    eq(NATIVE_IDENTITY_MANAGER), anyInt()))
                            .thenReturn(ACCOUNT_INFO);
                    return PrimaryAccountError.NO_ERROR;
                };
        doAnswer(setPrimaryAccountAnswer)
                .when(mIdentityMutator)
                .setPrimaryAccount(
                        ACCOUNT_INFO.getId(), ConsentLevel.SYNC, SigninAccessPoint.START_PAGE);

        mSigninManager.signinAndEnableSync(
                AccountUtils.createAccountFromName(ACCOUNT_INFO.getEmail()),
                SigninAccessPoint.START_PAGE,
                null);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        verify(mSignInStateObserver).onSignInAllowedChanged();

        mSigninManager.finishSignInAfterPolicyEnforced();
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        verify(mSignInStateObserver).onSignOutAllowedChanged();
        assertFalse(mSigninManager.isSigninAllowed());
        assertTrue(mSigninManager.isSignOutAllowed());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.SYNC_ANDROID_LIMIT_NTP_PROMO_IMPRESSIONS)
    public void signInStateObserverCallOnSignOut() {
        when(mIdentityManagerNativeMock.getPrimaryAccountInfo(
                        eq(NATIVE_IDENTITY_MANAGER), anyInt()))
                .thenReturn(ACCOUNT_INFO);
        assertTrue(mSigninManager.isSignOutAllowed());

        mSigninManager.signOut(SignoutReason.TEST);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        verify(mSignInStateObserver).onSignOutAllowedChanged();
        assertFalse(mSigninManager.isSignOutAllowed());
    }

    @Test
    public void signOutNotAllowedForChildAccounts() {
        when(mIdentityManagerNativeMock.getPrimaryAccountInfo(
                        eq(NATIVE_IDENTITY_MANAGER), anyInt()))
                .thenReturn(ACCOUNT_INFO);
        when(mIdentityManagerNativeMock.isClearPrimaryAccountAllowed(NATIVE_IDENTITY_MANAGER))
                .thenReturn(false);

        assertFalse(mSigninManager.isSignOutAllowed());
    }

    @Test
    public void signInShouldBeSupportedForNonDemoUsers() {
        when(mExternalAuthUtils.canUseGooglePlayServices()).thenReturn(true);

        // Make sure that the user is not a demo user.
        ShadowApplication shadowApplication = ShadowApplication.getInstance();
        UserManager userManager = Mockito.mock(UserManager.class);
        Mockito.when(userManager.isDemoUser()).thenReturn(false);
        shadowApplication.setSystemService(Context.USER_SERVICE, userManager);

        assertTrue(mSigninManager.isSigninSupported(/* requireUpdatedPlayServices= */ true));
        assertTrue(mSigninManager.isSigninSupported(/* requireUpdatedPlayServices= */ false));
    }

    @Test
    public void signInShouldNotBeSupportedWhenGooglePlayServicesIsRequiredAndNotAvailable() {
        when(mExternalAuthUtils.canUseGooglePlayServices()).thenReturn(false);

        // Make sure that the user is not a demo user.
        ShadowApplication shadowApplication = ShadowApplication.getInstance();
        UserManager userManager = Mockito.mock(UserManager.class);
        Mockito.when(userManager.isDemoUser()).thenReturn(false);
        shadowApplication.setSystemService(Context.USER_SERVICE, userManager);

        assertFalse(mSigninManager.isSigninSupported(/* requireUpdatedPlayServices= */ true));
    }
}
