// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.dom_distiller;

import static com.google.common.truth.Truth.assertThat;

import static org.hamcrest.Matchers.is;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.download.DownloadTestRule;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.transit.page.CtaPageStation;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.components.dom_distiller.core.DistilledPagePrefs;
import org.chromium.components.dom_distiller.core.DomDistillerService;
import org.chromium.content_public.browser.HostZoomMap;
import org.chromium.content_public.browser.test.util.TestCallbackHelperContainer;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.base.DeviceFormFactor;

import java.util.concurrent.TimeoutException;
import java.util.concurrent.atomic.AtomicReference;

/** End-to-end tests for Reader Mode (Simplified view). */
@RunWith(ChromeJUnit4ClassRunner.class)
@Restriction(DeviceFormFactor.PHONE)
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
    "--reader-mode-heuristics=alwaystrue"
})
public class ReaderModeTest {

    public final FreshCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();
    public final DownloadTestRule mDownloadTestRule = new DownloadTestRule();

    @Rule
    public final RuleChain mRuleChain =
            RuleChain.outerRule(mActivityTestRule).around(mDownloadTestRule);

    private static final String TEST_PAGE = "/chrome/test/data/dom_distiller/simple_article.html";
    // Suffix added to page titles, string is defined as IDS_DOM_DISTILLER_VIEWER_TITLE_SUFFIX in
    // dom_distiller_strings.grdp.
    private static final String TITLE_SUFFIX = " - Reading Mode";
    private static final String PAGE_TITLE = "Test Page Title" + TITLE_SUFFIX;
    private static final String CONTENT = "Lorem ipsum";

    private EmbeddedTestServer mTestServer;

    private String mURL;

    private CtaPageStation mPage;

    @Before
    public void setUp() {
        mTestServer =
                EmbeddedTestServer.createAndStartServer(
                        ApplicationProvider.getApplicationContext());
        mURL = mTestServer.getURL(TEST_PAGE);
    }

    @Test
    @MediumTest
    public void testReaderModeInRegularTab() throws TimeoutException {
        mPage = mActivityTestRule.startOnUrl(mURL).openRegularTabAppMenu().enterReaderMode();

        Tab originalTab = mPage.getTab();
        waitForDistillation(PAGE_TITLE, originalTab);
    }

    @Test
    @MediumTest
    public void testZoomLevelPrefsCallbackUpdatesFontScaling() throws TimeoutException {
        mPage = mActivityTestRule.startOnBlankPage();
        final DistilledPagePrefs distilledPagePrefs = getDistilledPagePrefs();

        // Check that the initial font scaling is tied to the default zoom level.
        final double initialZoomLevel =
                ThreadUtils.runOnUiThreadBlocking(
                        () ->
                                HostZoomMap.getDefaultZoomLevel(
                                        mActivityTestRule.getActivityTab().getProfile()));
        final float initialZoomFactor = (float) Math.pow(1.2, initialZoomLevel);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertEquals(
                            initialZoomFactor, distilledPagePrefs.getFontScaling(), 0.001f);
                });

        // Change the default zoom level and ensure the distilled page prefs are
        // updated to reflect the change.
        final double newZoomLevel = 2.0;
        final float newZoomFactor = (float) Math.pow(1.2, newZoomLevel);

        final CallbackHelper fontScalingChangedCallback = new CallbackHelper();
        DistilledPagePrefs.Observer observer =
                new DistilledPagePrefs.Observer() {
                    @Override
                    public void onChangeTheme(int theme) {}

                    @Override
                    public void onChangeFontFamily(int font) {}

                    @Override
                    public void onChangeFontScaling(float fontScaling) {
                        if (Math.abs(fontScaling - newZoomFactor) < 0.001f) {
                            fontScalingChangedCallback.notifyCalled();
                        }
                    }

                    @Override
                    public void onChangeLinksEnabled(boolean enabled) {}
                };
        ThreadUtils.runOnUiThreadBlocking(() -> distilledPagePrefs.addObserver(observer));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    HostZoomMap.setDefaultZoomLevel(
                            mActivityTestRule.getActivityTab().getProfile(), newZoomLevel);
                });

        fontScalingChangedCallback.waitForCallback(0);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertEquals(newZoomFactor, distilledPagePrefs.getFontScaling(), 0.001f);
                    distilledPagePrefs.removeObserver(observer);
                });
    }

    private DistilledPagePrefs getDistilledPagePrefs() {
        AtomicReference<DistilledPagePrefs> prefs = new AtomicReference<>();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    DomDistillerService domDistillerService =
                            DomDistillerServiceFactory.getForProfile(
                                    ProfileManager.getLastUsedRegularProfile());
                    prefs.set(domDistillerService.getDistilledPagePrefs());
                });
        return prefs.get();
    }

    /**
     * Run JavaScript on a certain {@link Tab}.
     *
     * @param tab The tab to be injected to.
     * @param javaScript The JavaScript code to be injected.
     * @return The result of the code.
     */
    private String runJavaScript(Tab tab, String javaScript) throws TimeoutException {
        TestCallbackHelperContainer.OnEvaluateJavaScriptResultHelper javascriptHelper =
                new TestCallbackHelperContainer.OnEvaluateJavaScriptResultHelper();
        javascriptHelper.evaluateJavaScriptForTests(tab.getWebContents(), javaScript);
        javascriptHelper.waitUntilHasValue();
        return javascriptHelper.getJsonResultAndClear();
    }

    /**
     * @param tab The tab to be inspected.
     * @return The inner HTML of a certain {@link Tab}.
     */
    private String getInnerHtml(Tab tab) throws TimeoutException {
        return runJavaScript(tab, "document.body.innerHTML");
    }

    /**
     * Wait until the distilled content is shown on the {@link Tab}.
     *
     * @param expectedTitle The expected title of the distilled content
     * @param tab the tab to wait
     */
    private void waitForDistillation(
            @SuppressWarnings("SameParameterValue") String expectedTitle, Tab tab)
            throws TimeoutException {
        CriteriaHelper.pollUiThread(
                () ->
                        Criteria.checkThat(
                                ChromeTabUtils.getUrlOnUiThread(tab).getScheme(),
                                is("chrome-distiller")));
        ChromeTabUtils.waitForTabPageLoaded(tab, null);
        // Distiller Viewer load the content dynamically, so waitForTabPageLoaded() is not enough.
        CriteriaHelper.pollUiThreadLongTimeout(
                null, () -> Criteria.checkThat(tab.getTitle(), is(expectedTitle)));

        String innerHtml = getInnerHtml(tab);
        assertThat(innerHtml).contains("article-header");
        assertThat(innerHtml).contains(CONTENT);
    }
}
