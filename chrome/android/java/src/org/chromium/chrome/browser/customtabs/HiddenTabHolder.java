// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static androidx.browser.customtabs.CustomTabsIntent.EXTRA_NETWORK;

import android.content.Intent;
import android.os.Bundle;
import android.text.TextUtils;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.browser.customtabs.CustomTabsSessionToken;
import androidx.browser.customtabs.TrustedWebUtils;
import androidx.browser.trusted.TrustedWebActivityIntentBuilder;

import org.chromium.base.Callback;
import org.chromium.base.IntentUtils;
import org.chromium.base.TraceEvent;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.WarmupManager;
import org.chromium.chrome.browser.browserservices.intents.SessionHolder;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityTabController;
import org.chromium.chrome.browser.customtabs.content.TabObserverRegistrar;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preloading.PreloadingDataBridge;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.RedirectHandlerTabHelper;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabLoadIfNeededCaller;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.common.Referrer;
import org.chromium.network.mojom.ReferrerPolicy;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.Origin;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Objects;

/**
 * Holds a hidden tab which may be used to preload pages before a CustomTabActivity is launched.
 *
 * <p>Lifecycle: 1:1 relationship between this and {@link CustomTabsConnection}. <br>
 * Thread safety: Only access on UI Thread. <br>
 * Native: This class needs native to be loaded (since it creates Tabs).
 */
public class HiddenTabHolder {
    private static final String EARLY_NAV_FAILURE_NAME = "CustomTabs.Startup.EarlyNavFailureReason";

