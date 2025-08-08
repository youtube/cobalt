// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.searchwidget;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.robolectric.Shadows.shadowOf;

import android.app.Activity;
import android.app.SearchManager;
import android.content.Intent;
import android.view.View;
import android.view.View.OnClickListener;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.android.controller.ActivityController;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;
import org.robolectric.shadows.ShadowActivity;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.JniMocker;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.content.WebContentsFactory;
import org.chromium.chrome.browser.firstrun.FirstRunStatus;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.metrics.UmaActivityObserver;
import org.chromium.chrome.browser.omnibox.LocationBarCoordinator;
import org.chromium.chrome.browser.omnibox.UrlBarCoordinator;
import org.chromium.chrome.browser.omnibox.status.StatusCoordinator;
import org.chromium.chrome.browser.omnibox.suggestions.CachedZeroSuggestionsManager;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxLoadUrlParams;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.rlz.RevenueStats;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactoryJni;
import org.chromium.chrome.browser.searchwidget.SearchActivity.TerminationReason;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabBuilder;
import org.chromium.chrome.browser.ui.searchactivityutils.SearchActivityClient.IntentBuilder;
import org.chromium.chrome.browser.ui.searchactivityutils.SearchActivityExtras.IntentOrigin;
import org.chromium.chrome.browser.ui.searchactivityutils.SearchActivityExtras.ResolutionType;
import org.chromium.chrome.browser.ui.searchactivityutils.SearchActivityExtras.SearchType;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.metrics.OmniboxEventProtos.OmniboxEventProto.PageClassification;
import org.chromium.components.omnibox.OmniboxFeatureList;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.PageTransition;
import org.chromium.url.GURL;

import java.util.Map;
import java.util.Optional;
import java.util.Set;

@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {
            SearchActivityUnitTest.ShadowSearchActivityUtils.class,
            SearchActivityUnitTest.ShadowWebContentsFactory.class,
            SearchActivityUnitTest.ShadowProfileManager.class,
            SearchActivityUnitTest.ShadowRevenueStats.class,
            SearchActivityUnitTest.ShadowTabBuilder.class,
        })
public class SearchActivityUnitTest {
    private static final String TEST_URL = "https://abc.xyz/";
    private static final String TEST_REFERRER = "com.package.name";
    private static final OmniboxLoadUrlParams LOAD_URL_PARAMS_SIMPLE =
            new OmniboxLoadUrlParams.Builder(TEST_URL, PageTransition.TYPED).build();
    private static final String HISTOGRAM_SUFFIX_SEARCH_WIDGET = ".SearchWidget";
    private static final String HISTOGRAM_SUFFIX_SHORTCUTS_WIDGET = ".ShortcutsWidget";
    private static final String HISTOGRAM_SUFFIX_CUSTOM_TAB = ".CustomTab";
    private static final String HISTOGRAM_SUFFIX_LAUNCHER = ".Launcher";
    private static final String HISTOGRAM_SUFFIX_HUB = ".Hub";

    // SearchActivityUtils call intercepting mock.
    private interface TestSearchActivityUtils {
        void resolveOmniboxRequestForResult(Activity activity, OmniboxLoadUrlParams params);
    }

    // Shadow forwarding static calls to TestSearchActivityUtils.
    @Implements(SearchActivityUtils.class)
    public static class ShadowSearchActivityUtils {
        static TestSearchActivityUtils sMockUtils;

        @Implementation
        public static void resolveOmniboxRequestForResult(
                Activity activity, OmniboxLoadUrlParams params) {
            sMockUtils.resolveOmniboxRequestForResult(activity, params);
        }
    }

    @Implements(WebContentsFactory.class)
    public static class ShadowWebContentsFactory {
        static WebContents sMockWebContents;

        @Implementation
        public static WebContents createWebContents(
                Profile p, boolean initiallyHidden, boolean initRenderer) {
            return sMockWebContents;
        }
    }

    @Implements(TabBuilder.class)
    public static class ShadowTabBuilder {
        static Tab sMockTab;

        @Implementation
        public Tab build() {
            return sMockTab;
        }
    }

    @Implements(ProfileManager.class)
    public static class ShadowProfileManager {
        public static Profile sProfile;

        static void setProfile(Profile profile) {
            sProfile = profile;
            ProfileManager.onProfileAdded(profile);
            ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        }

        @Implementation
        public static boolean isInitialized() {
            return sProfile != null;
        }

        @Implementation
        public static Profile getLastUsedRegularProfile() {
            return sProfile;
        }
    }

    @Implements(RevenueStats.class)
    public static class ShadowRevenueStats {
        static Callback<String> sSetCustomTabSearchClient;

        @Implementation
        public static void setCustomTabSearchClient(String client) {
            sSetCustomTabSearchClient.onResult(client);
        }
    }

    public @Rule MockitoRule mMockitoRule = MockitoJUnit.rule();
    public @Rule JniMocker mJniMocker = new JniMocker();
    private @Mock TestSearchActivityUtils mUtils;
    private @Mock TemplateUrlService mTemplateUrlSvc;
    private @Mock Profile mProfile;
    private @Mock TemplateUrlServiceFactoryJni mTemplateUrlFactoryJni;
    private @Mock WebContents mWebContents;
    private @Mock Tab mTab;
    private @Mock SearchActivity.SearchActivityDelegate mDelegate;
    private @Mock SearchActivityLocationBarLayout mLocationBar;
    private @Mock UmaActivityObserver mUmaObserver;
    private @Mock Callback<String> mSetCustomTabSearchClient;
    private ObservableSupplier<Profile> mProfileSupplier;
    private OneshotSupplier<ProfileProvider> mProfileProviderSupplier;

