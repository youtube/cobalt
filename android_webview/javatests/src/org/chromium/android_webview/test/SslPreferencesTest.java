// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.net.http.SslError;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.UseParametersRunnerFactory;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.test.TestAwContentsClient.OnReceivedSslErrorHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.net.test.ServerCertificate;

/** SslError tests. */
@RunWith(Parameterized.class)
@UseParametersRunnerFactory(AwJUnit4ClassRunnerWithParameters.Factory.class)
public class SslPreferencesTest extends AwParameterizedTest {
    @Rule public AwActivityTestRule mActivityTestRule;

    private AwTestContainerView mTestContainerView;
    private TestAwContentsClient mContentsClient;
    private AwTestContainerView mContainerView;
    private AwContents mAwContents;
    private EmbeddedTestServer mTestServer;

    private static final String HELLO_WORLD_HTML = "/android_webview/test/data/hello_world.html";
    private static final String HELLO_WORLD_TITLE = "Hello, World!";

    public SslPreferencesTest(AwSettingsMutation param) {
        this.mActivityTestRule = new AwActivityTestRule(param.getMutation());
    }

    @Before
    public void setUp() {
        mContentsClient = new TestAwContentsClient();
        mTestContainerView = mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient);
        mAwContents = mTestContainerView.getAwContents();
    }

    @Test
    @Feature({"AndroidWebView"})
    @SmallTest
    public void testSslErrorNotCalledForOkCert() throws Throwable {
        mTestServer =
                EmbeddedTestServer.createAndStartHTTPSServer(
                        InstrumentationRegistry.getInstrumentation().getContext(),
                        ServerCertificate.CERT_OK);
        final String pageUrl = mTestServer.getURL(HELLO_WORLD_HTML);
        final OnReceivedSslErrorHelper onReceivedSslErrorHelper =
                mContentsClient.getOnReceivedSslErrorHelper();
        int onSslErrorCallCount = onReceivedSslErrorHelper.getCallCount();
        mContentsClient.setAllowSslError(true);
        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), pageUrl);
        if (onSslErrorCallCount != onReceivedSslErrorHelper.getCallCount()) {
            Assert.fail(
                    "onReceivedSslError should not be called, but was called with error "
                            + onReceivedSslErrorHelper.getError());
        }
    }

    @Test
    @Feature({"AndroidWebView"})
    @SmallTest
    public void testSslErrorMismatchedName() throws Throwable {
        mTestServer =
                EmbeddedTestServer.createAndStartHTTPSServer(
                        InstrumentationRegistry.getInstrumentation().getContext(),
                        ServerCertificate.CERT_MISMATCHED_NAME);
        final String pageUrl = mTestServer.getURL(HELLO_WORLD_HTML);
        final OnReceivedSslErrorHelper onReceivedSslErrorHelper =
                mContentsClient.getOnReceivedSslErrorHelper();
        int onSslErrorCallCount = onReceivedSslErrorHelper.getCallCount();
        mContentsClient.setAllowSslError(true);
        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), pageUrl);
        Assert.assertEquals(
                "onReceivedSslError should be called once",
                onSslErrorCallCount + 1,
                onReceivedSslErrorHelper.getCallCount());
        SslError error = onReceivedSslErrorHelper.getError();
        Assert.assertTrue("Expected SSL_IDMISMATCH", error.hasError(SslError.SSL_IDMISMATCH));
    }

    @Test
    @Feature({"AndroidWebView"})
    @SmallTest
    public void testSslErrorInvalidDate() throws Throwable {
        mTestServer =
                EmbeddedTestServer.createAndStartHTTPSServer(
                        InstrumentationRegistry.getInstrumentation().getContext(),
                        // WebView currently returns DATE_INVALID instead of SSL_EXPIRED (see
                        // SslUtil.java).
                        ServerCertificate.CERT_EXPIRED);
        final String pageUrl = mTestServer.getURL(HELLO_WORLD_HTML);
        final OnReceivedSslErrorHelper onReceivedSslErrorHelper =
                mContentsClient.getOnReceivedSslErrorHelper();
        int onSslErrorCallCount = onReceivedSslErrorHelper.getCallCount();
        mContentsClient.setAllowSslError(true);
        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), pageUrl);
        Assert.assertEquals(
                "onReceivedSslError should be called once",
                onSslErrorCallCount + 1,
                onReceivedSslErrorHelper.getCallCount());
        SslError error = onReceivedSslErrorHelper.getError();
        Assert.assertTrue("Expected SSL_DATE_INVALID", error.hasError(SslError.SSL_DATE_INVALID));
    }

    @Test
    @Feature({"AndroidWebView"})
    @SmallTest
    public void testSslErrorCommonNameOnly() throws Throwable {
        mTestServer =
                EmbeddedTestServer.createAndStartHTTPSServer(
                        InstrumentationRegistry.getInstrumentation().getContext(),
                        ServerCertificate.CERT_COMMON_NAME_ONLY);
        final String pageUrl = mTestServer.getURL(HELLO_WORLD_HTML);
        final OnReceivedSslErrorHelper onReceivedSslErrorHelper =
                mContentsClient.getOnReceivedSslErrorHelper();
        int onSslErrorCallCount = onReceivedSslErrorHelper.getCallCount();
        mContentsClient.setAllowSslError(true);
        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), pageUrl);
        Assert.assertEquals(
                "onReceivedSslError should be called once",
                onSslErrorCallCount + 1,
                onReceivedSslErrorHelper.getCallCount());
        SslError error = onReceivedSslErrorHelper.getError();
        Assert.assertTrue("Expected SSL_IDMISMATCH", error.hasError(SslError.SSL_IDMISMATCH));
    }

    // onReceivedSslError() does not imply any callbacks other than onPageFinished().
    @Test
    @Feature({"AndroidWebView"})
    @SmallTest
    public void testCancelSslErrorDoesNotCallOtherCallbacks() throws Throwable {
        mTestServer =
                EmbeddedTestServer.createAndStartHTTPSServer(
                        InstrumentationRegistry.getInstrumentation().getContext(),
                        ServerCertificate.CERT_EXPIRED);
        final String pageUrl = mTestServer.getURL(HELLO_WORLD_HTML);
        final OnReceivedSslErrorHelper onReceivedSslErrorHelper =
                mContentsClient.getOnReceivedSslErrorHelper();
        int onSslErrorCallCount = onReceivedSslErrorHelper.getCallCount();
        int errorCount = mContentsClient.getOnReceivedErrorHelper().getCallCount();
        int httpErrorCount = mContentsClient.getOnReceivedHttpErrorHelper().getCallCount();
        // Load the page and cancel the SslError
        mContentsClient.setAllowSslError(false);
        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), pageUrl);
        Assert.assertEquals(
                "onReceivedSslError should be called once",
                onSslErrorCallCount + 1,
                onReceivedSslErrorHelper.getCallCount());
        Assert.assertEquals(
                "Canceled SslErrors should not trigger network errors",
                errorCount,
                mContentsClient.getOnReceivedErrorHelper().getCallCount());
        Assert.assertEquals(
                "Canceled SslErrors should not trigger HTTP errors",
                httpErrorCount,
                mContentsClient.getOnReceivedHttpErrorHelper().getCallCount());
        // Same thing, but allow the SslError this time
        mContentsClient.setAllowSslError(true);
        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), pageUrl);
        Assert.assertEquals(
                "onReceivedSslError should be called a second time",
                onSslErrorCallCount + 2,
                onReceivedSslErrorHelper.getCallCount());
        Assert.assertEquals(
                "Allowed SslErrors should not trigger network errors",
                errorCount,
                mContentsClient.getOnReceivedErrorHelper().getCallCount());
        Assert.assertEquals(
                "Allowed SslErrors should not trigger HTTP errors",
                httpErrorCount,
                mContentsClient.getOnReceivedHttpErrorHelper().getCallCount());
    }

    @Test
    @Feature({"AndroidWebView"})
    @SmallTest
    public void testAllowSslErrorShowsPage() throws Throwable {
        mTestServer =
                EmbeddedTestServer.createAndStartHTTPSServer(
                        InstrumentationRegistry.getInstrumentation().getContext(),
                        ServerCertificate.CERT_EXPIRED);
        final String pageUrl = mTestServer.getURL(HELLO_WORLD_HTML);
        final OnReceivedSslErrorHelper onReceivedSslErrorHelper =
                mContentsClient.getOnReceivedSslErrorHelper();
        int onSslErrorCallCount = onReceivedSslErrorHelper.getCallCount();
        mContentsClient.setAllowSslError(true);
        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), pageUrl);
        Assert.assertEquals(
                "onReceivedSslError should be called once",
                onSslErrorCallCount + 1,
                onReceivedSslErrorHelper.getCallCount());
        // Assert the page has successfully loaded, which can be indicated by changing the page
        // title.
        Assert.assertEquals(
                "Page has loaded and set the title",
                HELLO_WORLD_TITLE,
                mActivityTestRule.getTitleOnUiThread(mAwContents));
    }

    @Test
    @Feature({"AndroidWebView"})
    @SmallTest
    public void testCancelSslErrorBlocksPage() throws Throwable {
        mTestServer =
                EmbeddedTestServer.createAndStartHTTPSServer(
                        InstrumentationRegistry.getInstrumentation().getContext(),
                        ServerCertificate.CERT_EXPIRED);
        final String pageUrl = mTestServer.getURL(HELLO_WORLD_HTML);
        final OnReceivedSslErrorHelper onReceivedSslErrorHelper =
                mContentsClient.getOnReceivedSslErrorHelper();
        int onSslErrorCallCount = onReceivedSslErrorHelper.getCallCount();
        mContentsClient.setAllowSslError(false);
        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), pageUrl);
        Assert.assertEquals(
                "onReceivedSslError should be called once",
                onSslErrorCallCount + 1,
                onReceivedSslErrorHelper.getCallCount());
        // Assert the page did not load. This is generally hard to check, so we instead check
        // that the title is the empty string (as the real HTML sets the title to
        // HELLO_WORLD_TITLE).
        Assert.assertEquals(
                "Page should not be loaded and title should be empty",
                "",
                mActivityTestRule.getTitleOnUiThread(mAwContents));
    }

    // If the user allows the ssl error, the same ssl error will not trigger the onReceivedSslError
    // callback.
    @Test
    @Feature({"AndroidWebView"})
    @SmallTest
    public void testAllowSslErrorIsRemembered() throws Throwable {
        mTestServer =
                EmbeddedTestServer.createAndStartHTTPSServer(
                        InstrumentationRegistry.getInstrumentation().getContext(),
                        ServerCertificate.CERT_EXPIRED);
        final String pageUrl = mTestServer.getURL(HELLO_WORLD_HTML);
        final OnReceivedSslErrorHelper onReceivedSslErrorHelper =
                mContentsClient.getOnReceivedSslErrorHelper();
        int onSslErrorCallCount = onReceivedSslErrorHelper.getCallCount();
        mContentsClient.setAllowSslError(true);
        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), pageUrl);
        Assert.assertEquals(
                "onReceivedSslError should be called once",
                onSslErrorCallCount + 1,
                onReceivedSslErrorHelper.getCallCount());
        // Now load the page again. This time, we expect no ssl error, because
        // user's decision should be remembered.
        onSslErrorCallCount = onReceivedSslErrorHelper.getCallCount();
        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), pageUrl);
        Assert.assertEquals(
                "onReceivedSslError should not be called again",
                onSslErrorCallCount,
                onReceivedSslErrorHelper.getCallCount());
    }

    // If the user cancels the ssl error, the same ssl error should trigger the onReceivedSslError
    // callback again.
    @Test
    @Feature({"AndroidWebView"})
    @SmallTest
    public void testCancelSslErrorIsRemembered() throws Throwable {
        mTestServer =
                EmbeddedTestServer.createAndStartHTTPSServer(
                        InstrumentationRegistry.getInstrumentation().getContext(),
                        ServerCertificate.CERT_EXPIRED);
        final String pageUrl = mTestServer.getURL(HELLO_WORLD_HTML);
        final OnReceivedSslErrorHelper onReceivedSslErrorHelper =
                mContentsClient.getOnReceivedSslErrorHelper();
        int onSslErrorCallCount = onReceivedSslErrorHelper.getCallCount();
        mContentsClient.setAllowSslError(false);
        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), pageUrl);
        Assert.assertEquals(
                "onReceivedSslError should be called once",
                onSslErrorCallCount + 1,
                onReceivedSslErrorHelper.getCallCount());
        SslError error = onReceivedSslErrorHelper.getError();
        Assert.assertTrue("Expected SSL_DATE_INVALID", error.hasError(SslError.SSL_DATE_INVALID));
        // Now load the page again. This time, we expect the same ssl error, because
        // user's decision should be remembered.
        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), pageUrl);
        Assert.assertEquals(
                "onReceivedSslError should be called a second time",
                onSslErrorCallCount + 2,
                onReceivedSslErrorHelper.getCallCount());
        // And that error should have the same error code.
        error = onReceivedSslErrorHelper.getError();
        Assert.assertTrue("Expected SSL_DATE_INVALID", error.hasError(SslError.SSL_DATE_INVALID));
    }
}
