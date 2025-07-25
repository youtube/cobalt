// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import androidx.test.filters.LargeTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.blink.mojom.WebPage;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.schema_org.mojom.Entity;
import org.chromium.schema_org.mojom.Property;
import org.chromium.schema_org.mojom.Values;
import org.chromium.url.mojom.Url;

import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;

/** Tests Copyless Paste AppIndexing using instrumented tests. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
    "enable-features=CopylessPaste"
})
@Restriction(Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE)
public class CopylessPasteTest {
    @Rule
    public final ChromeTabbedActivityTestRule mActivityTestRule =
            new ChromeTabbedActivityTestRule();

    // The default timeout (in seconds) for a callback to wait.
    public static final long WAIT_TIMEOUT_SECONDS = 20L;

    // NODATA_PAGE doesn't contain desired metadata.
    private static final String NODATA_PAGE = "/chrome/test/data/android/about.html";

    // DATA_PAGE contains desired metadata.
    private static final String DATA_PAGE = "/chrome/test/data/android/appindexing/json-ld.html";

    private EmbeddedTestServer mTestServer;
    private CopylessHelper mCallbackHelper;

    @Before
    public void setUp() throws Exception {
        mTestServer = mActivityTestRule.getTestServer();

        mCallbackHelper = new CopylessHelper();

        AppIndexingUtil.setCallbackForTesting(webpage -> mCallbackHelper.notifyCalled(webpage));
        mActivityTestRule.startMainActivityOnBlankPage();
    }

    private static class CopylessHelper extends CallbackHelper {
        private WebPage mWebPage;

        public WebPage getWebPage() {
            return mWebPage;
        }

        public void notifyCalled(WebPage page) {
            mWebPage = page;
            notifyCalled();
        }
    }

    /** Tests that CopylessPaste is disabled in Incognito tabs. */
    @Test
    @LargeTest
    @Feature({"CopylessPaste"})
    public void testIncognito() {
        // Incognito tabs are ignored.
        mActivityTestRule.newIncognitoTabsFromMenu(1);
        mActivityTestRule.loadUrl(mTestServer.getURL(NODATA_PAGE));
        mActivityTestRule.loadUrl(mTestServer.getURL(DATA_PAGE));
        ChromeTabUtils.closeCurrentTab(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity());
        Assert.assertEquals(0, mCallbackHelper.getCallCount());
    }

    /** Tests that CopylessPaste skips invalid schemes. */
    @Test
    @LargeTest
    @Feature({"CopylessPaste"})
    public void testInvalidScheme() {
        // CopylessPaste only parses http and https.
        mActivityTestRule.loadUrl(UrlConstants.NTP_NON_NATIVE_URL);
        mActivityTestRule.loadUrl(UrlConstants.ABOUT_URL);
        Assert.assertEquals(0, mCallbackHelper.getCallCount());
    }

    /** Tests that CopylessPaste works on pages without desired metadata. */
    @Test
    @LargeTest
    @Feature({"CopylessPaste"})
    public void testNoMeta() throws TimeoutException {
        mActivityTestRule.loadUrl(mTestServer.getURL(NODATA_PAGE));
        mCallbackHelper.waitForCallback(0, 1, WAIT_TIMEOUT_SECONDS, TimeUnit.SECONDS);
        Assert.assertNull(mCallbackHelper.getWebPage());
    }

    /** Tests that CopylessPaste works end-to-end. */
    @Test
    @LargeTest
    @Feature({"CopylessPaste"})
    public void testValid() throws TimeoutException {
        mActivityTestRule.loadUrl(mTestServer.getURL(DATA_PAGE));
        mCallbackHelper.waitForCallback(0, 1, WAIT_TIMEOUT_SECONDS, TimeUnit.SECONDS);
        WebPage extracted = mCallbackHelper.getWebPage();

        WebPage expected = new WebPage();
        expected.url = new Url();
        expected.url.url = mTestServer.getURL(DATA_PAGE);
        expected.title = "JSON-LD for AppIndexing Test";
        Entity e = new Entity();
        e.type = "Hotel";
        e.properties = new Property[2];
        e.properties[0] = new Property();
        e.properties[0].name = "@context";
        e.properties[0].values = new Values();
        e.properties[0].values.setStringValues(new String[] {"http://schema.org"});
        e.properties[1] = new Property();
        e.properties[1].name = "name";
        e.properties[1].values = new Values();
        e.properties[1].values.setStringValues(new String[] {"Hotel Name"});
        expected.entities = new Entity[1];
        expected.entities[0] = e;
        Assert.assertEquals(expected.serialize(), extracted.serialize());
    }

    /** Tests that CopylessPaste skips parsing visited pages. */
    @Test
    @LargeTest
    @Feature({"CopylessPaste"})
    public void testCache() throws TimeoutException {
        mActivityTestRule.loadUrl(mTestServer.getURL(NODATA_PAGE));
        mCallbackHelper.waitForCallback(0, 1, WAIT_TIMEOUT_SECONDS, TimeUnit.SECONDS);
        mActivityTestRule.loadUrl(mTestServer.getURL(DATA_PAGE));
        mCallbackHelper.waitForCallback(1, 1, WAIT_TIMEOUT_SECONDS, TimeUnit.SECONDS);

        // Cache hit without entities. Shouldn't parse again.
        mActivityTestRule.loadUrl(mTestServer.getURL(NODATA_PAGE));
        // Cache hit with entities. Shouldn't parse again.
        mActivityTestRule.loadUrl(mTestServer.getURL(DATA_PAGE));
        Assert.assertEquals(2, mCallbackHelper.getCallCount());
    }
}
