// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.action.ViewActions.longClick;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.allOf;
import static org.hamcrest.Matchers.is;
import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.ui.test.util.ViewUtils.waitForView;

import android.content.ComponentCallbacks2;
import android.content.res.Resources;
import android.graphics.Canvas;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.GradientDrawable;
import android.view.KeyEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.MarginLayoutParams;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.espresso.contrib.RecyclerViewActions;
import androidx.test.filters.LargeTest;
import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.Assert;
import org.junit.Assume;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;

import org.chromium.base.Callback;
import org.chromium.base.FeatureList;
import org.chromium.base.GarbageCollectionTestUtils;
import org.chromium.base.MemoryPressureListener;
import org.chromium.base.memory.MemoryPressureCallback;
import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterProvider;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.JniMocker;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.feed.FeedActionDelegate;
import org.chromium.chrome.browser.feed.FeedReliabilityLogger;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.logo.LogoBridge;
import org.chromium.chrome.browser.logo.LogoBridgeJni;
import org.chromium.chrome.browser.logo.LogoCoordinator;
import org.chromium.chrome.browser.native_page.ContextMenuManager;
import org.chromium.chrome.browser.omnibox.OmniboxStub;
import org.chromium.chrome.browser.omnibox.UrlBar;
import org.chromium.chrome.browser.omnibox.voice.VoiceRecognitionHandler;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.suggestions.SiteSuggestion;
import org.chromium.chrome.browser.suggestions.tile.Tile;
import org.chromium.chrome.browser.suggestions.tile.TileGroup;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabHidingType;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.util.BrowserUiUtils;
import org.chromium.chrome.browser.util.BrowserUiUtils.ModuleTypeOnStartAndNTP;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.NewTabPageTestUtils;
import org.chromium.chrome.test.util.OmniboxTestUtils;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.chrome.test.util.browser.signin.SigninTestRule;
import org.chromium.chrome.test.util.browser.signin.SigninTestUtil;
import org.chromium.chrome.test.util.browser.suggestions.SuggestionsDependenciesRule;
import org.chromium.chrome.test.util.browser.suggestions.mostvisited.FakeMostVisitedSites;
import org.chromium.components.browser_ui.widget.scrim.ScrimCoordinator;
import org.chromium.components.browser_ui.widget.tile.TileView;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.policy.test.annotations.Policies;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.browser.test.util.TestTouchUtils;
import org.chromium.content_public.browser.test.util.TouchCommon;
import org.chromium.content_public.common.ContentUrlConstants;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.net.test.util.TestWebServer;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.mojom.WindowOpenDisposition;
import org.chromium.url.GURL;

import java.io.IOException;
import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.concurrent.Callable;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.Semaphore;
import java.util.concurrent.TimeUnit;

/**
 * Tests for the native android New Tab Page.
 *
 * <p>TODO(https://crbug.com/906151): Add new goldens and enable ExploreSites.
 */
@RunWith(ParameterizedRunner.class)
@ParameterAnnotations.UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
    "disable-features=IPH_FeedHeaderMenu"
})
@DisableFeatures({ChromeFeatureList.QUERY_TILES})
public class NewTabPageTest {
    /** Parameter set controlling whether scrollable mvt is enabled. */
    public static class MVTParams implements ParameterProvider {
        @Override
        public Iterable<ParameterSet> getParameters() {
            return Arrays.asList(
                    new ParameterSet().value(true).name("EnableScrollableMVTOnNTP"),
                    new ParameterSet().value(false).name("DisableScrollableMVTOnNTP"));
        }
    }

    private static final int ARTICLE_SECTION_HEADER_POSITION = 1;

    private static final int RENDER_TEST_REVISION = 5;

    private static final String HISTOGRAM_NTP_MODULE_CLICK = "NewTabPage.Module.Click";
    private static final String HISTOGRAM_NTP_MODULE_LONGCLICK = "NewTabPage.Module.LongClick";

    private static final String SURFACE_POLISH_PARAMS = "force-fieldtrial-params=Study.Group:";

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule public SuggestionsDependenciesRule mSuggestionsDeps = new SuggestionsDependenciesRule();
    @Rule public SigninTestRule mSigninTestRule = new SigninTestRule();

    @Rule
    public ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setRevision(RENDER_TEST_REVISION)
                    .setBugComponent(ChromeRenderTestRule.Component.UI_BROWSER_NEW_TAB_PAGE)
                    .build();

    @Rule public JniMocker mJniMocker = new JniMocker();
    @Mock OmniboxStub mOmniboxStub;
    @Mock VoiceRecognitionHandler mVoiceRecognitionHandler;
    @Mock FeedReliabilityLogger mFeedReliabilityLogger;
    @Mock private Callback mOnVisitComplete;
    @Mock private Runnable mOnPageLoaded;
    @Mock LogoBridge.Natives mLogoBridgeJniMock;
    @Mock private LogoBridge mLogoBridge;

    private static final String TEST_PAGE = "/chrome/test/data/android/navigate/simple.html";
    private static final String TEST_FEED =
            UrlUtils.getIsolatedTestFilePath("/chrome/test/data/android/feed/hello_world.gcl.bin");
    private static final String TEST_URL = "https://www.example.com/";

    private static final String EMAIL = "email@gmail.com";
    private static final String NAME = "Email Emailson";

    private Tab mTab;
    private TemplateUrlService mTemplateUrlService;
    private NewTabPage mNtp;
    private View mFakebox;
    private ViewGroup mMvTilesLayout;
    private FakeMostVisitedSites mMostVisitedSites;
    private EmbeddedTestServer mTestServer;
    private List<SiteSuggestion> mSiteSuggestions;
    private OmniboxTestUtils mOmnibox;
    private boolean mEnableScrollableMVT;

    @ParameterAnnotations.UseMethodParameterBefore(MVTParams.class)
    public void setIsScrollableMVTEnabledForTest(boolean isScrollableMVTEnabled) {
        mEnableScrollableMVT = isScrollableMVTEnabled;
        FeatureList.TestValues testValuesOverride = new FeatureList.TestValues();
        testValuesOverride.addFeatureFlagOverride(
                ChromeFeatureList.SHOW_SCROLLABLE_MVT_ON_NTP_ANDROID, isScrollableMVTEnabled);
        testValuesOverride.addFeatureFlagOverride(
                ChromeFeatureList.SHOW_SCROLLABLE_MVT_ON_NTP_PHONE_ANDROID, isScrollableMVTEnabled);
        FeatureList.setTestValues(testValuesOverride);
    }

