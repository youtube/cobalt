// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import android.content.Context;
import android.text.Spannable;
import android.text.SpannableStringBuilder;

import androidx.test.filters.MediumTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.UiThreadTest;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.test.ChromeBrowserTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.components.omnibox.OmniboxUrlEmphasizer;
import org.chromium.components.omnibox.OmniboxUrlEmphasizer.UrlEmphasisColorSpan;
import org.chromium.components.omnibox.OmniboxUrlEmphasizer.UrlEmphasisSecurityErrorSpan;
import org.chromium.components.omnibox.OmniboxUrlEmphasizer.UrlEmphasisSpan;
import org.chromium.components.security_state.ConnectionSecurityLevel;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.Arrays;
import java.util.Comparator;

/**
 * Unit tests for OmniboxUrlEmphasizer that ensure various types of URLs are emphasized and colored
 * correctly.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
public class OmniboxUrlEmphasizerTest {
    @Rule public final ChromeBrowserTestRule mChromeBrowserTestRule = new ChromeBrowserTestRule();

    private Profile mProfile;
    private ChromeAutocompleteSchemeClassifier mChromeAutocompleteSchemeClassifier;
    private Context mContext;

    @Before
    public void setUp() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mProfile = Profile.getLastUsedRegularProfile();
                    mChromeAutocompleteSchemeClassifier =
                            new ChromeAutocompleteSchemeClassifier(mProfile);
                    mContext = InstrumentationRegistry.getInstrumentation().getTargetContext();
                });
    }

    @After
    public void tearDown() {
        mChromeAutocompleteSchemeClassifier.destroy();
    }

    /** Convenience class for testing a URL emphasized by OmniboxUrlEmphasizer.emphasizeUrl(). */
    private static class EmphasizedUrlSpanHelper {
        UrlEmphasisSpan mSpan;
        Spannable mParent;

        private EmphasizedUrlSpanHelper(UrlEmphasisSpan span, Spannable parent) {
            mSpan = span;
            mParent = parent;
        }

        private String getContents() {
            return mParent.subSequence(getStartIndex(), getEndIndex()).toString();
        }

        private int getStartIndex() {
            return mParent.getSpanStart(mSpan);
        }

        private int getEndIndex() {
            return mParent.getSpanEnd(mSpan);
        }

        private String getClassName() {
            return mSpan.getClass().getSimpleName();
        }

        private int getColorForColoredSpan() {
            return ((UrlEmphasisColorSpan) mSpan).getForegroundColor();
        }

        public static EmphasizedUrlSpanHelper[] getSpansForEmphasizedUrl(Spannable emphasizedUrl) {
            UrlEmphasisSpan[] existingSpans = OmniboxUrlEmphasizer.getEmphasisSpans(emphasizedUrl);
            EmphasizedUrlSpanHelper[] helperSpans =
                    new EmphasizedUrlSpanHelper[existingSpans.length];
            for (int i = 0; i < existingSpans.length; i++) {
                helperSpans[i] = new EmphasizedUrlSpanHelper(existingSpans[i], emphasizedUrl);
            }
            return helperSpans;
        }

        public void assertIsColoredSpan(String contents, int startIndex, int color) {
            Assert.assertEquals("Unexpected span contents:", contents, getContents());
            Assert.assertEquals(
                    "Unexpected starting index for '" + contents + "' span:",
                    startIndex,
                    getStartIndex());
            Assert.assertEquals(
                    "Unexpected ending index for '" + contents + "' span:",
                    startIndex + contents.length(),
                    getEndIndex());
            Assert.assertEquals(
                    "Unexpected class for '" + contents + "' span:",
                    UrlEmphasisColorSpan.class.getSimpleName(),
                    getClassName());
            Assert.assertEquals(
                    "Unexpected color for '" + contents + "' span:",
                    color,
                    getColorForColoredSpan());
        }

        public void assertIsStrikethroughSpan(String contents, int startIndex) {
            Assert.assertEquals("Unexpected span contents:", contents, getContents());
            Assert.assertEquals(
                    "Unexpected starting index for '" + contents + "' span:",
                    startIndex,
                    getStartIndex());
            Assert.assertEquals(
                    "Unexpected ending index for '" + contents + "' span:",
                    startIndex + contents.length(),
                    getEndIndex());
            Assert.assertEquals(
                    "Unexpected class for '" + contents + "' span:",
                    UrlEmphasisSecurityErrorSpan.class.getSimpleName(),
                    getClassName());
        }
    }

    /**
     * Wraps EmphasizedUrlHelper.getSpansForEmphasizedUrl and sorts spans to fix an Android N bug:
     * https://code.google.com/p/android/issues/detail?id=229861.
     */
    private EmphasizedUrlSpanHelper[] getSpansForEmphasizedUrl(Spannable url) {
        EmphasizedUrlSpanHelper[] spans = EmphasizedUrlSpanHelper.getSpansForEmphasizedUrl(url);
        Arrays.sort(
                spans,
                new Comparator<EmphasizedUrlSpanHelper>() {
                    @Override
                    public int compare(EmphasizedUrlSpanHelper o1, EmphasizedUrlSpanHelper o2) {
                        return o1.getStartIndex() - o2.getStartIndex();
                    }
                });
        return spans;
    }

    /**
     * Verify that a short, secure HTTPS URL is colored correctly by
     * OmniboxUrlEmphasizer.emphasizeUrl().
     */
    @Test
    @MediumTest
    @UiThreadTest
    @Feature({"Browser", "Main"})
    public void testShortSecureHTTPSUrl() {
        Spannable url = new SpannableStringBuilder("https://www.google.com/");
        OmniboxUrlEmphasizer.emphasizeUrl(
                url,
                mContext,
                mChromeAutocompleteSchemeClassifier,
                ConnectionSecurityLevel.SECURE,
                true,
                true);
        EmphasizedUrlSpanHelper[] spans = getSpansForEmphasizedUrl(url);

        Assert.assertEquals("Unexpected number of spans:", 4, spans.length);
        spans[0].assertIsColoredSpan("https", 0, mContext.getColor(R.color.google_green_600));
        spans[1].assertIsColoredSpan(
                "://", 5, mContext.getColor(R.color.url_emphasis_non_emphasized_text));
        spans[2].assertIsColoredSpan(
                "www.google.com", 8, mContext.getColor(R.color.url_emphasis_emphasized_text));
        spans[3].assertIsColoredSpan(
                "/", 22, mContext.getColor(R.color.url_emphasis_non_emphasized_text));
    }

    /**
     * Verify that a short, secure HTTPS URL is colored correctly with light colors by
     * OmniboxUrlEmphasizer.emphasizeUrl().
     */
    @Test
    @MediumTest
    @UiThreadTest
    @Feature({"Browser", "Main"})
    public void testShortSecureHTTPSUrlWithLightColors() {
        Spannable url = new SpannableStringBuilder("https://www.google.com/");
        OmniboxUrlEmphasizer.emphasizeUrl(
                url,
                mContext,
                mChromeAutocompleteSchemeClassifier,
                ConnectionSecurityLevel.SECURE,
                false,
                false);
        EmphasizedUrlSpanHelper[] spans = getSpansForEmphasizedUrl(url);

        Assert.assertEquals("Unexpected number of spans:", 4, spans.length);
        spans[0].assertIsColoredSpan(
                "https", 0, mContext.getColor(R.color.url_emphasis_light_non_emphasized_text));
        spans[1].assertIsColoredSpan(
                "://", 5, mContext.getColor(R.color.url_emphasis_light_non_emphasized_text));
        spans[2].assertIsColoredSpan(
                "www.google.com", 8, mContext.getColor(R.color.url_emphasis_light_emphasized_text));
        spans[3].assertIsColoredSpan(
                "/", 22, mContext.getColor(R.color.url_emphasis_light_non_emphasized_text));
    }

    /**
     * Verify that a long, insecure HTTPS URL is colored correctly by
     * OmniboxUrlEmphasizer.emphasizeUrl().
     */
    @Test
    @MediumTest
    @UiThreadTest
    @Feature({"Browser", "Main"})
    public void testLongInsecureHTTPSUrl() {
        Spannable url =
                new SpannableStringBuilder("https://www.google.com/q?query=abc123&results=1");
        OmniboxUrlEmphasizer.emphasizeUrl(
                url,
                mContext,
                mChromeAutocompleteSchemeClassifier,
                ConnectionSecurityLevel.DANGEROUS,
                true,
                true);
        EmphasizedUrlSpanHelper[] spans = getSpansForEmphasizedUrl(url);

        Assert.assertEquals("Unexpected number of spans:", 5, spans.length);
        spans[0].assertIsStrikethroughSpan("https", 0);
        spans[1].assertIsColoredSpan("https", 0, mContext.getColor(R.color.google_red_600));
        spans[2].assertIsColoredSpan(
                "://", 5, mContext.getColor(R.color.url_emphasis_non_emphasized_text));
        spans[3].assertIsColoredSpan(
                "www.google.com", 8, mContext.getColor(R.color.url_emphasis_emphasized_text));
        spans[4].assertIsColoredSpan(
                "/q?query=abc123&results=1",
                22,
                mContext.getColor(R.color.url_emphasis_non_emphasized_text));
    }

    /**
     * Verify that a very short, HTTP Warning URL is colored correctly by
     * OmniboxUrlEmphasizer.emphasizeUrl().
     */
    @Test
    @MediumTest
    @UiThreadTest
    @Feature({"Browser", "Main"})
    public void testVeryShortHTTPWarningUrl() {
        Spannable url = new SpannableStringBuilder("m.w.co/p");
        OmniboxUrlEmphasizer.emphasizeUrl(
                url,
                mContext,
                mChromeAutocompleteSchemeClassifier,
                ConnectionSecurityLevel.WARNING,
                true,
                false);
        EmphasizedUrlSpanHelper[] spans = getSpansForEmphasizedUrl(url);

        Assert.assertEquals("Unexpected number of spans:", 2, spans.length);
        spans[0].assertIsColoredSpan(
                "m.w.co", 0, mContext.getColor(R.color.url_emphasis_emphasized_text));
        spans[1].assertIsColoredSpan(
                "/p", 6, mContext.getColor(R.color.url_emphasis_non_emphasized_text));
    }

    /**
     * Verify that an internal 'about:' page is colored correctly by
     * OmniboxUrlEmphasizer.emphasizeUrl().
     */
    @Test
    @MediumTest
    @UiThreadTest
    @Feature({"Browser", "Main"})
    public void testAboutPageUrl() {
        Spannable url = new SpannableStringBuilder("about:blank");
        OmniboxUrlEmphasizer.emphasizeUrl(
                url,
                mContext,
                mChromeAutocompleteSchemeClassifier,
                ConnectionSecurityLevel.NONE,
                true,
                true);
        EmphasizedUrlSpanHelper[] spans = getSpansForEmphasizedUrl(url);

        Assert.assertEquals("Unexpected number of spans:", 3, spans.length);
        spans[0].assertIsColoredSpan(
                "about", 0, mContext.getColor(R.color.url_emphasis_non_emphasized_text));
        spans[1].assertIsColoredSpan(
                ":", 5, mContext.getColor(R.color.url_emphasis_non_emphasized_text));
        spans[2].assertIsColoredSpan(
                "blank", 6, mContext.getColor(R.color.url_emphasis_emphasized_text));
    }

    /** Verify that a 'data:' URL is colored correctly by OmniboxUrlEmphasizer.emphasizeUrl(). */
    @Test
    @MediumTest
    @UiThreadTest
    @Feature({"Browser", "Main"})
    public void testDataUrl() {
        Spannable url =
                new SpannableStringBuilder("data:text/plain;charset=utf-8;base64,VGVzdCBVUkw=");
        OmniboxUrlEmphasizer.emphasizeUrl(
                url,
                mContext,
                mChromeAutocompleteSchemeClassifier,
                ConnectionSecurityLevel.NONE,
                true,
                true);
        EmphasizedUrlSpanHelper[] spans = getSpansForEmphasizedUrl(url);

        Assert.assertEquals("Unexpected number of spans:", 2, spans.length);
        spans[0].assertIsColoredSpan("data", 0, mContext.getColor(R.color.default_text_color_dark));
        spans[1].assertIsColoredSpan(
                ":text/plain;charset=utf-8;base64,VGVzdCBVUkw=",
                4,
                mContext.getColor(R.color.url_emphasis_non_emphasized_text));
    }

    /**
     * Verify that an internal 'chrome://' page is colored correctly by
     * OmniboxUrlEmphasizer.emphasizeUrl().
     */
    @Test
    @MediumTest
    @UiThreadTest
    @Feature({"Browser", "Main"})
    public void testInternalChromePageUrl() {
        Spannable url = new SpannableStringBuilder("chrome://bookmarks");
        OmniboxUrlEmphasizer.emphasizeUrl(
                url,
                mContext,
                mChromeAutocompleteSchemeClassifier,
                ConnectionSecurityLevel.NONE,
                true,
                true);
        EmphasizedUrlSpanHelper[] spans = getSpansForEmphasizedUrl(url);

        Assert.assertEquals("Unexpected number of spans:", 3, spans.length);
        spans[0].assertIsColoredSpan(
                "chrome", 0, mContext.getColor(R.color.url_emphasis_non_emphasized_text));
        spans[1].assertIsColoredSpan(
                "://", 6, mContext.getColor(R.color.url_emphasis_non_emphasized_text));
        spans[2].assertIsColoredSpan(
                "bookmarks", 9, mContext.getColor(R.color.url_emphasis_emphasized_text));
    }

    /**
     * Verify that an internal 'chrome-native://' page is colored correctly by
     * OmniboxUrlEmphasizer.emphasizeUrl().
     */
    @Test
    @MediumTest
    @UiThreadTest
    @Feature({"Browser", "Main"})
    public void testInternalChromeNativePageUrl() {
        Spannable url = new SpannableStringBuilder("chrome-native://bookmarks");
        OmniboxUrlEmphasizer.emphasizeUrl(
                url,
                mContext,
                mChromeAutocompleteSchemeClassifier,
                ConnectionSecurityLevel.NONE,
                true,
                true);
        EmphasizedUrlSpanHelper[] spans = getSpansForEmphasizedUrl(url);

        Assert.assertEquals("Unexpected number of spans:", 3, spans.length);
        spans[0].assertIsColoredSpan(
                "chrome-native", 0, mContext.getColor(R.color.url_emphasis_non_emphasized_text));
        spans[1].assertIsColoredSpan(
                "://", 13, mContext.getColor(R.color.url_emphasis_non_emphasized_text));
        spans[2].assertIsColoredSpan(
                "bookmarks", 16, mContext.getColor(R.color.url_emphasis_emphasized_text));
    }

    /** Verify that an invalid URL is colored correctly by OmniboxUrlEmphasizer.emphasizeUrl(). */
    @Test
    @MediumTest
    @UiThreadTest
    @Feature({"Browser", "Main"})
    public void testInvalidUrl() {
        Spannable url = new SpannableStringBuilder("invalidurl");
        OmniboxUrlEmphasizer.emphasizeUrl(
                url,
                mContext,
                mChromeAutocompleteSchemeClassifier,
                ConnectionSecurityLevel.NONE,
                true,
                true);
        EmphasizedUrlSpanHelper[] spans = getSpansForEmphasizedUrl(url);

        Assert.assertEquals("Unexpected number of spans:", 1, spans.length);
        spans[0].assertIsColoredSpan(
                "invalidurl", 0, mContext.getColor(R.color.url_emphasis_emphasized_text));
    }

    /**
     * Verify that an empty URL is processed correctly by OmniboxUrlEmphasizer.emphasizeUrl().
     * Regression test for crbug.com/700769
     */
    @Test
    @MediumTest
    @UiThreadTest
    @Feature({"Browser", "Main"})
    public void testEmptyUrl() {
        Spannable url = new SpannableStringBuilder("");
        OmniboxUrlEmphasizer.emphasizeUrl(
                url,
                mContext,
                mChromeAutocompleteSchemeClassifier,
                ConnectionSecurityLevel.NONE,
                true,
                true);
        EmphasizedUrlSpanHelper[] spans = getSpansForEmphasizedUrl(url);

        Assert.assertEquals("Unexpected number of spans:", 0, spans.length);
    }

    /**
     * Verify that the origin index is calculated correctly for HTTP and HTTPS URLs by
     * OmniboxUrlEmphasizer.getOriginEndIndex().
     */
    @Test
    @MediumTest
    @UiThreadTest
    @Feature({"Browser", "Main"})
    public void testHTTPAndHTTPSUrlsOriginEndIndex() {
        String url;

        url = "http://www.google.com/";
        Assert.assertEquals(
                "Unexpected origin end index for url " + url + ":",
                "http://www.google.com".length(),
                OmniboxUrlEmphasizer.getOriginEndIndex(url, mChromeAutocompleteSchemeClassifier));

        url = "https://www.google.com/";
        Assert.assertEquals(
                "Unexpected origin end index for url " + url + ":",
                "https://www.google.com".length(),
                OmniboxUrlEmphasizer.getOriginEndIndex(url, mChromeAutocompleteSchemeClassifier));

        url = "http://www.news.com/dir/a/b/c/page.html?foo=bar";
        Assert.assertEquals(
                "Unexpected origin end index for url " + url + ":",
                "http://www.news.com".length(),
                OmniboxUrlEmphasizer.getOriginEndIndex(url, mChromeAutocompleteSchemeClassifier));

        url = "http://www.test.com?foo=bar";
        Assert.assertEquals(
                "Unexpected origin end index for url " + url + ":",
                "http://www.test.com".length(),
                OmniboxUrlEmphasizer.getOriginEndIndex(url, mChromeAutocompleteSchemeClassifier));
    }

    /**
     * Verify that the origin index is calculated correctly for data URLs by
     * OmniboxUrlEmphasizer.getOriginEndIndex().
     */
    @Test
    @MediumTest
    @UiThreadTest
    @Feature({"Browser", "Main"})
    public void testDataUrlsOriginEndIndex() {
        String url;

        // Data URLs have no origin.
        url = "data:ABC123";
        Assert.assertEquals(
                "Unexpected origin end index for url " + url + ":",
                0,
                OmniboxUrlEmphasizer.getOriginEndIndex(url, mChromeAutocompleteSchemeClassifier));

        url = "data:kf94hfJEj#N";
        Assert.assertEquals(
                "Unexpected origin end index for url " + url + ":",
                0,
                OmniboxUrlEmphasizer.getOriginEndIndex(url, mChromeAutocompleteSchemeClassifier));

        url = "data:text/plain;charset=utf-8;base64,dGVzdA==";
        Assert.assertEquals(
                "Unexpected origin end index for url " + url + ":",
                0,
                OmniboxUrlEmphasizer.getOriginEndIndex(url, mChromeAutocompleteSchemeClassifier));
    }

    /**
     * Verify that the origin index is calculated correctly for URLS other than HTTP, HTTPS and data
     * by OmniboxUrlEmphasizer.getOriginEndIndex().
     */
    @Test
    @MediumTest
    @UiThreadTest
    @Feature({"Browser", "Main"})
    public void testOtherUrlsOriginEndIndex() {
        String url;

        // In non-HTTP/HTTPS/data URLs, the whole URL is considered the origin.
        url = "file://my/pc/somewhere/foo.html";
        Assert.assertEquals(
                "Unexpected origin end index for url " + url + ":",
                url.length(),
                OmniboxUrlEmphasizer.getOriginEndIndex(url, mChromeAutocompleteSchemeClassifier));

        url = "about:blank";
        Assert.assertEquals(
                "Unexpected origin end index for url " + url + ":",
                url.length(),
                OmniboxUrlEmphasizer.getOriginEndIndex(url, mChromeAutocompleteSchemeClassifier));

        url = "chrome://version";
        Assert.assertEquals(
                "Unexpected origin end index for url " + url + ":",
                url.length(),
                OmniboxUrlEmphasizer.getOriginEndIndex(url, mChromeAutocompleteSchemeClassifier));

        url = "chrome-native://bookmarks";
        Assert.assertEquals(
                "Unexpected origin end index for url " + url + ":",
                url.length(),
                OmniboxUrlEmphasizer.getOriginEndIndex(url, mChromeAutocompleteSchemeClassifier));

        url = "invalidurl";
        Assert.assertEquals(
                "Unexpected origin end index for url " + url + ":",
                url.length(),
                OmniboxUrlEmphasizer.getOriginEndIndex(url, mChromeAutocompleteSchemeClassifier));
    }
}
