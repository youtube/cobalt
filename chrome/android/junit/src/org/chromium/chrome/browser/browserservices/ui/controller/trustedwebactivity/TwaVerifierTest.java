// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.ui.controller.trustedwebactivity;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.Promise;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.browserservices.verification.ChromeOriginVerifier;
import org.chromium.chrome.browser.browserservices.verification.ChromeOriginVerifierFactory;
import org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityTabProvider;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.components.content_relationship_verification.OriginVerifier.OriginVerificationListener;
import org.chromium.components.embedder_support.util.Origin;

import java.util.Collections;
import java.util.HashSet;

/** Tests for {@link TwaVerifier}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TwaVerifierTest {
    private static final String INITIAL_URL = "https://www.initialurl.com/page.html";
    private static final String ADDITIONAL_ORIGIN = "https://www.otherverifiedorigin.com";
    private static final String OTHER_URL = "https://www.notverifiedurl.com/page2.html";

    @Mock ActivityLifecycleDispatcher mLifecycleDispatcher;
    @Mock CustomTabIntentDataProvider mIntentDataProvider;
    @Mock ChromeOriginVerifier mOriginVerifier;
    @Mock CustomTabActivityTabProvider mActivityTabProvider;
    @Mock ClientPackageNameProvider mClientPackageNameProvider;

    private TwaVerifier mDelegate;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        when(mIntentDataProvider.getUrlToLoad()).thenReturn(INITIAL_URL);
        HashSet<Origin> trustedOrigins = new HashSet<Origin>();
        Collections.addAll(
                trustedOrigins, Origin.create(INITIAL_URL), Origin.create(ADDITIONAL_ORIGIN));
        when(mIntentDataProvider.getAllTrustedWebActivityOrigins()).thenReturn(trustedOrigins);
        ChromeOriginVerifierFactory.setInstanceForTesting(mOriginVerifier);

        when(mClientPackageNameProvider.get()).thenReturn("some.package.name");

        mDelegate =
                new TwaVerifier(
                        mLifecycleDispatcher,
                        mIntentDataProvider,
                        mClientPackageNameProvider,
                        mActivityTabProvider);
    }

    @Test
    public void verifiedScopeIsOrigin() {
        assertEquals(
                "https://www.example.com", mDelegate.getVerifiedScope("https://www.example.com"));
        assertEquals(
                "https://www.example.com",
                mDelegate.getVerifiedScope("https://www.example.com/page1.html"));
        assertEquals(
                "https://www.example.com",
                mDelegate.getVerifiedScope("https://www.example.com/dir/page2.html"));
    }

    @Test
    public void isPageInVerificationCache() {
        Origin trusted = Origin.create("https://www.trusted.com");
        Origin untrusted = Origin.create("https://www.untrusted.com");
        when(mOriginVerifier.wasPreviouslyVerified(eq(trusted))).thenReturn(true);
        when(mOriginVerifier.wasPreviouslyVerified(eq(untrusted))).thenReturn(false);

        assertTrue(mDelegate.wasPreviouslyVerified(trusted.toString()));
        assertFalse(mDelegate.wasPreviouslyVerified(untrusted.toString()));
    }

    @Test
    public void verify_failsInvalidUrl() {
        Promise<Boolean> result = mDelegate.verify("not-an-origin");
        assertFalse(result.getResult());
    }

    /** Tests that the first call to verify for a trusted origin performs full verification. */
    @Test
    public void verify_firstCall() {
        verifyFullCheck(INITIAL_URL);
    }

    /** Tests that subsequent calls to verify for a given origin just check the cache. */
    @Test
    public void verify_subsequentCalls() {
        verifyFullCheck(INITIAL_URL);
        verifyCacheCheck(INITIAL_URL);
    }

    /** Tests that things work as they should navigating to another verified origin and back. */
    @Test
    public void verify_acrossOriginSubsequentCalls() {
        verifyFullCheck(INITIAL_URL);
        verifyFullCheck(ADDITIONAL_ORIGIN);
        verifyCacheCheck(INITIAL_URL);
        verifyCacheCheck(ADDITIONAL_ORIGIN);
    }

    private void verifyFullCheck(String url) {
        Promise<Boolean> promise = mDelegate.verify(url);

        ArgumentCaptor<OriginVerificationListener> callback =
                ArgumentCaptor.forClass(OriginVerificationListener.class);
        verify(mOriginVerifier).start(callback.capture(), eq(Origin.create(url)));

        callback.getValue().onOriginVerified(null, null, true, true);

        assertTrue(promise.getResult());

        // So we don't interfere with the rest of the test.
        reset(mOriginVerifier);
    }

    private void verifyCacheCheck(String url) {
        Origin origin = Origin.create(url);

        when(mOriginVerifier.wasPreviouslyVerified(eq(origin))).thenReturn(true);

        Promise<Boolean> promise = mDelegate.verify(url);

        verify(mOriginVerifier, never()).start(any(), any());
        verify(mOriginVerifier).wasPreviouslyVerified(eq(origin));

        assertTrue(promise.getResult());

        // So we don't interfere with the rest of the test.
        reset(mOriginVerifier);
    }

    @Test
    public void verify_notTrustedOrigin() {
        Promise<Boolean> promise = mDelegate.verify(OTHER_URL);

        verify(mOriginVerifier, never()).start(any(), any());
        verify(mOriginVerifier).wasPreviouslyVerified(eq(Origin.create(OTHER_URL)));

        assertFalse(promise.getResult());
    }
}
