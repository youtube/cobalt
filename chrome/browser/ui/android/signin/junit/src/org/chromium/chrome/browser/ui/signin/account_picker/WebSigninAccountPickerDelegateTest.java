// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.account_picker;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;
import org.robolectric.annotation.LooperMode;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.signin.services.WebSigninBridge;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.components.signin.AccountUtils;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.base.GoogleServiceAuthError;
import org.chromium.components.signin.base.GoogleServiceAuthError.State;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.signin.test.util.FakeAccountManagerFacade;
import org.chromium.content_public.browser.LoadUrlParams;

/** This class tests the {@link WebSigninAccountPickerDelegate}. */
@RunWith(BaseRobolectricTestRunner.class)
@LooperMode(LooperMode.Mode.LEGACY)
public class WebSigninAccountPickerDelegateTest {
    private static final String CONTINUE_URL = "https://test-continue-url.com";
    private static final String TEST_EMAIL = "test.account@gmail.com";

    private final FakeAccountManagerFacade mFakeAccountManagerFacade =
            spy(new FakeAccountManagerFacade());

    @Rule
    public final AccountManagerTestRule mAccountManagerTestRule =
            new AccountManagerTestRule(mFakeAccountManagerFacade);

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Mock private WebSigninBridge.Factory mWebSigninBridgeFactoryMock;

    @Mock private WebSigninBridge mWebSigninBridgeMock;

    @Mock private SigninManager mSigninManagerMock;

    @Mock private IdentityManager mIdentityManagerMock;

    @Mock private Profile mProfileMock;

    @Mock private Tab mTabMock;

    @Captor private ArgumentCaptor<LoadUrlParams> mLoadUrlParamsCaptor;

    @Captor private ArgumentCaptor<WebSigninBridge.Listener> mWebSigninBridgeListenerCaptor;

    private WebSigninAccountPickerDelegate mDelegate;

    private CoreAccountInfo mCoreAccountInfo;

    @Before
    public void setUp() {
        Profile.setLastUsedProfileForTesting(mProfileMock);
        IdentityServicesProvider.setInstanceForTests(mock(IdentityServicesProvider.class));
        when(IdentityServicesProvider.get().getIdentityManager(any()))
                .thenReturn(mIdentityManagerMock);
        when(IdentityServicesProvider.get().getSigninManager(any())).thenReturn(mSigninManagerMock);

        mCoreAccountInfo = mAccountManagerTestRule.addAccount(TEST_EMAIL);

        mDelegate =
                new WebSigninAccountPickerDelegate(
                        mTabMock, mWebSigninBridgeFactoryMock, CONTINUE_URL);
        when(mWebSigninBridgeFactoryMock.create(eq(mProfileMock), any(), any()))
                .thenReturn(mWebSigninBridgeMock);
    }

    @After
    public void tearDown() {
        mDelegate.destroy();
    }

    @Test
    public void testSignInSucceeded() {
        mDelegate.signIn(TEST_EMAIL, error -> {});
        InOrder calledInOrder = inOrder(mWebSigninBridgeFactoryMock, mSigninManagerMock);
        calledInOrder
                .verify(mWebSigninBridgeFactoryMock)
                .create(
                        eq(mProfileMock),
                        eq(mCoreAccountInfo),
                        mWebSigninBridgeListenerCaptor.capture());
        calledInOrder
                .verify(mSigninManagerMock)
                .signin(
                        eq(AccountUtils.createAccountFromName(TEST_EMAIL)),
                        eq(SigninAccessPoint.WEB_SIGNIN),
                        any());
        mWebSigninBridgeListenerCaptor.getValue().onSigninSucceeded();
        verify(mTabMock).loadUrl(mLoadUrlParamsCaptor.capture());
        LoadUrlParams loadUrlParams = mLoadUrlParamsCaptor.getValue();
        Assert.assertEquals("Continue url does not match!", CONTINUE_URL, loadUrlParams.getUrl());
    }

    @Test
    public void testSignInAborted() {
        doAnswer(
                        invocation -> {
                            SigninManager.SignInCallback callback = invocation.getArgument(2);
                            callback.onSignInAborted();
                            return null;
                        })
                .when(mSigninManagerMock)
                .signin(
                        eq(AccountUtils.createAccountFromName(TEST_EMAIL)),
                        eq(SigninAccessPoint.WEB_SIGNIN),
                        any());
        mDelegate.signIn(TEST_EMAIL, error -> {});
        verify(mWebSigninBridgeMock).destroy();
    }

    @Test
    public void testSigninTriggersSignoutIfAlreadySignedIn() {
        // In case an error is fired because cookies are taking longer to generate than usual,
        // if user retries the sign-in from the error screen, we need to sign out the user
        // first before signing in again.
        mDelegate.signIn(TEST_EMAIL, error -> {});
        when(mIdentityManagerMock.hasPrimaryAccount(anyInt())).thenReturn(true);

        mDelegate.signIn(TEST_EMAIL, error -> {});
        InOrder calledInOrder =
                inOrder(
                        mWebSigninBridgeMock,
                        mSigninManagerMock,
                        mWebSigninBridgeFactoryMock,
                        mSigninManagerMock);
        calledInOrder.verify(mWebSigninBridgeMock).destroy();
        calledInOrder.verify(mSigninManagerMock).signOut(anyInt());
        calledInOrder
                .verify(mWebSigninBridgeFactoryMock)
                .create(eq(mProfileMock), eq(mCoreAccountInfo), any());
        calledInOrder
                .verify(mSigninManagerMock)
                .signin(
                        eq(AccountUtils.createAccountFromName(TEST_EMAIL)),
                        eq(SigninAccessPoint.WEB_SIGNIN),
                        any());
    }

    @Test
    public void testSignInFailedWithConnectionError() {
        Callback<GoogleServiceAuthError> mockCallback = mock(Callback.class);
        GoogleServiceAuthError error = new GoogleServiceAuthError(State.CONNECTION_FAILED);
        mDelegate.signIn(TEST_EMAIL, mockCallback);
        verify(mWebSigninBridgeFactoryMock)
                .create(
                        eq(mProfileMock),
                        eq(mCoreAccountInfo),
                        mWebSigninBridgeListenerCaptor.capture());
        mWebSigninBridgeListenerCaptor.getValue().onSigninFailed(error);
        verify(mockCallback).onResult(error);
        // WebSigninBridge should be kept alive in case cookies are taking longer to
        // generate than usual
        verify(mWebSigninBridgeMock, never()).destroy();
    }
}