    @Before
    public void setUp() throws Exception {
        MockitoAnnotations.initMocks(this);
        mActivityTestRule.startMainActivityWithURL("about:blank");
        TemplateUrlService originalService =
                TestThreadUtils.runOnUiThreadBlockingNoException(
                        () ->
                                TemplateUrlServiceFactory.getForProfile(
                                        Profile.getLastUsedRegularProfile()));
        mTemplateUrlService = Mockito.spy(originalService);
        TemplateUrlServiceFactory.setInstanceForTesting(mTemplateUrlService);
        Assume.assumeFalse(mActivityTestRule.getActivity().isTablet() && mEnableScrollableMVT);

        mOmnibox = new OmniboxTestUtils(mActivityTestRule.getActivity());

        mTestServer =
                EmbeddedTestServer.createAndStartServer(
                        ApplicationProvider.getApplicationContext());

        mSiteSuggestions = NewTabPageTestUtils.createFakeSiteSuggestions(mTestServer);
        mMostVisitedSites = new FakeMostVisitedSites();
        mMostVisitedSites.setTileSuggestions(mSiteSuggestions);
        mSuggestionsDeps.getFactory().mostVisitedSites = mMostVisitedSites;

        mActivityTestRule.loadUrl(UrlConstants.NTP_URL);
        mTab = mActivityTestRule.getActivity().getActivityTab();
        NewTabPageTestUtils.waitForNtpLoaded(mTab);

        Assert.assertTrue(mTab.getNativePage() instanceof NewTabPage);
        mNtp = (NewTabPage) mTab.getNativePage();
        mFakebox = mNtp.getView().findViewById(R.id.search_box);
        mMvTilesLayout = mNtp.getView().findViewById(R.id.mv_tiles_layout);
        Assert.assertEquals(mSiteSuggestions.size(), mMvTilesLayout.getChildCount());
    }

    @Test
    @MediumTest
    @Feature({"NewTabPage", "FeedNewTabPage", "RenderTest"})
    @ParameterAnnotations.UseMethodParameter(MVTParams.class)
    // Disable sign-in to suppress sync promo, as it's unrelated to this render test.
    @Policies.Add(@Policies.Item(key = "BrowserSignin", string = "0"))
    public void testRender_FocusFakeBoxT(boolean isScrollableMVTEnabled) throws Exception {
        ScrimCoordinator scrimCoordinator =
                mActivityTestRule
                        .getActivity()
                        .getRootUiCoordinatorForTesting()
                        .getScrimCoordinatorForTesting();
        scrimCoordinator.disableAnimationForTesting(true);
        onView(withId(R.id.search_box)).perform(click());
        ChromeRenderTestRule.sanitize(mNtp.getView().getRootView());
        mRenderTestRule.render(
                mNtp.getView().getRootView(),
                "focus_fake_box"
                        + (mEnableScrollableMVT
                                ? "_with_scrollable_mvt"
                                : "_with_non_scrollable_mvt"));
        scrimCoordinator.disableAnimationForTesting(false);
    }

    @Test
    @MediumTest
    @Feature({"NewTabPage", "FeedNewTabPage", "RenderTest"})
    public void testRender_ArticleSectionHeader() throws Exception {
        // Scroll to the article section header in case it is not visible.
        onView(withId(R.id.feed_stream_recycler_view))
                .perform(RecyclerViewActions.scrollToPosition(ARTICLE_SECTION_HEADER_POSITION));
        waitForView((ViewGroup) mNtp.getView(), allOf(withId(R.id.header_title), isDisplayed()));
        View view = mNtp.getCoordinatorForTesting().getSectionHeaderViewForTesting();
        // Check header is expanded.
        mRenderTestRule.render(view, "expandable_header_expanded");

        // Toggle header on the current tab.
        onView(withId(R.id.feed_stream_recycler_view))
                .perform(RecyclerViewActions.scrollToPosition(ARTICLE_SECTION_HEADER_POSITION));
        waitForView((ViewGroup) mNtp.getView(), allOf(withId(R.id.header_title), isDisplayed()));
        onView(withId(R.id.header_title)).perform(click());
        // Check header is collapsed.
        mRenderTestRule.render(view, "expandable_header_collapsed");
    }

    /**
     * Tests that clicking on the fakebox causes it to animate upwards and focus the omnibox, and
     * defocusing the omnibox causes the fakebox to animate back down.
     */
    @Test
    @SmallTest
    @Feature({"NewTabPage", "FeedNewTabPage"})
    public void testFocusFakebox() {
        int initialFakeboxTop = getFakeboxTop(mNtp);

        TouchCommon.singleClickView(mFakebox);

        waitForFakeboxFocusAnimationComplete(mNtp);
        mOmnibox.checkFocus(true);
        if (!mActivityTestRule.getActivity().isTablet()) {
            int afterFocusFakeboxTop = getFakeboxTop(mNtp);
            Assert.assertTrue(afterFocusFakeboxTop < initialFakeboxTop);
        }

        mOmnibox.clearFocus();
        waitForFakeboxTopPosition(mNtp, initialFakeboxTop);
    }

    /**
     * Tests that clicking on the fakebox causes it to focus the omnibox and allows typing and
     * navigating to a URL.
     */
    @Test
    @SmallTest
    @Feature({"NewTabPage", "FeedNewTabPage"})
    @DisabledTest(message = "https://crbug.com/1033654")
    public void testSearchFromFakebox() {
        TouchCommon.singleClickView(mFakebox);
        waitForFakeboxFocusAnimationComplete(mNtp);
        final UrlBar urlBar = (UrlBar) mActivityTestRule.getActivity().findViewById(R.id.url_bar);
        mOmnibox.requestFocus();
        mOmnibox.typeText(UrlConstants.VERSION_URL, false);
        mOmnibox.checkSuggestionsShown();
        mOmnibox.sendKey(KeyEvent.KEYCODE_ENTER);
    }

    /** Tests clicking on a most visited item. */
    @Test
    @SmallTest
    @Feature({"NewTabPage", "FeedNewTabPage"})
    @ParameterAnnotations.UseMethodParameter(MVTParams.class)
    public void testClickMostVisitedItem(boolean isScrollableMVTEnabled) {
        Assert.assertNotNull(mMvTilesLayout);
        HistogramWatcher histogramWatcher = expectMostVisitedTilesRecordForNtpModuleClick();

        ChromeTabUtils.waitForTabPageLoaded(
                mTab,
                mSiteSuggestions.get(0).url.getSpec(),
                new Runnable() {
                    @Override
                    public void run() {
                        View mostVisitedItem = mMvTilesLayout.getChildAt(0);
                        TouchCommon.singleClickView(mostVisitedItem);
                    }
                });

        Assert.assertEquals(mSiteSuggestions.get(0).url, ChromeTabUtils.getUrlOnUiThread(mTab));
        histogramWatcher.assertExpected();
    }

    /** Tests opening a most visited item in a new tab. */
    @Test
    @SmallTest
    @Feature({"NewTabPage", "FeedNewTabPage"})
    @ParameterAnnotations.UseMethodParameter(MVTParams.class)
    @DisabledTest(message = "Flaky - crbug.com/543138")
    public void testOpenMostVisitedItemInNewTab(boolean isScrollableMVTEnabled)
            throws ExecutionException {
        Assert.assertNotNull(mMvTilesLayout);
        ChromeTabUtils.invokeContextMenuAndOpenInANewTab(
                mActivityTestRule,
                mMvTilesLayout.getChildAt(0),
                ContextMenuManager.ContextMenuItemId.OPEN_IN_NEW_TAB,
                false,
                mSiteSuggestions.get(0).url.getSpec());
    }

    /** Tests opening a most visited item in a new incognito tab. */
    @Test
    @SmallTest
    @Feature({"NewTabPage", "FeedNewTabPage"})
    @ParameterAnnotations.UseMethodParameter(MVTParams.class)
    public void testOpenMostVisitedItemInIncognitoTab(boolean isScrollableMVTEnabled)
            throws ExecutionException {
        Assert.assertNotNull(mMvTilesLayout);
        HistogramWatcher histogramWatcher = expectMostVisitedTilesRecordForNtpModuleClick();

        ChromeTabUtils.invokeContextMenuAndOpenInANewTab(
                mActivityTestRule,
                mMvTilesLayout.getChildAt(0),
                ContextMenuManager.ContextMenuItemId.OPEN_IN_INCOGNITO_TAB,
                true,
                mSiteSuggestions.get(0).url.getSpec());

        histogramWatcher.assertExpected();
    }