    private ActivityController<SearchActivity> mController;
    private SearchActivity mActivity;
    private ShadowActivity mShadowActivity;
    private SearchBoxDataProvider mDataProvider;

    @Before
    public void setUp() {
        FirstRunStatus.setFirstRunFlowComplete(true);
        mController = Robolectric.buildActivity(SearchActivity.class);
        mActivity = spy(mController.get());
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
        mShadowActivity = shadowOf(mActivity);
        mDataProvider = mActivity.getSearchBoxDataProviderForTesting();

        // Many of the scenarios could be tested by simply applying a test instance of the
        // TemplateUrlService to TemplateUrlServiceFactory#setInstanceForTesting.
        // Some scenarios however require Factory to return null, which isn't currently possible.
        mJniMocker.mock(TemplateUrlServiceFactoryJni.TEST_HOOKS, mTemplateUrlFactoryJni);
        doReturn(mTemplateUrlSvc).when(mTemplateUrlFactoryJni).getTemplateUrlService(any());

        mProfileSupplier = mActivity.getProfileSupplierForTesting();

        SearchActivity.setDelegateForTests(mDelegate);
        mActivity.setLocationBarLayoutForTesting(mLocationBar);
        mActivity.setUmaActivityObserverForTesting(mUmaObserver);
        mProfileProviderSupplier = mActivity.createProfileProvider();

        ShadowSearchActivityUtils.sMockUtils = mUtils;
        ShadowWebContentsFactory.sMockWebContents = mWebContents;
        ShadowTabBuilder.sMockTab = mTab;
        ShadowRevenueStats.sSetCustomTabSearchClient = mSetCustomTabSearchClient;
    }

    @After
    public void tearDown() {
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        FirstRunStatus.setFirstRunFlowComplete(false);
        IdentityServicesProvider.setInstanceForTests(null);
        TemplateUrlServiceFactory.setInstanceForTesting(null);
    }

    private Intent buildTestWidgetIntent(@IntentOrigin int intentOrigin) {
        return newIntentBuilder(intentOrigin, TEST_URL).build();
    }

    private Intent buildTestServiceIntent(@IntentOrigin int intentOrigin) {
        return newIntentBuilder(intentOrigin, TEST_URL)
                .setResolutionType(ResolutionType.SEND_TO_CALLER)
                .build();
    }

    private IntentBuilder newIntentBuilder(@IntentOrigin int intentOrigin, String url) {
        return new SearchActivityClientImpl(mActivity, intentOrigin)
                .newIntentBuilder()
                .setPageUrl(new GURL(url));
    }

    @Test
    public void searchActivity_forcesPhoneUi() {
        assertTrue(mActivity.getEmbedderUiOverridesForTesting().isForcedPhoneStyleOmnibox());
    }

    @Test
    public void loadUrl_dispatchResultToCallingActivity() {
        mActivity.handleNewIntent(buildTestServiceIntent(IntentOrigin.CUSTOM_TAB), false);

        ArgumentCaptor<OmniboxLoadUrlParams> captor =
                ArgumentCaptor.forClass(OmniboxLoadUrlParams.class);

        try (var watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                SearchActivity.HISTOGRAM_SESSION_TERMINATION_REASON,
                                TerminationReason.NAVIGATION)
                        .expectIntRecord(
                                SearchActivity.HISTOGRAM_SESSION_TERMINATION_REASON
                                        + HISTOGRAM_SUFFIX_CUSTOM_TAB,
                                TerminationReason.NAVIGATION)
                        .build()) {
            mActivity.loadUrl(LOAD_URL_PARAMS_SIMPLE, false);
            verify(mUtils).resolveOmniboxRequestForResult(eq(mActivity), captor.capture());
        }