    /**
     * Records why External Navigation would have failed if we didn't work around whatever bug we're
     * hitting. Used for debugging.
     */
    @IntDef({
        EarlyNavFailureReason.SUCCESS,
        EarlyNavFailureReason.NO_SESSION,
        EarlyNavFailureReason.NO_SPECULATION,
        EarlyNavFailureReason.WRONG_URL_VALID_SESSION,
        EarlyNavFailureReason.WRONG_REFERRER_VALID_SESSION,
        EarlyNavFailureReason.WRONG_URL_INVALID_SESSION,
        EarlyNavFailureReason.WRONG_REFERRER_INVALID_SESSION,
        EarlyNavFailureReason.NUM_ENTRIES
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface EarlyNavFailureReason {
        /* Not a failure. */
        int SUCCESS = 0;
        /* Intent has no associated session. */
        int NO_SESSION = 1;
        /* Speculation was gone by the time we received the intent. */
        int NO_SPECULATION = 2;
        /* Intent URL doesn't match speculated URL, session is valid. */
        int WRONG_URL_VALID_SESSION = 3;
        /* Intent referrer doesn't match speculated referrer, session is valid. */
        int WRONG_REFERRER_VALID_SESSION = 4;
        /* Intent URL doesn't match speculated URL, session is invalid. */
        int WRONG_URL_INVALID_SESSION = 5;
        /* Intent referrer doesn't match speculated referrer, session is invalid. */
        int WRONG_REFERRER_INVALID_SESSION = 6;

        int NUM_ENTRIES = 6;
    }

    /** Holds the parameters for the current hidden tab speculation. */
    @VisibleForTesting
    static final class SpeculationParams {
        public final SessionHolder<?> session;
        public final HiddenTab hiddenTab;
        public final String referrer;
        public final boolean isEarlyNav;

        private SpeculationParams(
                SessionHolder<?> session,
                String url,
                Tab tab,
                String referrer,
                TabObserverRegistrar tabObserverRegistrar,
                CustomTabObserver customTabObserver,
                CustomTabNavigationEventObserver customTabNavigationEventObserver,
                boolean isEarlyNav) {
            this.session = session;
            this.hiddenTab =
                    new HiddenTab(
                            tab,
                            tabObserverRegistrar,
                            customTabObserver,
                            customTabNavigationEventObserver,
                            url);
            this.referrer = referrer;
            this.isEarlyNav = isEarlyNav;
        }
    }

    public static class HiddenTab {
        public final Tab tab;
        public final TabObserverRegistrar tabObserverRegistrar;
        public final CustomTabObserver customTabObserver;
        public final CustomTabNavigationEventObserver customTabNavigationEventObserver;
        public final String url;

        public HiddenTab(
                Tab tab,
                TabObserverRegistrar tabObserverRegistrar,
                CustomTabObserver customTabObserver,
                CustomTabNavigationEventObserver customTabNavigationEventObserver,
                String url) {
            this.tab = tab;
            this.tabObserverRegistrar = tabObserverRegistrar;
            this.customTabObserver = customTabObserver;
            this.customTabNavigationEventObserver = customTabNavigationEventObserver;
            this.url = url;
        }
    }

    private class HiddenTabObserver extends EmptyTabObserver {
        @Override
        public void onCrash(Tab tab) {
            if (mSpeculation == null || mSpeculation.hiddenTab.tab != tab) return;
            destroyHiddenTab(null);
        }

        @Override
        public void onActivityAttachmentChanged(Tab tab, WindowAndroid window) {
            tab.removeObserver(this);
        }
    }

    @Nullable private SpeculationParams mSpeculation;

    /**
     * Creates a hidden tab and initiates a navigation.
     *
     * @param tabCreatedCallback Callback run with the tab that is created. This is run before the
     *     url is loaded.
     * @param session The {@link CustomTabsSessionToken} for the Tab to be associated with.
     * @param profile The Profile the tab is associated with.
     * @param clientManager The {@link ClientManager} to get referrer information and link
     *     PostMessage.
     * @param url The URL to load into the Tab.
     * @param extras Extras to be passed that may contain referrer information.
     * @param webContents The {@link WebContents} to use in the hidden tab. If null the default is
     *     used.
     */
    void launchUrlInHiddenTab(
            Callback<Tab> tabCreatedCallback,
            SessionHolder<?> session,
            Profile profile,
            ClientManager clientManager,
            String url,
            @Nullable Bundle extras,
            @Nullable WebContents webContents) {
        // If a new speculation arrives with an early-nav in progress, don't clobber the early nav.
        if (mSpeculation != null && mSpeculation.isEarlyNav) return;
        assert mSpeculation == null;
        Intent extrasIntent = new Intent();
        if (extras != null) extrasIntent.putExtras(extras);

        // Ensures no Browser.EXTRA_HEADERS were in the Intent.
        if (IntentHandler.getExtraHeadersFromIntent(extrasIntent) != null) return;

        WarmupManager warmupManager = WarmupManager.getInstance();
        // launchUrlInHiddenTab is called only during speculative loads/as a side effect of
        // mayLaunchUrl being called. Hence, it's safe to always pass targetsNetwork = false because
        // multi-network CCT does not support that. Or, more correctly, currently multi-network CCT
        // ignores these performance improvements, as it's not considered performance critical and
        // supporting this would increase its complexity by a lot.
        if (warmupManager.hasSpareTab(profile, /* targetsNetwork= */ false)
                && webContents != null) {
            warmupManager.destroySpareTab();
        }
        warmupManager.createRegularSpareTab(profile, webContents);
        // In case creating the tab fails for some reason.
        // See above as to why we can always pass targetsNetwork = false.
        if (!warmupManager.hasSpareTab(profile, /* targetsNetwork= */ false)) return;
        Tab tab =
                warmupManager.takeSpareTab(
                        profile, true, TabLaunchType.FROM_SPECULATIVE_BACKGROUND_CREATION);

        tabCreatedCallback.onResult(tab);

        tab.addObserver(new HiddenTabObserver());

        // Updating post message as soon as we have a valid WebContents.
        clientManager.resetPostMessageHandlerForSession(session, tab.getWebContents());

        LoadUrlParams loadParams = new LoadUrlParams(url);
        String referrer = IntentHandler.getReferrerUrlIncludingExtraHeaders(extrasIntent);
        if (referrer == null && clientManager.getDefaultReferrerForSession(session) != null) {
            referrer = clientManager.getDefaultReferrerForSession(session).getUrl();
        }
        if (referrer == null) referrer = "";
        if (!referrer.isEmpty()) {
            loadParams.setReferrer(new Referrer(referrer, ReferrerPolicy.DEFAULT));
        }
        // The sender of an intent can't be trusted, so we navigate from an opaque Origin to
        // avoid sending same-site cookies.
        loadParams.setInitiatorOrigin(Origin.createOpaqueOrigin());

        loadParams.setTransitionType(PageTransition.LINK | PageTransition.FROM_API);
        RedirectHandlerTabHelper.getOrCreateHandlerFor(tab).setIsPrefetchLoadForIntent(true);

        TabObserverRegistrar registrar = new TabObserverRegistrar();
        CustomTabObserver customTabObserver =
                new CustomTabObserver(
                        /* openedByChrome= */ false, session, /* twaStartupUptimeMillis= */ null);
        CustomTabNavigationEventObserver customTabNavigationEventObserver =
                new CustomTabNavigationEventObserver(session, /* forPrerender= */ true);
        CustomTabActivityTabController.addTabNavigationObservers(
                registrar, customTabObserver, customTabNavigationEventObserver, tab, session);

        mSpeculation =
                new SpeculationParams(
                        session,
                        url,
                        tab,
                        referrer,
                        registrar,
                        customTabObserver,
                        customTabNavigationEventObserver,
                        /* isEarlyNav= */ false);
        tab.loadUrl(loadParams);
    }

    /**
     * Returns the preloaded {@link Tab} if it matches the given |url| and |referrer|. Null if no
     * such {@link Tab}. If a {@link Tab} is preloaded but it does not match, it is discarded.
     *
     * @param session The Binder object identifying a session the hidden tab was created for.
     * @param ignoreFragments Whether to ignore fragments while matching the url.
     * @param url The URL the tab is for.
     * @param referrer The referrer to use for |url|.
     * @return The hidden tab, or null.
     */
    @Nullable
    HiddenTab takeHiddenTab(
            @Nullable SessionHolder<?> session,
            boolean ignoreFragments,
            String url,
            Intent intent) {
        try (TraceEvent e = TraceEvent.scoped("CustomTabsConnection.takeHiddenTab")) {
            if (intent.getBooleanExtra(IntentHandler.EXTRA_CCT_EARLY_NAV, false)) {
                recordEarlyNavDebugMetric(session, ignoreFragments, url, intent);
            }

            if (mSpeculation == null) return null;
            // ~10% of CCT startups have no session, allow them to use Early Nav.
            if (!Objects.equals(session, mSpeculation.session)) return null;

            HiddenTab hiddenTab = mSpeculation.hiddenTab;
            String speculatedUrl = hiddenTab.url;
            String speculationReferrer = mSpeculation.referrer;

            mSpeculation = null;

            boolean urlsMatch =
                    ignoreFragments
                            ? UrlUtilities.urlsMatchIgnoringFragments(speculatedUrl, url)
                            : TextUtils.equals(speculatedUrl, url);

            String referrer = IntentHandler.getReferrerUrlIncludingExtraHeaders(intent);
            if (referrer == null) referrer = "";

            if (urlsMatch && TextUtils.equals(speculationReferrer, referrer)) {
                return hiddenTab;
            } else {
                hiddenTab.tab.destroy();
                return null;
            }
        }
    }

    // Not a dynamic string, just an error code enum.
    @SuppressWarnings("NoDynamicStringsInTraceEventCheck")
    void recordEarlyNavDebugMetric(
            @Nullable SessionHolder<?> session,
            boolean ignoreFragments,
            String url,
            Intent intent) {
        @EarlyNavFailureReason int failureReason = EarlyNavFailureReason.SUCCESS;
        if (mSpeculation == null) {
            failureReason = EarlyNavFailureReason.NO_SPECULATION;
        } else if (!Objects.equals(session, mSpeculation.session)) {
            // NO_SESSION redefined to be wrong session. This is just for short term debugging so
            // it's fine.
            failureReason = EarlyNavFailureReason.NO_SESSION;
        } else {
            boolean isSessionValid = CustomTabsConnection.getInstance().isSessionValid(session);
            HiddenTab hiddenTab = mSpeculation.hiddenTab;
            String speculatedUrl = hiddenTab.url;
            boolean urlsMatch =
                    ignoreFragments
                            ? UrlUtilities.urlsMatchIgnoringFragments(speculatedUrl, url)
                            : TextUtils.equals(speculatedUrl, url);
            String referrer = IntentHandler.getReferrerUrlIncludingExtraHeaders(intent);
            if (referrer == null) referrer = "";
            if (!urlsMatch) {
                if (isSessionValid) {
                    failureReason = EarlyNavFailureReason.WRONG_URL_VALID_SESSION;
                } else {
                    failureReason = EarlyNavFailureReason.WRONG_URL_INVALID_SESSION;
                }
            } else {
                if (referrer == null) referrer = "";
                if (!TextUtils.equals(mSpeculation.referrer, referrer)) {
                    if (isSessionValid) {
                        failureReason = EarlyNavFailureReason.WRONG_REFERRER_VALID_SESSION;
                    } else {
                        failureReason = EarlyNavFailureReason.WRONG_REFERRER_INVALID_SESSION;
                    }
                }
            }
        }
        TraceEvent.instant(EARLY_NAV_FAILURE_NAME + "_" + failureReason);
        RecordHistogram.recordEnumeratedHistogram(
                EARLY_NAV_FAILURE_NAME, failureReason, EarlyNavFailureReason.NUM_ENTRIES);
    }

    /** Cancels the speculation for a given session, or any session if null. */
    void destroyHiddenTab(@Nullable SessionHolder<?> session) {
        if (mSpeculation == null) return;
        if (session != null && !session.equals(mSpeculation.session)) return;
        if (mSpeculation.isEarlyNav) return;

        mSpeculation.hiddenTab.tab.destroy();
        mSpeculation = null;
    }

    /** Returns whether there currently is a hidden tab. */
    boolean hasHiddenTab() {
        return mSpeculation != null;
    }

    public boolean startEarlynavigation(Profile profile, Intent intent) {
        // Don't clobber an existing speculation.
        if (mSpeculation != null) return false;

        // CCT Multi-network isn't supported here.
        if (IntentUtils.safeGetParcelableExtra(intent, EXTRA_NETWORK) != null) {
            return false;
        }

        WarmupManager manager = WarmupManager.getInstance();
        if (!manager.hasSpareTab(profile, /* targetsNetwork= */ false)) return false;

        boolean isTrustedWebActivity =
                IntentUtils.safeGetBooleanExtra(
                        intent, TrustedWebUtils.EXTRA_LAUNCH_AS_TRUSTED_WEB_ACTIVITY, false);

        // Start hidden as Tab needs to be shown after observers are attached.
        Tab tab =
                WarmupManager.getInstance()
                        .takeSpareTab(profile, true, TabLaunchType.FROM_EXTERNAL_APP);

        String url = IntentHandler.getUrlFromIntent(intent);
        LoadUrlParams params = new LoadUrlParams(url);
        IntentHandler.addReferrerAndHeaders(params, intent);
        int transitionType =
                isTrustedWebActivity
                        ? PageTransition.AUTO_TOPLEVEL | PageTransition.FROM_API
                        : PageTransition.LINK | PageTransition.FROM_API;
        params.setTransitionType(IntentHandler.getTransitionTypeFromIntent(intent, transitionType));
        params.setInitiatorOrigin(Origin.createOpaqueOrigin());
        RedirectHandlerTabHelper.updateIntentInTab(tab, intent);

        String referrer = IntentHandler.getReferrerUrlIncludingExtraHeaders(intent);
        if (referrer == null) referrer = "";

        TabObserverRegistrar registrar = new TabObserverRegistrar();
        SessionHolder<?> token = SessionHolder.getSessionHolderFromIntent(intent);
        CustomTabObserver customTabObserver =
                new CustomTabObserver(
                        /* openedByChrome= */ false, token, /* twaStartupUptimeMillis= */ null);
        CustomTabNavigationEventObserver customTabNavigationEventObserver =
                new CustomTabNavigationEventObserver(token, /* forPrerender= */ false);
        CustomTabActivityTabController.addTabNavigationObservers(
                registrar, customTabObserver, customTabNavigationEventObserver, tab, token);

        tab.show(TabSelectionType.FROM_NEW, TabLoadIfNeededCaller.REQUEST_TO_SHOW_TAB_THEN_SHOW);

        // Unlike a prerender, this isn't a speculative load, so we can record metrics for it
        // unconditionally.
        customTabObserver.trackNextPageLoadForLaunch(tab, intent);

        if (isTrustedWebActivity) {
            TwaOfflineDataProvider.createFor(
                    tab,
                    url,
                    IntentUtils.safeGetStringArrayListExtra(
                            intent,
                            TrustedWebActivityIntentBuilder.EXTRA_ADDITIONAL_TRUSTED_ORIGINS),
                    CustomTabsConnection.getInstance().getClientPackageNameForSession(token));
        }

        mSpeculation =
                new SpeculationParams(
                        token,
                        url,
                        tab,
                        referrer,
                        registrar,
                        customTabObserver,
                        customTabNavigationEventObserver,
                        /* isEarlyNav= */ true);

        // Notifies PreloadingImpl that a navigation to CCT is happening. This is used to calculate
        // the recall of CCT prefetch's attempt. Please see
        // PreloadingData::setIsNavigationInDomainCallback for more details.
        if (ChromeFeatureList.sPrefetchBrowserInitiatedTriggers.isEnabled()
                && ChromeFeatureList.sCctNavigationalPrefetch.isEnabled()) {
            WebContents webContents = tab.getWebContents();
            if (webContents != null) {
                PreloadingDataBridge.setIsNavigationInDomainCallbackForCct(webContents);
            }
        }

        intent.putExtra(IntentHandler.EXTRA_CCT_EARLY_NAV, true);
        tab.loadUrl(params);
        return true;
    }

    public Tab getHiddenTabForTesting() {
        return mSpeculation != null ? mSpeculation.hiddenTab.tab : null;
    }

    @Nullable
    SpeculationParams getSpeculationParamsForTesting() {
        return mSpeculation;
    }
}