    /** Tests deleting a most visited item. */
    @Test
    @SmallTest
    @Feature({"NewTabPage", "FeedNewTabPage"})
    @ParameterAnnotations.UseMethodParameter(MVTParams.class)
    @DisabledTest(message = "crbug.com/1036500")
    public void testRemoveMostVisitedItem(boolean isScrollableMVTEnabled)
            throws ExecutionException {
        Assert.assertNotNull(mMvTilesLayout);
        SiteSuggestion testSite = mSiteSuggestions.get(0);
        View mostVisitedItem = mMvTilesLayout.getChildAt(0);
        ArrayList<View> views = new ArrayList<>();
        mMvTilesLayout.findViewsWithText(views, testSite.title, View.FIND_VIEWS_WITH_TEXT);
        Assert.assertEquals(1, views.size());

        TestTouchUtils.performLongClickOnMainSync(
                InstrumentationRegistry.getInstrumentation(), mostVisitedItem);
        Assert.assertTrue(
                InstrumentationRegistry.getInstrumentation()
                        .invokeContextMenuAction(
                                mActivityTestRule.getActivity(),
                                ContextMenuManager.ContextMenuItemId.REMOVE,
                                0));

        Assert.assertTrue(mMostVisitedSites.isUrlBlocklisted(testSite.url));
    }

    @Test
    @MediumTest
    @Feature({"NewTabPage", "FeedNewTabPage"})
    public void testUrlFocusAnimationsDisabledOnLoad() {
        Assert.assertFalse(getUrlFocusAnimationsDisabled());
        ChromeTabUtils.waitForTabPageLoaded(
                mTab,
                mTestServer.getURL(TEST_PAGE),
                new Runnable() {
                    @Override
                    public void run() {
                        TestThreadUtils.runOnUiThreadBlocking(
                                () -> {
                                    int pageTransition =
                                            PageTransition.TYPED | PageTransition.FROM_ADDRESS_BAR;
                                    mTab.loadUrl(
                                            new LoadUrlParams(
                                                    mTestServer.getURL(TEST_PAGE), pageTransition));
                                    // It should be disabled as soon as a load URL is triggered.
                                    Assert.assertTrue(getUrlFocusAnimationsDisabled());
                                });
                    }
                });
        // Ensure it is still marked as disabled once the new page is fully loaded.
        Assert.assertTrue(getUrlFocusAnimationsDisabled());
    }

    @Test
    @LargeTest
    @Feature({"NewTabPage", "FeedNewTabPage"})
    public void testUrlFocusAnimationsEnabledOnFailedLoad() throws Exception {
        // TODO(jbudorick): switch this to EmbeddedTestServer.
        TestWebServer webServer = TestWebServer.start();
        try {
            final Semaphore delaySemaphore = new Semaphore(0);
            Runnable delayAction =
                    new Runnable() {
                        @Override
                        public void run() {
                            try {
                                Assert.assertTrue(delaySemaphore.tryAcquire(10, TimeUnit.SECONDS));
                            } catch (InterruptedException e) {
                                e.printStackTrace();
                            }
                        }
                    };
            final String testPageUrl =
                    webServer.setResponseWithRunnableAction(
                            "/ntp_test.html", "<html><body></body></html>", null, delayAction);

            Assert.assertFalse(getUrlFocusAnimationsDisabled());

            clickFakebox();
            mOmnibox.checkFocus(true);
            mOmnibox.typeText(testPageUrl, false);
            mOmnibox.checkSuggestionsShown();

            final CallbackHelper loadedCallback = new CallbackHelper();
            TestThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        mTab.addObserver(
                                new EmptyTabObserver() {
                                    @Override
                                    public void onPageLoadFinished(Tab tab, GURL url) {
                                        loadedCallback.notifyCalled();
                                        tab.removeObserver(this);
                                    }
                                });
                    });

            mOmnibox.sendKey(KeyEvent.KEYCODE_ENTER);
            waitForUrlFocusAnimationsDisabledState(true);
            waitForTabLoading();

            TestThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        mTab.stopLoading();
                    });
            waitForUrlFocusAnimationsDisabledState(false);
            delaySemaphore.release();
            loadedCallback.waitForCallback(0);
            Assert.assertFalse(getUrlFocusAnimationsDisabled());
        } finally {
            webServer.shutdown();
        }
    }

    /** Tests setting whether the search provider has a logo. */
    @Test
    @SmallTest
    @Feature({"NewTabPage", "FeedNewTabPage"})
    public void testSetSearchProviderInfo() throws Throwable {
        mActivityTestRule.runOnUiThread(
                new Runnable() {
                    @Override
                    public void run() {
                        NewTabPageLayout ntpLayout = mNtp.getNewTabPageLayout();
                        View logoView = ntpLayout.findViewById(R.id.search_provider_logo);
                        Assert.assertEquals(View.VISIBLE, logoView.getVisibility());

                        ntpLayout.setSearchProviderInfo(/* hasLogo= */ false, /* isGoogle= */ true);
                        // Mock to notify the template URL service observer.
                        when(mTemplateUrlService.doesDefaultSearchEngineHaveLogo())
                                .thenReturn(false);
                        when(mTemplateUrlService.isDefaultSearchEngineGoogle()).thenReturn(true);
                        ntpLayout
                                .getLogoCoordinatorForTesting()
                                .onTemplateURLServiceChangedForTesting();
                        Assert.assertEquals(View.GONE, logoView.getVisibility());

                        ntpLayout.setSearchProviderInfo(/* hasLogo= */ true, /* isGoogle= */ true);
                        // Mock to notify the template URL service observer.
                        when(mTemplateUrlService.doesDefaultSearchEngineHaveLogo())
                                .thenReturn(true);
                        ntpLayout
                                .getLogoCoordinatorForTesting()
                                .onTemplateURLServiceChangedForTesting();
                        Assert.assertEquals(View.VISIBLE, logoView.getVisibility());
                    }
                });
    }

    /**
     * Verifies that the placeholder is only shown when there are no tile suggestions and the search
     * provider has no logo.
     */
    @Test
    @SmallTest
    @Feature({"NewTabPage", "FeedNewTabPage"})
    @ParameterAnnotations.UseMethodParameter(MVTParams.class)
    public void testPlaceholder(boolean isScrollableMVTEnabled) {
        when(mTemplateUrlService.doesDefaultSearchEngineHaveLogo()).thenReturn(true);

        final NewTabPageLayout ntpLayout = mNtp.getNewTabPageLayout();
        final View logoView = ntpLayout.findViewById(R.id.search_provider_logo);
        final View searchBoxView = ntpLayout.findViewById(R.id.search_box);

        // Initially, the logo is visible, the search box is visible, there is one tile suggestion,
        // and the placeholder has not been inflated yet.
        Assert.assertEquals(View.VISIBLE, logoView.getVisibility());
        Assert.assertEquals(View.VISIBLE, searchBoxView.getVisibility());
        Assert.assertEquals(8, mMvTilesLayout.getChildCount());
        Assert.assertNull(mNtp.getView().findViewById(R.id.tile_grid_placeholder));

        // When the search provider has no logo and there are no tile suggestions, the placeholder
        // is shown.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    when(mTemplateUrlService.doesDefaultSearchEngineHaveLogo()).thenReturn(false);
                    when(mTemplateUrlService.isDefaultSearchEngineGoogle()).thenReturn(true);
                    ntpLayout.setSearchProviderInfo(/* hasLogo= */ false, /* isGoogle= */ true);
                    // Mock to notify the template URL service observer.
                    ntpLayout
                            .getLogoCoordinatorForTesting()
                            .onTemplateURLServiceChangedForTesting();

                    Assert.assertEquals(View.GONE, logoView.getVisibility());
                    Assert.assertEquals(View.GONE, searchBoxView.getVisibility());

                    mMostVisitedSites.setTileSuggestions(new String[] {});
                    ntpLayout.onSwitchToForeground(); // Force tile refresh.
                    // Mock to notify the template URL service observer.
                    ntpLayout
                            .getMostVisitedTilesCoordinatorForTesting()
                            .onTemplateURLServiceChangedForTesting();
                });
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            "The tile grid was not updated.",
                            mMvTilesLayout.getChildCount(),
                            is(0));
                });
        Assert.assertNotNull(mNtp.getView().findViewById(R.id.tile_grid_placeholder));
        Assert.assertEquals(
                View.VISIBLE,
                mNtp.getView().findViewById(R.id.tile_grid_placeholder).getVisibility());

        // Once the search provider has a logo again, the logo and search box are shown again and
        // the placeholder is hidden.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    when(mTemplateUrlService.doesDefaultSearchEngineHaveLogo()).thenReturn(true);
                    when(mTemplateUrlService.isDefaultSearchEngineGoogle()).thenReturn(true);
                    ntpLayout.setSearchProviderInfo(/* hasLogo= */ true, /* isGoogle= */ true);
                    // Mock to notify the template URL service observer.
                    ntpLayout
                            .getLogoCoordinatorForTesting()
                            .onTemplateURLServiceChangedForTesting();

                    Assert.assertEquals(View.VISIBLE, logoView.getVisibility());
                    Assert.assertEquals(View.VISIBLE, searchBoxView.getVisibility());

                    // Mock to notify the template URL service observer.
                    ntpLayout
                            .getMostVisitedTilesCoordinatorForTesting()
                            .onTemplateURLServiceChangedForTesting();
                    Assert.assertEquals(
                            View.GONE,
                            mNtp.getView()
                                    .findViewById(R.id.tile_grid_placeholder)
                                    .getVisibility());
                });
    }

    @Test
    @SmallTest
    @Feature({"NewTabPage", "FeedNewTabPage"})
    public void testMemoryPressure() throws Exception {
        // TODO(twellington): This test currently just checks that sending a memory pressure
        // signal doesn't crash. Enhance the test to also check whether certain behaviors are
        // performed.
        CallbackHelper callback = new CallbackHelper();
        MemoryPressureCallback pressureCallback = pressure -> callback.notifyCalled();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    MemoryPressureListener.addCallback(pressureCallback);
                    mActivityTestRule
                            .getActivity()
                            .getApplication()
                            .onTrimMemory(ComponentCallbacks2.TRIM_MEMORY_MODERATE);
                });

        callback.waitForCallback(0);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> MemoryPressureListener.removeCallback(pressureCallback));
    }

    @Test
    @DisabledTest(message = "Test is flaky. crbug.com/1077724")
    @SmallTest
    @Feature("NewTabPage")
    public void testNewTabPageCanBeGarbageCollected() throws IOException {
        WeakReference<NewTabPage> ntpRef = new WeakReference<>(mNtp);

        mActivityTestRule.loadUrl("about:blank");

        mNtp = null;
        mMostVisitedSites = null;
        mSuggestionsDeps.getFactory().mostVisitedSites = null;
        mFakebox = null;
        mMvTilesLayout = null;
        mTab = null;

        Assert.assertTrue(GarbageCollectionTestUtils.canBeGarbageCollected(ntpRef));
    }

    @Test
    @SmallTest
    @Feature({"NewTabPage", "FeedNewTabPage"})
    public void testSettingOmniboxStubAddsVoiceObserver() throws IOException {
        when(mOmniboxStub.getVoiceRecognitionHandler()).thenReturn(mVoiceRecognitionHandler);
        when(mVoiceRecognitionHandler.isVoiceSearchEnabled()).thenReturn(true);

        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mNtp.setOmniboxStub(mOmniboxStub);
                    verify(mVoiceRecognitionHandler).addObserver(eq(mNtp));
                    View micButton = mNtp.getView().findViewById(R.id.voice_search_button);
                    assertEquals(View.VISIBLE, micButton.getVisibility());

                    when(mVoiceRecognitionHandler.isVoiceSearchEnabled()).thenReturn(false);
                    mNtp.onVoiceAvailabilityImpacted();
                    assertEquals(View.GONE, micButton.getVisibility());
                });
    }

    @Test
    @SmallTest
    @Feature({"NewTabPage", "FeedNewTabPage"})
    public void testSettingOmniboxStubAddsUrlFocusChangeListener() throws IOException {
        mNtp.getCoordinatorForTesting().setReliabilityLoggerForTesting(mFeedReliabilityLogger);

        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mNtp.setOmniboxStub(mOmniboxStub);
                    verify(mOmniboxStub).addUrlFocusChangeListener(eq(mFeedReliabilityLogger));
                });
    }

    @Test
    @SmallTest
    @Feature({"NewTabPage", "FeedNewTabPage"})
    public void testFeedReliabilityLoggingFocusOmnibox() throws IOException {
        mNtp.getCoordinatorForTesting().setReliabilityLoggerForTesting(mFeedReliabilityLogger);

        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mNtp.getNewTabPageManagerForTesting()
                            .focusSearchBox(/* beginVoiceSearch= */ false, /* pastedText= */ "");
                    verify(mFeedReliabilityLogger).onOmniboxFocused();
                });
    }

    @Test
    @SmallTest
    @Feature({"NewTabPage", "FeedNewTabPage"})
    public void testFeedReliabilityLoggingVoiceSearch() throws IOException {
        mNtp.getCoordinatorForTesting().setReliabilityLoggerForTesting(mFeedReliabilityLogger);

        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mNtp.getNewTabPageManagerForTesting()
                            .focusSearchBox(/* beginVoiceSearch= */ true, /* pastedText= */ "");
                    verify(mFeedReliabilityLogger).onVoiceSearch();
                });
    }

    @Test
    @SmallTest
    @Feature({"NewTabPage", "FeedNewTabPage"})
    public void testFeedReliabilityLoggingHideWithBack() throws IOException {
        mNtp.getCoordinatorForTesting().setReliabilityLoggerForTesting(mFeedReliabilityLogger);

        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ChromeTabbedActivity activity =
                            (ChromeTabbedActivity) mActivityTestRule.getActivity();
                    activity.handleBackPressed();
                    verify(mFeedReliabilityLogger).onNavigateBack();
                });
    }

    /**
     * Test whether the clicking action on MV tiles in {@link NewTabPage} is been recorded in
     * histogram correctly.
     */
    @Test
    @SmallTest
    @DisabledTest(message = "https://crbug.com/1434807")
    public void testRecordHistogramMostVisitedItemClick_Ntp() {
        Tile tileForTest = new Tile(mSiteSuggestions.get(0), 0);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TileGroup.Delegate tileGroupDelegate = mNtp.getTileGroupDelegateForTesting();

                    // Test clicking on MV tiles.
                    HistogramWatcher histogramWatcher =
                            expectMostVisitedTilesRecordForNtpModuleClick();
                    tileGroupDelegate.openMostVisitedItem(
                            WindowOpenDisposition.CURRENT_TAB, tileForTest);
                    histogramWatcher.assertExpected(
                            HISTOGRAM_NTP_MODULE_CLICK
                                    + " is not recorded correctly when click on MV tiles.");

                    // Test long press then open in new tab in group on MV tiles.
                    histogramWatcher = expectMostVisitedTilesRecordForNtpModuleClick();
                    tileGroupDelegate.openMostVisitedItemInGroup(
                            WindowOpenDisposition.NEW_BACKGROUND_TAB, tileForTest);
                    histogramWatcher.assertExpected(
                            HISTOGRAM_NTP_MODULE_CLICK
                                    + " is not recorded correctly when long press then open in new"
                                    + " tab in group on MV tiles.");

                    // Test long press then open in new tab on MV tiles.
                    histogramWatcher = expectMostVisitedTilesRecordForNtpModuleClick();
                    tileGroupDelegate.openMostVisitedItem(
                            WindowOpenDisposition.NEW_BACKGROUND_TAB, tileForTest);
                    histogramWatcher.assertExpected(
                            HISTOGRAM_NTP_MODULE_CLICK
                                    + " is not recorded correctly when long press then open in new"
                                    + " tab on MV tiles.");

                    // Test long press then open in other window on MV tiles.
                    histogramWatcher = expectNoRecordsForNtpModuleClick();
                    tileGroupDelegate.openMostVisitedItem(
                            WindowOpenDisposition.NEW_WINDOW, tileForTest);
                    histogramWatcher.assertExpected(
                            HISTOGRAM_NTP_MODULE_CLICK
                                    + " shouldn't be recorded when long press then open in other"
                                    + " window on MV tiles.");

                    // Test long press then download link on MV tiles.
                    histogramWatcher = expectMostVisitedTilesRecordForNtpModuleClick();
                    tileGroupDelegate.openMostVisitedItem(
                            WindowOpenDisposition.SAVE_TO_DISK, tileForTest);
                    histogramWatcher.assertExpected(
                            HISTOGRAM_NTP_MODULE_CLICK
                                    + " is not recorded correctly when long press then download"
                                    + " link on MV tiles.");

                    // Test long press then open in Incognito tab on MV tiles.
                    histogramWatcher = expectMostVisitedTilesRecordForNtpModuleClick();
                    tileGroupDelegate.openMostVisitedItem(
                            WindowOpenDisposition.OFF_THE_RECORD, tileForTest);
                    histogramWatcher.assertExpected(
                            HISTOGRAM_NTP_MODULE_CLICK
                                    + " is not recorded correctly when long press then open in"
                                    + " Incognito tab on MV tiles.");
                });
    }

    /**
     * Test whether the clicking action on Feeds in {@link NewTabPage} is been recorded in histogram
     * correctly.
     */
    @Test
    @SmallTest
    public void testRecordHistogramFeedClick_Ntp() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    FeedActionDelegate feedActionDelegate = mNtp.getFeedActionDelegateForTesting();

                    // Test click on Feeds or long press then check about this source & topic on
                    // Feeds.
                    HistogramWatcher histogramWatcher = expectFeedRecordForNtpModuleClick();
                    feedActionDelegate.openSuggestionUrl(
                            WindowOpenDisposition.CURRENT_TAB,
                            new LoadUrlParams(TEST_URL, PageTransition.AUTO_BOOKMARK),
                            false,
                            mOnPageLoaded,
                            mOnVisitComplete);
                    histogramWatcher.assertExpected(
                            HISTOGRAM_NTP_MODULE_CLICK
                                    + " is not recorded correctly when click on Feeds or long press"
                                    + " then check about this source & topic on Feeds.");

                    // Test long press then open in new tab on Feeds.
                    histogramWatcher = expectFeedRecordForNtpModuleClick();
                    feedActionDelegate.openSuggestionUrl(
                            WindowOpenDisposition.NEW_BACKGROUND_TAB,
                            new LoadUrlParams(TEST_URL, PageTransition.AUTO_BOOKMARK),
                            false,
                            mOnPageLoaded,
                            mOnVisitComplete);
                    histogramWatcher.assertExpected(
                            HISTOGRAM_NTP_MODULE_CLICK
                                    + " is not recorded correctly when long press then open in new"
                                    + " tab on Feeds.");

                    // Test long press then open in incognito tab on Feeds.
                    histogramWatcher = expectFeedRecordForNtpModuleClick();
                    feedActionDelegate.openSuggestionUrl(
                            WindowOpenDisposition.OFF_THE_RECORD,
                            new LoadUrlParams(TEST_URL, PageTransition.AUTO_BOOKMARK),
                            false,
                            mOnPageLoaded,
                            mOnVisitComplete);
                    histogramWatcher.assertExpected(
                            HISTOGRAM_NTP_MODULE_CLICK
                                    + " is not recorded correctly when long press then open in"
                                    + " incognito tab on  Feeds.");

                    // Test manage activity or manage interests on Feeds.
                    histogramWatcher = expectNoRecordsForNtpModuleClick();
                    feedActionDelegate.openUrl(
                            WindowOpenDisposition.CURRENT_TAB,
                            new LoadUrlParams(TEST_URL, PageTransition.LINK));
                    histogramWatcher.assertExpected(
                            HISTOGRAM_NTP_MODULE_CLICK
                                    + " shouldn't be recorded when manage activity or manage"
                                    + " interests on Feeds.");

                    // Test click Learn More button on Feeds.
                    histogramWatcher = expectFeedRecordForNtpModuleClick();
                    feedActionDelegate.openHelpPage();
                    histogramWatcher.assertExpected(
                            HISTOGRAM_NTP_MODULE_CLICK
                                    + " is not recorded correctly when click Learn More button on"
                                    + " Feeds.");
                });
    }

    /**
     * Test whether the clicking action on the home button in {@link NewTabPage} is been recorded in
     * histogram correctly.
     */
    @Test
    @SmallTest
    public void testRecordHistogramHomeButtonClick_Ntp() {
        HistogramWatcher histogramWatcher = expectHomeButtonRecordForNtpModuleClick();
        onView(withId(R.id.home_button)).perform(click());
        histogramWatcher.assertExpected(
                HISTOGRAM_NTP_MODULE_CLICK
                        + " is not recorded correctly when click on the home button.");

        histogramWatcher = expectHomeButtonRecordForNtpModuleLongClick();
        onView(withId(R.id.home_button)).perform(longClick());
        onView(withText(R.string.options_homepage_edit_title)).perform(click());
        histogramWatcher.assertExpected(
                HISTOGRAM_NTP_MODULE_LONGCLICK
                        + " is not recorded correctly when we perform long click on the home button"
                        + " and navigate to home page setting.");
    }

    /**
     * Test whether the clicking action on the profile button in {@link NewTabPage} is been recorded
     * in histogram correctly.
     */
    @Test
    @SmallTest
    public void testRecordHistogramProfileButtonClick_Ntp() {
        // Identity Disc should be shown on sign-in state.
        waitForSignIn();
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        HISTOGRAM_NTP_MODULE_CLICK,
                        BrowserUiUtils.ModuleTypeOnStartAndNTP.PROFILE_BUTTON);
        onView(withId(R.id.optional_toolbar_button)).perform(click());
        histogramWatcher.assertExpected(
                HISTOGRAM_NTP_MODULE_CLICK
                        + " is not recorded correctly when click on the profile button.");
        mSigninTestRule.signOut();
    }

    /**
     * Test whether the clicking action on Logo in {@link NewTabPage} is been recorded in histogram
     * correctly.
     */
    @Test
    @SmallTest
    @Feature({"NewTabPage"})
    public void testRecordHistogramLogoClick_Ntp() {
        mJniMocker.mock(LogoBridgeJni.TEST_HOOKS, mLogoBridgeJniMock);
        NewTabPageLayout ntpLayout = mNtp.getNewTabPageLayout();
        LogoCoordinator logoCoordinator = ntpLayout.getLogoCoordinatorForTesting();
        logoCoordinator.setLogoBridgeForTesting(mLogoBridge);
        logoCoordinator.setOnLogoClickUrlForTesting(TEST_URL);
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        HISTOGRAM_NTP_MODULE_CLICK, ModuleTypeOnStartAndNTP.DOODLE);
        TestThreadUtils.runOnUiThreadBlocking(() -> logoCoordinator.onLogoClickedForTesting(true));
        histogramWatcher.assertExpected(
                HISTOGRAM_NTP_MODULE_CLICK
                        + " is not recorded correctly when click on Logo with doodle enabled.");
    }

    /**
     * Test whether the clicking action on the menu button in {@link NewTabPage} is been recorded in
     * histogram correctly.
     */
    @Test
    @SmallTest
    public void testRecordHistogramMenuButtonClick_Ntp() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        HISTOGRAM_NTP_MODULE_CLICK,
                        BrowserUiUtils.ModuleTypeOnStartAndNTP.MENU_BUTTON);
        onView(withId(R.id.menu_button_wrapper)).perform(click());
        histogramWatcher.assertExpected(
                HISTOGRAM_NTP_MODULE_CLICK
                        + " is not recorded correctly when click on the menu button.");
    }

    /** Test show and click on the single tab card on the {@link NewTabPage} in the tablet. */
    @Test
    @MediumTest
    @Feature({"NewTabPage"})
    @EnableFeatures({ChromeFeatureList.SHOW_SCROLLABLE_MVT_ON_NTP_PHONE_ANDROID})
    public void testSingleTabCardShowAndClick() {
        ChromeTabbedActivity activity = mActivityTestRule.getActivity();
        mActivityTestRule.loadUrl(TEST_URL);
        Tab mostRecentTab = activity.getActivityTab();
        Tab newTab = mActivityTestRule.loadUrlInNewTab(UrlConstants.NTP_URL);
        NewTabPageTestUtils.waitForNtpLoaded(newTab);
        NewTabPage ntp = (NewTabPage) newTab.getNativePage();
        NewTabPageLayout ntpLayout = ntp.getNewTabPageLayout();

        TestThreadUtils.runOnUiThreadBlocking(() -> ntp.showHomeSurfaceUi(mostRecentTab));

        View singleTabCardView = ntpLayout.findViewById(R.id.single_tab_view);
        assertEquals(
                "The single tab card is still invisible after initialization.",
                View.VISIBLE,
                singleTabCardView.getVisibility());
        TextView title = singleTabCardView.findViewById(R.id.tab_title_view);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    String mostRecentTabTitle = mostRecentTab.getTitle();
                    assertEquals(
                            "The title of the single tab card is wrong.",
                            mostRecentTabTitle,
                            title.getText());
                });

        onView(withId(R.id.single_tab_view)).perform(click());
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    int mostRecentTabId = mostRecentTab.getId();
                    assertEquals(
                            "After clicking on the single tab card, it leads us to the wrong tab.",
                            mostRecentTabId,
                            mActivityTestRule.getActivity().getActivityTab().getId());
                });
    }

    /** Test destroy the already existing single tab card on the {@link NewTabPage} on tablet. */
    @Test
    @MediumTest
    @Feature({"NewTabPage"})
    public void testSingleTabCardDestroy() {
        ChromeTabbedActivity activity = mActivityTestRule.getActivity();
        mActivityTestRule.loadUrl(TEST_URL);
        Tab mostRecentTab = activity.getActivityTab();
        Tab newTab = mActivityTestRule.loadUrlInNewTab(UrlConstants.NTP_URL);
        NewTabPageTestUtils.waitForNtpLoaded(newTab);
        NewTabPage ntp = (NewTabPage) newTab.getNativePage();
        NewTabPageLayout ntpLayout = ntp.getNewTabPageLayout();

        TestThreadUtils.runOnUiThreadBlocking(() -> ntp.showHomeSurfaceUi(mostRecentTab));

        ViewGroup singleTabCardViewContainer =
                ntpLayout.findViewById(R.id.tab_switcher_module_container);
        assertEquals(
                "The single tab card container is still invisible after initialization.",
                View.VISIBLE,
                singleTabCardViewContainer.getVisibility());
        View singleTabCardView = ntpLayout.findViewById(R.id.single_tab_view);
        assertEquals(
                "The single tab card is still invisible after initialization.",
                View.VISIBLE,
                singleTabCardView.getVisibility());

        TestThreadUtils.runOnUiThreadBlocking(ntp::destroySingleTabCard);
        assertEquals(
                "The single tab card container is still visible after destroying it.",
                View.GONE,
                singleTabCardViewContainer.getVisibility());
        assertEquals(
                "The single tab card is still visible after destroying it.",
                View.GONE,
                singleTabCardView.getVisibility());
    }

    /**
     * Test hide the already existing single tab card on the {@link NewTabPage} in the tablet by the
     * tab observer.
     */
    @Test
    @MediumTest
    @Feature({"NewTabPage"})
    public void testSingleTabCardHide() {
        ChromeTabbedActivity activity = mActivityTestRule.getActivity();
        mActivityTestRule.loadUrl(TEST_URL);
        Tab mostRecentTab = activity.getActivityTab();
        Tab newTab = mActivityTestRule.loadUrlInNewTab(UrlConstants.NTP_URL);
        NewTabPageTestUtils.waitForNtpLoaded(newTab);
        NewTabPage ntp = (NewTabPage) newTab.getNativePage();
        NewTabPageLayout ntpLayout = ntp.getNewTabPageLayout();

        TestThreadUtils.runOnUiThreadBlocking(() -> ntp.showHomeSurfaceUi(mostRecentTab));

        ViewGroup singleTabCardViewContainer =
                ntpLayout.findViewById(R.id.tab_switcher_module_container);
        assertEquals(
                "The single tab card container is still invisible after initialization.",
                View.VISIBLE,
                singleTabCardViewContainer.getVisibility());
        View singleTabCardView = ntpLayout.findViewById(R.id.single_tab_view);
        assertEquals(
                "The single tab card is still invisible after initialization.",
                View.VISIBLE,
                singleTabCardView.getVisibility());

        TabObserver tabObserver = ntp.getTabObserverForTesting();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    tabObserver.onHidden(newTab, TabHidingType.ACTIVITY_HIDDEN);
                });
        assertEquals(
                "The single tab card container is still visible after hiding it.",
                View.GONE,
                singleTabCardViewContainer.getVisibility());
        assertEquals(
                "The single tab card is still visible after hiding it.",
                View.GONE,
                singleTabCardView.getVisibility());
    }

    /** Test update the most recent tab of the single tab card with the regular tab information. */
    @Test
    @MediumTest
    @Feature({"NewTabPage"})
    public void testSingleTabCardUpdateMostRecentTab_NotNtp() {
        ChromeTabbedActivity activity = mActivityTestRule.getActivity();
        mActivityTestRule.loadUrl(ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);
        Tab originalMostRecentTab = activity.getActivityTab();
        Tab tab = mActivityTestRule.loadUrlInNewTab(UrlConstants.NTP_URL);
        NewTabPageTestUtils.waitForNtpLoaded(tab);
        NewTabPage ntp = (NewTabPage) tab.getNativePage();
        NewTabPageLayout ntpLayout = ntp.getNewTabPageLayout();

        TestThreadUtils.runOnUiThreadBlocking(() -> ntp.showHomeSurfaceUi(originalMostRecentTab));

        View singleTabCardView = ntpLayout.findViewById(R.id.single_tab_view);
        assertEquals(
                "The single tab card is still invisible after initialization.",
                View.VISIBLE,
                singleTabCardView.getVisibility());
        TextView title = singleTabCardView.findViewById(R.id.tab_title_view);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    String originalMostRecentTabTitleForCheck = originalMostRecentTab.getTitle();
                    assertEquals(
                            "The title of the single tab card is wrong after initialization.",
                            originalMostRecentTabTitleForCheck,
                            title.getText());
                });

        Tab newMostRecentTab = mActivityTestRule.loadUrlInNewTab(TEST_URL);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TabModelUtils.setIndex(
                            activity.getCurrentTabModel(),
                            TabModelUtils.getTabIndexById(
                                    activity.getCurrentTabModel(), tab.getId()),
                            false);
                    ntp.showHomeSurfaceUi(newMostRecentTab);
                });

        assertEquals(
                "The single tab card is invisible after updating the single tab card.",
                View.VISIBLE,
                singleTabCardView.getVisibility());
        TextView newTitle = singleTabCardView.findViewById(R.id.tab_title_view);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    String newMostRecentTabTitleForCheck = newMostRecentTab.getTitle();
                    assertEquals(
                            "The title of the single tab card is wrong after updating.",
                            newMostRecentTabTitleForCheck,
                            newTitle.getText());
                });
    }

    /** Test update the most recent tab of the single tab card with the new tab page information. */
    @Test
    @MediumTest
    @Feature({"NewTabPage"})
    public void testSingleTabCardUpdateMostRecentTab_Ntp() {
        ChromeTabbedActivity activity = mActivityTestRule.getActivity();
        mActivityTestRule.loadUrl(TEST_URL);
        Tab originalMostRecentTab = activity.getActivityTab();
        Tab tab = mActivityTestRule.loadUrlInNewTab(UrlConstants.NTP_URL);
        NewTabPageTestUtils.waitForNtpLoaded(tab);
        NewTabPage ntp = (NewTabPage) tab.getNativePage();
        NewTabPageLayout ntpLayout = ntp.getNewTabPageLayout();

        TestThreadUtils.runOnUiThreadBlocking(() -> ntp.showHomeSurfaceUi(originalMostRecentTab));

        ViewGroup singleTabCardViewContainer =
                ntpLayout.findViewById(R.id.tab_switcher_module_container);
        assertEquals(
                "The single tab card container is still invisible after initialization.",
                View.VISIBLE,
                singleTabCardViewContainer.getVisibility());
        View singleTabCardView = ntpLayout.findViewById(R.id.single_tab_view);
        assertEquals(
                "The single tab card is still invisible after initialization.",
                View.VISIBLE,
                singleTabCardView.getVisibility());
        TextView title = singleTabCardView.findViewById(R.id.tab_title_view);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    String originalMostRecentTabTitleForCheck = originalMostRecentTab.getTitle();
                    assertEquals(
                            "The title of the single tab card is wrong after initialization.",
                            originalMostRecentTabTitleForCheck,
                            title.getText());
                });

        Tab newMostRecentTab = mActivityTestRule.loadUrlInNewTab(UrlConstants.NTP_URL);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TabModelUtils.setIndex(
                            activity.getCurrentTabModel(),
                            TabModelUtils.getTabIndexById(
                                    activity.getCurrentTabModel(), tab.getId()),
                            false);
                    ntp.showHomeSurfaceUi(newMostRecentTab);
                });

        assertEquals(
                "The single tab card container is still visible after updating with "
                        + "the new tab page information.",
                View.GONE,
                singleTabCardViewContainer.getVisibility());
        assertEquals(
                "The single tab card is still visible after updating with the new tab "
                        + "page information.",
                View.GONE,
                singleTabCardView.getVisibility());
    }

    @Test
    @MediumTest
    @Feature({"NewTabPage"})
    @EnableFeatures({ChromeFeatureList.SURFACE_POLISH + "<Study,"})
    @CommandLineFlags.Add({
        "force-fieldtrials=Study/Group",
        SURFACE_POLISH_PARAMS + "scrollable_mvt/true"
    })
    public void test1RowMvtOnNtpAfterPolish() {
        verifyMostVisitedTileMarginPolish();

        Resources res = mActivityTestRule.getActivity().getResources();
        NewTabPageLayout ntpLayout = mNtp.getNewTabPageLayout();
        View mvTilesLayout = ntpLayout.findViewById(org.chromium.chrome.test.R.id.mv_tiles_layout);

        int expectedTitleTopMargin =
                res.getDimensionPixelSize(R.dimen.tile_view_title_margin_top_modern_polish);
        TileView suggestionsTileElement = (TileView) ((LinearLayout) mvTilesLayout).getChildAt(0);
        Assert.assertEquals(
                "The top margin of the tile element's title is wrong.",
                expectedTitleTopMargin,
                ((MarginLayoutParams) suggestionsTileElement.getTitleView().getLayoutParams())
                        .topMargin);
    }

    @Test
    @MediumTest
    @Feature({"NewTabPage"})
    @EnableFeatures(ChromeFeatureList.SURFACE_POLISH)
    @DisableFeatures({ChromeFeatureList.FEED_POSITION_ANDROID})
    public void test2RowMvtOnNtpAfterPolish() {
        verifyMostVisitedTileMarginPolish();

        Resources res = mActivityTestRule.getActivity().getResources();
        NewTabPageLayout ntpLayout = mNtp.getNewTabPageLayout();
        View mvTilesLayout = ntpLayout.findViewById(org.chromium.chrome.test.R.id.mv_tiles_layout);

        int expectedTitleTopMargin =
                res.getDimensionPixelSize(R.dimen.tile_view_title_margin_top_modern_polish);
        TileView suggestionsTileElement = (TileView) ((FrameLayout) mvTilesLayout).getChildAt(0);
        Assert.assertEquals(
                "The top margin of the tile element's title is wrong.",
                expectedTitleTopMargin,
                ((MarginLayoutParams) suggestionsTileElement.getTitleView().getLayoutParams())
                        .topMargin);

        int expectedMvtVerticalSpacing =
                res.getDimensionPixelSize(R.dimen.tile_grid_layout_vertical_spacing_polish);
        Assert.assertEquals(
                "The vertical spacing between the 2 rows of the most visited tiles is wrong.",
                expectedMvtVerticalSpacing,
                mvTilesLayout.getHeight() - suggestionsTileElement.getHeight() * 2,
                1);
    }

    private void verifyMostVisitedTileMarginPolish() {
        Resources res = mActivityTestRule.getActivity().getResources();
        NewTabPageLayout ntpLayout = mNtp.getNewTabPageLayout();
        View mvTilesContainer =
                ntpLayout.findViewById(org.chromium.chrome.test.R.id.mv_tiles_container);

        int expectedMvtLateralMargin =
                res.getDimensionPixelSize(R.dimen.mvt_container_lateral_margin_polish);
        Assert.assertEquals(
                "The left margin of the most visited tiles container is wrong.",
                expectedMvtLateralMargin,
                ((MarginLayoutParams) mvTilesContainer.getLayoutParams()).leftMargin);
        Assert.assertEquals(
                "The right margin of the most visited tiles container is wrong.",
                expectedMvtLateralMargin,
                ((MarginLayoutParams) mvTilesContainer.getLayoutParams()).rightMargin);
        Assert.assertEquals(
                "The width of the most visited tiles container is wrong.",
                expectedMvtLateralMargin * 2,
                ntpLayout.getWidth() - mvTilesContainer.getWidth());

        int expectedMvtTopMargin =
                res.getDimensionPixelSize(R.dimen.mvt_container_top_margin_polish);
        int expectedMvtBottomMargin = 0;
        Assert.assertEquals(
                "The top margin of the most visited tiles container is wrong.",
                expectedMvtTopMargin,
                ((MarginLayoutParams) mvTilesContainer.getLayoutParams()).topMargin,
                1);
        Assert.assertEquals(
                "The bottom margin of the most visited tiles container is wrong.",
                expectedMvtBottomMargin,
                ((MarginLayoutParams) mvTilesContainer.getLayoutParams()).bottomMargin);

        int expectedMvtTopPadding =
                res.getDimensionPixelSize(R.dimen.mvt_container_top_padding_polish);
        int expectedMvtBottomPadding =
                res.getDimensionPixelSize(R.dimen.mvt_container_bottom_padding_polish);
        Assert.assertEquals(
                "The top padding of the most visited tiles container is wrong.",
                expectedMvtTopPadding,
                mvTilesContainer.getPaddingTop());
        Assert.assertEquals(
                "The bottom padding of the most visited tiles container is wrong.",
                expectedMvtBottomPadding,
                mvTilesContainer.getPaddingBottom());

        Drawable mvTilesContainerBackground = mvTilesContainer.getBackground();
        Assert.assertEquals(
                "The shape of the background of the most visited tiles container is wrong.",
                GradientDrawable.RECTANGLE,
                ((GradientDrawable) mvTilesContainerBackground).getShape());
    }

    private void captureThumbnail() {
        Canvas canvas = new Canvas();
        mNtp.captureThumbnail(canvas);
    }

    private boolean getUrlFocusAnimationsDisabled() {
        return TestThreadUtils.runOnUiThreadBlockingNoException(
                new Callable<Boolean>() {
                    @Override
                    public Boolean call() {
                        return mNtp.getNewTabPageLayout().urlFocusAnimationsDisabled();
                    }
                });
    }

    private void waitForUrlFocusAnimationsDisabledState(boolean disabled) {
        CriteriaHelper.pollInstrumentationThread(
                () -> Criteria.checkThat(getUrlFocusAnimationsDisabled(), is(disabled)));
    }

    private void waitForTabLoading() {
        CriteriaHelper.pollUiThread(() -> mTab.isLoading());
    }

    private void waitForFakeboxFocusAnimationComplete(NewTabPage ntp) {
        // Tablet doesn't animate fakebox but simply focuses Omnibox upon click.
        // Skip the check on animation.
        if (mActivityTestRule.getActivity().isTablet()) return;
        waitForUrlFocusPercent(ntp, 1f);
    }

    private void waitForUrlFocusPercent(final NewTabPage ntp, float percent) {
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            ntp.getNewTabPageLayout().getUrlFocusChangeAnimationPercent(),
                            is(percent));
                });
    }

    private void clickFakebox() {
        View fakebox = mNtp.getView().findViewById(R.id.search_box);
        TouchCommon.singleClickView(fakebox);
    }

    /**
     * @return The position of the top of the fakebox relative to the window.
     */
    private int getFakeboxTop(final NewTabPage ntp) {
        return TestThreadUtils.runOnUiThreadBlockingNoException(
                new Callable<Integer>() {
                    @Override
                    public Integer call() {
                        final View fakebox = ntp.getView().findViewById(R.id.search_box);
                        int[] location = new int[2];
                        fakebox.getLocationInWindow(location);
                        return location[1];
                    }
                });
    }

    /** Waits until the top of the fakebox reaches the given position. */
    private void waitForFakeboxTopPosition(final NewTabPage ntp, int position) {
        CriteriaHelper.pollUiThread(() -> Criteria.checkThat(getFakeboxTop(ntp), is(position)));
    }

    private static HistogramWatcher expectMostVisitedTilesRecordForNtpModuleClick() {
        return HistogramWatcher.newSingleRecordWatcher(
                HISTOGRAM_NTP_MODULE_CLICK,
                BrowserUiUtils.ModuleTypeOnStartAndNTP.MOST_VISITED_TILES);
    }

    private static HistogramWatcher expectFeedRecordForNtpModuleClick() {
        return HistogramWatcher.newSingleRecordWatcher(
                HISTOGRAM_NTP_MODULE_CLICK, BrowserUiUtils.ModuleTypeOnStartAndNTP.FEED);
    }

    private static HistogramWatcher expectHomeButtonRecordForNtpModuleClick() {
        return HistogramWatcher.newSingleRecordWatcher(
                HISTOGRAM_NTP_MODULE_CLICK, BrowserUiUtils.ModuleTypeOnStartAndNTP.HOME_BUTTON);
    }

    private static HistogramWatcher expectHomeButtonRecordForNtpModuleLongClick() {
        return HistogramWatcher.newSingleRecordWatcher(
                HISTOGRAM_NTP_MODULE_LONGCLICK, BrowserUiUtils.ModuleTypeOnStartAndNTP.HOME_BUTTON);
    }

    private static HistogramWatcher expectNoRecordsForNtpModuleClick() {
        return HistogramWatcher.newBuilder().expectNoRecords(HISTOGRAM_NTP_MODULE_CLICK).build();
    }

    /** Transform the New Tab Page into the signed-in state. */
    private void waitForSignIn() {
        CoreAccountInfo coreAccountInfo =
                mSigninTestRule.addAccount(
                        EMAIL, NAME, SigninTestRule.NON_DISPLAYABLE_EMAIL_ACCOUNT_CAPABILITIES);
        mSigninTestRule.waitForSeeding();
        SigninTestUtil.signin(coreAccountInfo);
        // TODO(https://crbug.com/1132291): Remove the reload once the sign-in without sync observer
        //  is implemented.
        TestThreadUtils.runOnUiThreadBlocking(mTab::reload);
    }
}