        assertEquals(TEST_URL, captor.getValue().url);
        assertNull(mShadowActivity.getNextStartedActivity());
    }

    @Test
    public void loadUrl_openInChromeBrowser() {
        mActivity.handleNewIntent(
                buildTestWidgetIntent(IntentOrigin.QUICK_ACTION_SEARCH_WIDGET), false);

        mActivity.loadUrl(LOAD_URL_PARAMS_SIMPLE, false);
        verify(mUtils, never()).resolveOmniboxRequestForResult(any(), any());
        assertNotNull(mShadowActivity.getNextStartedActivity());
    }

    @Test
    public void terminateSession_dispatchResultToCallingActivity() {
        var intent = buildTestServiceIntent(IntentOrigin.CUSTOM_TAB);
        mActivity.handleNewIntent(intent, false);
        clearInvocations(mUtils);

        // Start at the first non-NAVIGATION reason. NAVIGATION is covered by a separate test
        // because it resolves termination slightly differently.
        for (@TerminationReason int reason = TerminationReason.NAVIGATION + 1;
                reason < TerminationReason.COUNT;
                reason++) {
            try (var watcher =
                    HistogramWatcher.newBuilder()
                            .expectIntRecord(
                                    SearchActivity.HISTOGRAM_SESSION_TERMINATION_REASON, reason)
                            .expectIntRecord(
                                    SearchActivity.HISTOGRAM_SESSION_TERMINATION_REASON
                                            + HISTOGRAM_SUFFIX_CUSTOM_TAB,
                                    reason)
                            .build()) {
                mActivity.finish(reason, null);
                verify(mUtils).resolveOmniboxRequestForResult(mActivity, null);
                clearInvocations(mUtils);

                // Subsequent calls must be ignored.
                mActivity.finish(reason, null);
                verifyNoMoreInteractions(mUtils);

                mShadowActivity.resetIsFinishing();
            }
        }
    }

    @Test
    public void terminateSession_startsChrome() {
        var intent =
                newIntentBuilder(IntentOrigin.LAUNCHER, TEST_URL)
                        .setResolutionType(ResolutionType.OPEN_OR_LAUNCH_CHROME)
                        .build();
        mActivity.handleNewIntent(intent, false);
        clearInvocations(mUtils);

        // Start at the first non-NAVIGATION reason. NAVIGATION is covered by a separate test
        // because it resolves termination slightly differently.
        for (@TerminationReason int reason = TerminationReason.NAVIGATION + 1;
                reason < TerminationReason.COUNT;
                reason++) {
            try (var watcher =
                    HistogramWatcher.newBuilder()
                            .expectIntRecord(
                                    SearchActivity.HISTOGRAM_SESSION_TERMINATION_REASON, reason)
                            .expectIntRecord(
                                    SearchActivity.HISTOGRAM_SESSION_TERMINATION_REASON
                                            + HISTOGRAM_SUFFIX_LAUNCHER,
                                    reason)
                            .build()) {
                mActivity.finish(reason, null);

                Intent nextStartedActivity = mShadowActivity.getNextStartedActivity();
                assertNotNull(nextStartedActivity);
                assertNull(nextStartedActivity.getData());
                verifyNoMoreInteractions(mUtils);
                clearInvocations(mUtils);

                // Subsequent calls must be ignored.
                mShadowActivity.clearNextStartedActivities();
                mActivity.finish(reason, null);
                assertNull(mShadowActivity.getNextStartedActivity());
                mShadowActivity.resetIsFinishing();
            }
        }
    }

    @Test
    public void terminateSession_searchWidget() {
        mActivity.handleNewIntent(buildTestWidgetIntent(IntentOrigin.SEARCH_WIDGET), false);
        clearInvocations(mUtils);

        for (@TerminationReason int reason = TerminationReason.NAVIGATION;
                reason < TerminationReason.COUNT;
                reason++) {
            try (var watcher =
                    HistogramWatcher.newBuilder()
                            .expectIntRecord(
                                    SearchActivity.HISTOGRAM_SESSION_TERMINATION_REASON, reason)
                            .expectIntRecord(
                                    SearchActivity.HISTOGRAM_SESSION_TERMINATION_REASON
                                            + HISTOGRAM_SUFFIX_SEARCH_WIDGET,
                                    reason)
                            .build()) {
                mActivity.finish(reason, null);
                verifyNoMoreInteractions(mUtils);

                // Verify that termination reason is recorded exactly once.
                mActivity.finish(reason, null);
                verifyNoMoreInteractions(mUtils);
                mShadowActivity.resetIsFinishing();
            }
        }
        verify(mUtils, never()).resolveOmniboxRequestForResult(any(), any());
    }

    @Test
    public void terminateSession_shortcutsWidget() {
        mActivity.handleNewIntent(
                buildTestWidgetIntent(IntentOrigin.QUICK_ACTION_SEARCH_WIDGET), false);
        clearInvocations(mUtils);

        for (@TerminationReason int reason = TerminationReason.NAVIGATION;
                reason < TerminationReason.COUNT;
                reason++) {
            try (var watcher =
                    HistogramWatcher.newBuilder()
                            .expectIntRecord(
                                    SearchActivity.HISTOGRAM_SESSION_TERMINATION_REASON, reason)
                            .expectIntRecord(
                                    SearchActivity.HISTOGRAM_SESSION_TERMINATION_REASON
                                            + HISTOGRAM_SUFFIX_SHORTCUTS_WIDGET,
                                    reason)
                            .build()) {
                mActivity.finish(reason, null);
                verifyNoMoreInteractions(mUtils);

                // Verify that termination reason is recorded exactly once.
                mActivity.finish(reason, null);
                verifyNoMoreInteractions(mUtils);
                mShadowActivity.resetIsFinishing();
            }
        }
        verify(mUtils, never()).resolveOmniboxRequestForResult(any(), any());
    }

    @Test
    public void handleNewIntent_forHubSearch() {
        LocationBarCoordinator locationBarCoordinator = mock(LocationBarCoordinator.class);
        StatusCoordinator statusCoordinator = mock(StatusCoordinator.class);
        doReturn(statusCoordinator).when(locationBarCoordinator).getStatusCoordinator();
        mActivity.setLocationBarCoordinatorForTesting(locationBarCoordinator);

        mActivity.handleNewIntent(buildTestServiceIntent(IntentOrigin.HUB), false);

        assertEquals(
                PageClassification.ANDROID_HUB_VALUE, mDataProvider.getPageClassification(true));
        assertEquals(
                PageClassification.ANDROID_HUB_VALUE, mDataProvider.getPageClassification(false));
        assertFalse(mActivity.getEmbedderUiOverridesForTesting().isLensEntrypointAllowed());
        assertFalse(mActivity.getEmbedderUiOverridesForTesting().isVoiceEntrypointAllowed());

        verify(statusCoordinator).setOnStatusIconNavigateBackButtonPress(any());
    }

    @Test
    public void exitSearchViaCustomBackArrow_HubSearch() {
        LocationBarCoordinator locationBarCoordinator = mock(LocationBarCoordinator.class);
        StatusCoordinator statusCoordinator = mock(StatusCoordinator.class);
        View view = mock(View.class);
        doReturn(statusCoordinator).when(locationBarCoordinator).getStatusCoordinator();
        mActivity.setLocationBarCoordinatorForTesting(locationBarCoordinator);

        ArgumentCaptor<OnClickListener> captor = ArgumentCaptor.forClass(OnClickListener.class);
        var histograms =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                SearchActivity.HISTOGRAM_SESSION_TERMINATION_REASON
                                        + HISTOGRAM_SUFFIX_HUB,
                                TerminationReason.CUSTOM_BACK_ARROW)
                        .build();

        mActivity.handleNewIntent(buildTestServiceIntent(IntentOrigin.HUB), false);
        verify(statusCoordinator).setOnStatusIconNavigateBackButtonPress(captor.capture());
        OnClickListener listener = captor.getValue();
        listener.onClick(view);
        histograms.assertExpected();
    }

    @Test
    public void cancelHubSearch_onBackKeyPressed() {
        LocationBarCoordinator locationBarCoordinator = mock(LocationBarCoordinator.class);
        StatusCoordinator statusCoordinator = mock(StatusCoordinator.class);
        doReturn(statusCoordinator).when(locationBarCoordinator).getStatusCoordinator();
        mActivity.setLocationBarCoordinatorForTesting(locationBarCoordinator);

        mActivity.handleNewIntent(buildTestServiceIntent(IntentOrigin.HUB), false);
        var histograms =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                SearchActivity.HISTOGRAM_SESSION_TERMINATION_REASON
                                        + HISTOGRAM_SUFFIX_HUB,
                                TerminationReason.BACK_KEY_PRESSED)
                        .build();

        assertFalse(mActivity.isFinishing());
        assertFalse(mActivity.isActivityFinishingOrDestroyed());
        mActivity.handleBackKeyPressed();
        assertTrue(mActivity.isActivityFinishingOrDestroyed());
        assertTrue(mActivity.isFinishing());
        histograms.assertExpected();
    }

    @Test
    public void handleNewIntent_forJumpStartOmnibox() {
        // Jump-start Omnibox relies on cached data above anything else.
        // Save some data to confirm it's properly picked.
        String jumpStartUrl = "https://asdf.com/ghjkl";
        CachedZeroSuggestionsManager.saveJumpStartContext(
                new CachedZeroSuggestionsManager.JumpStartContext(new GURL(jumpStartUrl), 123));

        mActivity.handleNewIntent(buildTestServiceIntent(IntentOrigin.LAUNCHER), false);

        assertEquals(123, mDataProvider.getPageClassification(true));
        assertEquals(123, mDataProvider.getPageClassification(false));
        assertEquals(jumpStartUrl, mDataProvider.getCurrentGurl().getSpec());
    }

    @Test
    public void handleNewIntent_forSearchWidget() {
        try (var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        SearchActivity.HISTOGRAM_INTENT_ORIGIN, IntentOrigin.SEARCH_WIDGET)) {
            mActivity.handleNewIntent(buildTestWidgetIntent(IntentOrigin.SEARCH_WIDGET), false);
        }

        assertEquals(
                PageClassification.ANDROID_SEARCH_WIDGET_VALUE,
                mDataProvider.getPageClassification(true));
        assertEquals(
                PageClassification.ANDROID_SEARCH_WIDGET_VALUE,
                mDataProvider.getPageClassification(false));
        assertFalse(mActivity.getEmbedderUiOverridesForTesting().isLensEntrypointAllowed());
        assertTrue(mActivity.getEmbedderUiOverridesForTesting().isVoiceEntrypointAllowed());
    }

    @Test
    public void handleNewIntent_forQuickActionSearchWidget() {
        try (var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        SearchActivity.HISTOGRAM_INTENT_ORIGIN,
                        IntentOrigin.QUICK_ACTION_SEARCH_WIDGET)) {
            mActivity.handleNewIntent(
                    buildTestWidgetIntent(IntentOrigin.QUICK_ACTION_SEARCH_WIDGET), false);
        }

        assertEquals(
                PageClassification.ANDROID_SHORTCUTS_WIDGET_VALUE,
                mDataProvider.getPageClassification(true));
        assertEquals(
                PageClassification.ANDROID_SHORTCUTS_WIDGET_VALUE,
                mDataProvider.getPageClassification(false));
        assertTrue(mActivity.getEmbedderUiOverridesForTesting().isLensEntrypointAllowed());
        assertTrue(mActivity.getEmbedderUiOverridesForTesting().isVoiceEntrypointAllowed());
    }

    @Test
    public void handleNewIntent_forCustomTabNoProfile() {
        try (var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        SearchActivity.HISTOGRAM_INTENT_ORIGIN, IntentOrigin.CUSTOM_TAB)) {
            mActivity.handleNewIntent(buildTestServiceIntent(IntentOrigin.CUSTOM_TAB), false);
        }

        assertEquals(
                PageClassification.OTHER_ON_CCT_VALUE, mDataProvider.getPageClassification(true));
        assertEquals(
                PageClassification.OTHER_ON_CCT_VALUE, mDataProvider.getPageClassification(false));
        assertFalse(mActivity.getEmbedderUiOverridesForTesting().isLensEntrypointAllowed());
        assertFalse(mActivity.getEmbedderUiOverridesForTesting().isVoiceEntrypointAllowed());

        // Note that the profile is not available at this point, so we should not attempt to refine
        // the page class.
        verifyNoMoreInteractions(mTemplateUrlFactoryJni, mTemplateUrlSvc);
    }

    @Test
    public void handleNewIntent_passIntentUrlToLocationBarData() {
        mActivity.handleNewIntent(buildTestWidgetIntent(IntentOrigin.SEARCH_WIDGET), false);
        assertEquals(TEST_URL, mDataProvider.getCurrentGurl().getSpec());
    }

    @Test
    public void recordUsage_searchWidget() {
        int[] searchTypes = new int[] {SearchType.TEXT, SearchType.VOICE, SearchType.LENS};

        for (var searchType : searchTypes) {
            var tester = new UserActionTester();

            SearchActivity.recordUsage(IntentOrigin.SEARCH_WIDGET, searchType);

            var actions = tester.getActions();
            assertEquals(1, actions.size());
            assertEquals(SearchActivity.USED_ANY_FROM_SEARCH_WIDGET, actions.get(0));

            tester.tearDown();
        }
    }

    @Test
    public void recordUsage_customTabs() {
        int[] searchTypes = new int[] {SearchType.TEXT, SearchType.VOICE, SearchType.LENS};

        for (var searchType : searchTypes) {
            var tester = new UserActionTester();

            SearchActivity.recordUsage(IntentOrigin.CUSTOM_TAB, searchType);

            var actions = tester.getActions();
            assertEquals(0, actions.size());

            tester.tearDown();
        }
    }

    @Test
    public void recordUsage_shortcutsWidget() {
        var searchTypes =
                Map.of(
                        SearchType.TEXT,
                        Optional.of(SearchActivity.USED_TEXT_FROM_SHORTCUTS_WIDGET),
                        SearchType.VOICE,
                        Optional.of(SearchActivity.USED_VOICE_FROM_SHORTCUTS_WIDGET),
                        SearchType.LENS,
                        Optional.of(SearchActivity.USED_LENS_FROM_SHORTCUTS_WIDGET),
                        // Invalid search type.
                        ~0,
                        Optional.empty());

        for (var searchType : searchTypes.entrySet()) {
            var tester = new UserActionTester();

            SearchActivity.recordUsage(
                    IntentOrigin.QUICK_ACTION_SEARCH_WIDGET, searchType.getKey());
            var value = searchType.getValue();
            var actions = tester.getActions();
            if (value.isEmpty()) {
                assertEquals(0, actions.size());
            } else {
                assertEquals(1, actions.size());
                assertEquals(value.get(), actions.get(0));
            }

            tester.tearDown();
        }
    }

    @Test
    public void recordUsage_unknownOrigins() {
        var originsToSkip =
                Set.of(
                        IntentOrigin.SEARCH_WIDGET,
                        IntentOrigin.QUICK_ACTION_SEARCH_WIDGET,
                        IntentOrigin.CUSTOM_TAB,
                        IntentOrigin.HUB);
        int[] searchTypes = new int[] {SearchType.TEXT, SearchType.VOICE, SearchType.LENS};

        for (int origin = 0; origin < 10; origin++) {
            if (originsToSkip.contains(origin)) continue;

            for (int searchType : searchTypes) {
                var tester = new UserActionTester();

                SearchActivity.recordUsage(origin, searchType);
                assertEquals(0, tester.getActions().size());

                tester.tearDown();
            }
        }
    }

    @Test
    public void refinePageClassWithProfile_noRefinementForSearchWidget() {
        doReturn(true).when(mTemplateUrlSvc).isSearchResultsPageFromDefaultSearchProvider(any());
        mActivity.handleNewIntent(buildTestWidgetIntent(IntentOrigin.SEARCH_WIDGET), false);

        assertEquals(
                PageClassification.ANDROID_SEARCH_WIDGET_VALUE,
                mDataProvider.getPageClassification(false));
        verifyNoMoreInteractions(mTemplateUrlSvc);
    }

    @Test
    public void refinePageClassWithProfile_noRefinementForShortcutsWidget() {
        doReturn(true).when(mTemplateUrlSvc).isSearchResultsPageFromDefaultSearchProvider(any());
        mActivity.handleNewIntent(
                buildTestWidgetIntent(IntentOrigin.QUICK_ACTION_SEARCH_WIDGET), false);

        assertEquals(
                PageClassification.ANDROID_SHORTCUTS_WIDGET_VALUE,
                mDataProvider.getPageClassification(false));
        verifyNoMoreInteractions(mTemplateUrlSvc);
    }

    @Test
    public void refinePageClassWithProfile_refinesBasicUrlForSearchResultsPage() {
        ShadowProfileManager.setProfile(mProfile);

        {
            // Simulate Search Results Page.
            doReturn(true)
                    .when(mTemplateUrlSvc)
                    .isSearchResultsPageFromDefaultSearchProvider(any());
            mActivity.handleNewIntent(buildTestServiceIntent(IntentOrigin.CUSTOM_TAB), false);
            assertEquals(
                    PageClassification.SEARCH_RESULT_PAGE_ON_CCT_VALUE,
                    mDataProvider.getPageClassification(false));
            assertEquals(
                    PageClassification.SEARCH_RESULT_PAGE_ON_CCT_VALUE,
                    mDataProvider.getPageClassification(true));
        }

        {
            // Simulate arbitrary page.
            doReturn(false)
                    .when(mTemplateUrlSvc)
                    .isSearchResultsPageFromDefaultSearchProvider(any());
            mActivity.handleNewIntent(buildTestServiceIntent(IntentOrigin.CUSTOM_TAB), false);
            assertEquals(
                    PageClassification.OTHER_ON_CCT_VALUE,
                    mDataProvider.getPageClassification(false));
            assertEquals(
                    PageClassification.OTHER_ON_CCT_VALUE,
                    mDataProvider.getPageClassification(true));
        }
    }

    @Test
    public void refinePageClassWithProfile_ignoresNullUrl() {
        doReturn(true).when(mTemplateUrlSvc).isSearchResultsPageFromDefaultSearchProvider(any());
        mActivity.handleNewIntent(
                newIntentBuilder(IntentOrigin.CUSTOM_TAB, /* url= */ null)
                        .setResolutionType(ResolutionType.SEND_TO_CALLER)
                        .build(),
                false);

        assertEquals(
                PageClassification.OTHER_ON_CCT_VALUE, mDataProvider.getPageClassification(false));
        verifyNoMoreInteractions(mTemplateUrlSvc);
    }

    @Test
    public void refinePageClassWithProfile_ignoresEmptyUrl() {
        doReturn(true).when(mTemplateUrlSvc).isSearchResultsPageFromDefaultSearchProvider(any());
        mActivity.handleNewIntent(
                newIntentBuilder(IntentOrigin.CUSTOM_TAB, /* url= */ "")
                        .setResolutionType(ResolutionType.SEND_TO_CALLER)
                        .build(),
                false);

        assertEquals(
                PageClassification.OTHER_ON_CCT_VALUE, mDataProvider.getPageClassification(false));
        verifyNoMoreInteractions(mTemplateUrlSvc);
    }

    @Test
    public void refinePageClassWithProfile_ignoresInvalidUrl() {
        doReturn(true).when(mTemplateUrlSvc).isSearchResultsPageFromDefaultSearchProvider(any());
        mActivity.handleNewIntent(
                newIntentBuilder(IntentOrigin.CUSTOM_TAB, /* url= */ "aoeui")
                        .setResolutionType(ResolutionType.SEND_TO_CALLER)
                        .build(),
                false);

        assertEquals(
                PageClassification.OTHER_ON_CCT_VALUE, mDataProvider.getPageClassification(false));
        verifyNoMoreInteractions(mTemplateUrlSvc);
    }

    @Test
    public void refinePageClassWithProfile_noTemplateUrl() {
        doReturn(null).when(mTemplateUrlFactoryJni).getTemplateUrlService(any());

        mActivity.handleNewIntent(buildTestServiceIntent(IntentOrigin.CUSTOM_TAB), false);

        assertEquals(
                PageClassification.OTHER_ON_CCT_VALUE, mDataProvider.getPageClassification(false));
    }

    @Test
    public void finishNativeInitialization_stopActivityWhenSearchEnginePromoCanceled() {
        mActivity.handleNewIntent(new Intent(), false);
        doNothing().when(mActivity).finishDeferredInitialization();

        ShadowProfileManager.setProfile(mProfile);
        mActivity.finishNativeInitialization();

        ArgumentCaptor<Callback<Boolean>> captor = ArgumentCaptor.forClass(Callback.class);
        verify(mDelegate).showSearchEngineDialogIfNeeded(eq(mActivity), captor.capture());

        // Notify Activity that the search engine promo dialog was canceled.
        captor.getValue().onResult(false);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        verify(mActivity, never()).finishDeferredInitialization();
        assertTrue(mActivity.isFinishing());
    }

    @Test
    public void finishNativeInitialization_stopActivityWhenSearchEnginePromoFailed() {
        mActivity.handleNewIntent(new Intent(), false);
        doNothing().when(mActivity).finishDeferredInitialization();

        ShadowProfileManager.setProfile(mProfile);
        mActivity.finishNativeInitialization();

        ArgumentCaptor<Callback<Boolean>> captor = ArgumentCaptor.forClass(Callback.class);
        verify(mDelegate).showSearchEngineDialogIfNeeded(eq(mActivity), captor.capture());

        // "should never happen".
        captor.getValue().onResult(null);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        verify(mActivity, never()).finishDeferredInitialization();
        assertTrue(mActivity.isFinishing());
    }

    @Test
    public void finishNativeInitialization_resumeActivityAfterSearchEnginePromoCleared() {
        doNothing().when(mActivity).finishDeferredInitialization();

        ShadowProfileManager.setProfile(mProfile);
        mActivity.finishNativeInitialization();

        ArgumentCaptor<Callback<Boolean>> captor = ArgumentCaptor.forClass(Callback.class);
        verify(mDelegate).showSearchEngineDialogIfNeeded(eq(mActivity), captor.capture());

        // Notify Activity that the search engine promo dialog was completed.
        captor.getValue().onResult(true);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        verify(mActivity).finishDeferredInitialization();
        assertFalse(mActivity.isFinishing());
    }

    @Test
    public void finish_recordsUnspecifiedTerminationReason() {
        mActivity.handleNewIntent(new Intent(), false);

        try (var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        SearchActivity.HISTOGRAM_SESSION_TERMINATION_REASON,
                        TerminationReason.UNSPECIFIED)) {
            mActivity.finish();
        }
    }

    @Test
    public void finishNativeInitialization_abortIfActivityTerminated() {
        mActivity.handleNewIntent(new Intent(), false);
        doNothing().when(mActivity).finishDeferredInitialization();

        ShadowProfileManager.setProfile(mProfile);
        mActivity.finishNativeInitialization();

        ArgumentCaptor<Callback<Boolean>> captor = ArgumentCaptor.forClass(Callback.class);
        verify(mDelegate).showSearchEngineDialogIfNeeded(eq(mActivity), captor.capture());

        // Cancel activity, and notify that the search engine promo dialog was completed.
        mActivity.finish(TerminationReason.ACTIVITY_FOCUS_LOST, null);
        captor.getValue().onResult(true);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        verify(mActivity, never()).finishDeferredInitialization();
    }

    @Test
    @EnableFeatures(OmniboxFeatureList.ANDROID_HUB_SEARCH)
    public void finishNativeInitialization_setHubSearchBoxUrlBarElements() {
        LocationBarCoordinator locationBarCoordinator = mock(LocationBarCoordinator.class);
        UrlBarCoordinator urlBarCoordinator = mock(UrlBarCoordinator.class);
        StatusCoordinator statusCoordinator = mock(StatusCoordinator.class);
        doReturn(statusCoordinator).when(locationBarCoordinator).getStatusCoordinator();
        doReturn(urlBarCoordinator).when(locationBarCoordinator).getUrlBarCoordinator();
        mActivity.setLocationBarCoordinatorForTesting(locationBarCoordinator);

        mActivity.handleNewIntent(buildTestServiceIntent(IntentOrigin.HUB), false);

        ShadowProfileManager.setProfile(mProfile);
        mActivity.finishNativeInitialization();

        verify(urlBarCoordinator).setUrlBarHintText(R.string.hub_search_empty_hint);
    }

    @Test
    public void cancelSearch_onBackKeyPressed() {
        mActivity.handleNewIntent(new Intent(), false);

        assertFalse(mActivity.isFinishing());
        assertFalse(mActivity.isActivityFinishingOrDestroyed());
        mActivity.handleBackKeyPressed();
        assertTrue(mActivity.isActivityFinishingOrDestroyed());
        assertTrue(mActivity.isFinishing());
    }

    @Test
    public void cancelSearch_onBackGesture() {
        // Same as above, but with predictive back gesture enabled.
        mActivity.handleNewIntent(new Intent(), false);

        assertFalse(mActivity.isFinishing());
        assertFalse(mActivity.isActivityFinishingOrDestroyed());
        mActivity.getOnBackPressedDispatcher().onBackPressed();
        assertTrue(mActivity.isActivityFinishingOrDestroyed());
        assertTrue(mActivity.isFinishing());
    }

    @Test
    public void cancelSearch_onTapOutside() {
        mActivity.handleNewIntent(new Intent(), false);

        assertFalse(mActivity.isFinishing());
        assertFalse(mActivity.isActivityFinishingOrDestroyed());
        var view = mActivity.createContentView();
        view.performClick();
        assertTrue(mActivity.isActivityFinishingOrDestroyed());
        assertTrue(mActivity.isFinishing());
    }

    @Test
    public void createProfileProvider_tracksProfileManager() {
        assertNull(mProfileSupplier.get());
        ShadowProfileManager.setProfile(mProfile);
        assertEquals(mProfile, mProfileSupplier.get());
    }

    @Test
    public void onNewIntent_applyQuery() {
        var intent = buildTestWidgetIntent(IntentOrigin.SEARCH_WIDGET);
        intent.putExtra(SearchManager.QUERY, "query1");
        mActivity.onNewIntent(intent);
        verify(mLocationBar)
                .beginQuery(
                        eq(IntentOrigin.SEARCH_WIDGET), eq(SearchType.TEXT), eq("query1"), any());

        intent = buildTestWidgetIntent(IntentOrigin.QUICK_ACTION_SEARCH_WIDGET);
        intent.putExtra(SearchManager.QUERY, "query2");
        mActivity.onNewIntent(intent);
        verify(mLocationBar)
                .beginQuery(
                        eq(IntentOrigin.QUICK_ACTION_SEARCH_WIDGET),
                        eq(SearchType.TEXT),
                        eq("query2"),
                        any());

        intent = buildTestWidgetIntent(IntentOrigin.SEARCH_WIDGET);
        intent.putExtra(SearchManager.QUERY, "");
        mActivity.onNewIntent(intent);
        verify(mLocationBar)
                .beginQuery(eq(IntentOrigin.SEARCH_WIDGET), eq(SearchType.TEXT), eq(""), any());

        intent = buildTestServiceIntent(IntentOrigin.CUSTOM_TAB);
        intent.removeExtra(SearchManager.QUERY);
        mActivity.onNewIntent(intent);
        verify(mLocationBar)
                .beginQuery(eq(IntentOrigin.CUSTOM_TAB), eq(SearchType.TEXT), eq(null), any());
    }

    @Test
    public void onResumeWithNative_fromSearchWidget() {
        mActivity.onNewIntent(buildTestWidgetIntent(IntentOrigin.SEARCH_WIDGET));
        mActivity.onResumeWithNative();

        verify(mUmaObserver).startUmaSession(eq(ActivityType.TABBED), eq(null), any());
        verifyNoMoreInteractions(mUmaObserver, mSetCustomTabSearchClient);
    }

    @Test
    public void onResumeWithNative_fromQuickActionWidget() {
        mActivity.onNewIntent(buildTestWidgetIntent(IntentOrigin.QUICK_ACTION_SEARCH_WIDGET));
        mActivity.onResumeWithNative();

        verify(mUmaObserver).startUmaSession(eq(ActivityType.TABBED), eq(null), any());
        verifyNoMoreInteractions(mUmaObserver, mSetCustomTabSearchClient);
    }

    @Test
    public void onResumeWithNative_fromCustomTabs_withoutPackage() {
        SearchActivity.SEARCH_IN_CCT_APPLY_REFERRER_ID.setForTesting(true);
        mActivity.onNewIntent(buildTestServiceIntent(IntentOrigin.CUSTOM_TAB));

        try (var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        SearchActivity.HISTOGRAM_INTENT_REFERRER_VALID, false)) {
            mActivity.onResumeWithNative();
        }

        verify(mUmaObserver).startUmaSession(eq(ActivityType.CUSTOM_TAB), eq(null), any());
        verify(mSetCustomTabSearchClient).onResult(null);
        verifyNoMoreInteractions(mUmaObserver, mSetCustomTabSearchClient);
    }

    @Test
    public void onResumeWithNative_fromCustomTabs_withPackage() {
        SearchActivity.SEARCH_IN_CCT_APPLY_REFERRER_ID.setForTesting(true);
        mActivity.onNewIntent(
                newIntentBuilder(IntentOrigin.CUSTOM_TAB, TEST_URL)
                        .setReferrer(TEST_REFERRER)
                        .setResolutionType(ResolutionType.SEND_TO_CALLER)
                        .build());

        try (var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        SearchActivity.HISTOGRAM_INTENT_REFERRER_VALID, true)) {
            mActivity.onResumeWithNative();
        }

        verify(mUmaObserver).startUmaSession(eq(ActivityType.CUSTOM_TAB), eq(null), any());
        verify(mSetCustomTabSearchClient).onResult("app-cct-" + TEST_REFERRER);
        verifyNoMoreInteractions(mUmaObserver, mSetCustomTabSearchClient);
    }

    @Test
    public void onResumeWithNative_fromCustomTabs_propagationDisabled() {
        SearchActivity.SEARCH_IN_CCT_APPLY_REFERRER_ID.setForTesting(false);
        mActivity.onNewIntent(buildTestServiceIntent(IntentOrigin.CUSTOM_TAB));

        try (var watcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords(SearchActivity.HISTOGRAM_INTENT_REFERRER_VALID)
                        .build()) {
            mActivity.onResumeWithNative();
        }

        verify(mUmaObserver).startUmaSession(eq(ActivityType.CUSTOM_TAB), eq(null), any());
        verify(mSetCustomTabSearchClient, never()).onResult(any());
        verifyNoMoreInteractions(mUmaObserver, mSetCustomTabSearchClient);
    }

    @Test
    public void onPauseWithNative() {
        mActivity.onPauseWithNative();

        verify(mUmaObserver).endUmaSession();
        verify(mSetCustomTabSearchClient).onResult(null);
        verifyNoMoreInteractions(mUmaObserver, mSetCustomTabSearchClient);
    }

    @Test
    public void shouldStartGpuProcess_alwaysTrue() {
        assertTrue(mActivity.shouldStartGpuProcess());
    }

    @Test
    public void onUrlFocusChange_propagateFocusGainEvent() {
        LocationBarCoordinator coordinator = mock(LocationBarCoordinator.class);
        mActivity.setLocationBarCoordinatorForTesting(coordinator);
        mActivity.onUrlFocusChange(true);
        verify(coordinator).setUrlFocusChangeInProgress(false);
        assertFalse(mActivity.isFinishing());
    }

    @Test
    public void recordNavigationTargetType() {
        GURL native_url = new GURL(UrlConstants.NTP_URL);
        GURL search_url = new GURL("https://google.com");
        GURL web_url = new GURL("https://abc.xyz");

        var variants =
                Map.of(
                        native_url, SearchActivity.NavigationTargetType.NATIVE_PAGE,
                        search_url, SearchActivity.NavigationTargetType.SEARCH,
                        web_url, SearchActivity.NavigationTargetType.URL);

        doReturn(true)
                .when(mTemplateUrlSvc)
                .isSearchResultsPageFromDefaultSearchProvider(search_url);

        for (var entry : variants.entrySet()) {
            var type = entry.getValue();
            try (var watcher =
                    HistogramWatcher.newBuilder()
                            .expectIntRecord(SearchActivity.HISTOGRAM_NAVIGATION_TARGET_TYPE, type)
                            .expectIntRecord(
                                    SearchActivity.HISTOGRAM_NAVIGATION_TARGET_TYPE
                                            + HISTOGRAM_SUFFIX_SEARCH_WIDGET,
                                    type)
                            .build()) {
                mActivity.onNewIntent(buildTestWidgetIntent(IntentOrigin.SEARCH_WIDGET));
                mActivity.recordNavigationTargetType(entry.getKey());
            }

            try (var watcher =
                    HistogramWatcher.newBuilder()
                            .expectIntRecord(SearchActivity.HISTOGRAM_NAVIGATION_TARGET_TYPE, type)
                            .expectIntRecord(
                                    SearchActivity.HISTOGRAM_NAVIGATION_TARGET_TYPE
                                            + HISTOGRAM_SUFFIX_SHORTCUTS_WIDGET,
                                    type)
                            .build()) {
                mActivity.onNewIntent(
                        buildTestWidgetIntent(IntentOrigin.QUICK_ACTION_SEARCH_WIDGET));
                mActivity.recordNavigationTargetType(entry.getKey());
            }

            try (var watcher =
                    HistogramWatcher.newBuilder()
                            .expectIntRecord(SearchActivity.HISTOGRAM_NAVIGATION_TARGET_TYPE, type)
                            .expectIntRecord(
                                    SearchActivity.HISTOGRAM_NAVIGATION_TARGET_TYPE
                                            + HISTOGRAM_SUFFIX_CUSTOM_TAB,
                                    type)
                            .build()) {
                mActivity.onNewIntent(buildTestServiceIntent(IntentOrigin.CUSTOM_TAB));
                mActivity.recordNavigationTargetType(entry.getKey());
            }
        }
    }

    @Test
    public void onTopResumedActivityChanged_clearOmniboxFocusIfNotActive() {
        doNothing().when(mActivity).super_onTopResumedActivityChanged(anyBoolean());
        mActivity.onTopResumedActivityChanged(false);
        verify(mLocationBar).clearOmniboxFocus();
        verify(mActivity).super_onTopResumedActivityChanged(false);
    }

    @Test
    public void onTopResumedActivityChanged_requestOmniboxFocusIfActive() {
        doNothing().when(mActivity).super_onTopResumedActivityChanged(anyBoolean());
        mActivity.onTopResumedActivityChanged(true);
        verify(mLocationBar).requestOmniboxFocus();
        verify(mActivity).super_onTopResumedActivityChanged(true);
    }
}
