// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.locale;

import android.app.Activity;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.chrome.browser.search_engines.DefaultSearchEngineDialogHelper;
import org.chromium.chrome.browser.search_engines.SearchEnginePromoType;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.browser_ui.settings.SettingsLauncher;
import org.chromium.components.search_engines.TemplateUrl;
import org.chromium.ui.base.PageTransition;

import java.util.List;

/**
 * Manager for some locale specific logics.
 * TODO(https://crbug.com/1198923) Turn this into a per-activity object.
 */
public class LocaleManager implements DefaultSearchEngineDialogHelper.Delegate {
    private static LocaleManager sInstance = new LocaleManager();

    private LocaleManagerDelegate mDelegate;

    /**
     * @return An instance of the {@link LocaleManager}. This should only be called on UI thread.
     */
    @CalledByNative
    public static LocaleManager getInstance() {
        assert ThreadUtils.runningOnUiThread();
        return sInstance;
    }

    /**
     * Default constructor.
     */
    public LocaleManager() {
        mDelegate = new LocaleManagerDelegateImpl();
        mDelegate.setDefaulSearchEngineDelegate(this);
    }

    /**
     * Starts listening to state changes of the phone.
     */
    public void startObservingPhoneChanges() {
        mDelegate.startObservingPhoneChanges();
    }

    /**
     * Stops listening to state changes of the phone.
     */
    public void stopObservingPhoneChanges() {
        mDelegate.stopObservingPhoneChanges();
    }

    /**
     * Starts recording metrics in deferred startup.
     */
    public void recordStartupMetrics() {
        mDelegate.recordStartupMetrics();
    }

    /** Returns whether the Chrome instance is running in a special locale. */
    public boolean isSpecialLocaleEnabled() {
        return mDelegate.isSpecialLocaleEnabled();
    }

    /**
     * @return The country id of the special locale.
     */
    public String getSpecialLocaleId() {
        return mDelegate.getSpecialLocaleId();
    }

    /**
     * Adds local search engines for special locale.
     */
    public void addSpecialSearchEngines() {
        mDelegate.addSpecialSearchEngines();
    }

    /**
     * Removes local search engines for special locale.
     */
    public void removeSpecialSearchEngines() {
        mDelegate.removeSpecialSearchEngines();
    }

    /**
     * Shows a promotion dialog about search engines depending on Locale and other conditions.
     * See {@link LocaleManager#getSearchEnginePromoShowType()} for possible types and logic.
     *
     * @param activity    Activity showing the dialog.
     * @param onSearchEngineFinalized Notified when the search engine has been finalized.  This can
     *                                either mean no dialog is needed, or the dialog was needed and
     *                                the user completed the dialog with a valid selection.
     */
    public void showSearchEnginePromoIfNeeded(
            final Activity activity, final @Nullable Callback<Boolean> onSearchEngineFinalized) {
        mDelegate.showSearchEnginePromoIfNeeded(activity, onSearchEngineFinalized);
    }

    /**
     * Sets whether auto switch for search engine is enabled.
     */
    public void setSearchEngineAutoSwitch(boolean isEnabled) {
        mDelegate.setSearchEngineAutoSwitch(isEnabled);
    }

    /**
     * Sets the {@link SnackbarManager} used by this instance.
     * @param manager SnackbarManager instance.
     */
    public void setSnackbarManager(SnackbarManager manager) {
        mDelegate.setSnackbarManager(manager);
    }

    /**
     * Sets the settings launcher for search engines.
     * @param settingsLauncher Launcher to start search engine settings on the snackbar UI.
     */
    public void setSettingsLauncher(SettingsLauncher settingsLauncher) {
        mDelegate.setSettingsLauncher(settingsLauncher);
    }

    /** Returns whether and which search engine promo should be shown. */
    @SearchEnginePromoType
    public int getSearchEnginePromoShowType() {
        return mDelegate.getSearchEnginePromoShowType();
    }

    /**
     * @return The referral ID to be passed when searching with Yandex as the DSE.
     */
    @CalledByNative
    protected String getYandexReferralId() {
        return mDelegate.getYandexReferralId();
    }

    /**
     * @return The referral ID to be passed when searching with Mail.RU as the DSE.
     */
    @CalledByNative
    protected String getMailRUReferralId() {
        return mDelegate.getMailRUReferralId();
    }

    @Override
    public List<TemplateUrl> getSearchEnginesForPromoDialog(@SearchEnginePromoType int promoType) {
        return mDelegate.getSearchEnginesForPromoDialog(promoType);
    }

    @Override
    public void onUserSearchEngineChoice(
            @SearchEnginePromoType int type, List<String> keywords, String keyword) {
        mDelegate.onUserSearchEngineChoiceFromPromoDialog(type, keywords, keyword);
    }

    /**
     * To be called when the search engine promo dialog is dismissed without the user confirming
     * a valid search engine selection.
     */
    public void onUserLeavePromoDialogWithNoConfirmedChoice(@SearchEnginePromoType int type) {
        mDelegate.onUserLeavePromoDialogWithNoConfirmedChoice(type);
    }

    /** Set a LocaleManager instance. This is called only by AppHooks. */
    public static void setInstance(LocaleManager instance) {
        sInstance = instance;
    }

    /**
     * Record any locale based metrics related with the search widget. Recorded on initialization
     * only.
     * @param widgetPresent Whether there is at least one search widget on home screen.
     */
    public void recordLocaleBasedSearchWidgetMetrics(boolean widgetPresent) {
        mDelegate.recordLocaleBasedSearchWidgetMetrics(widgetPresent);
    }

    /**
     * Returns whether the search engine promo has been shown and the user selected a valid option
     *         and successfully completed the promo.
     */
    public boolean hasCompletedSearchEnginePromo() {
        return mDelegate.hasCompletedSearchEnginePromo();
    }

    /** Returns whether the search engine promo has been shown in this session. */
    public boolean hasShownSearchEnginePromoThisSession() {
        return mDelegate.hasShownSearchEnginePromoThisSession();
    }

    /** Returns whether we still have to check for whether search engine dialog is necessary. */
    public boolean needToCheckForSearchEnginePromo() {
        return mDelegate.needToCheckForSearchEnginePromo();
    }

    /**
     * Record any locale based metrics related with search. Recorded per search.
     * @param isFromSearchWidget Whether the search was performed from the search widget.
     * @param url Url for the search made.
     * @param transition The transition type for the navigation.
     */
    public void recordLocaleBasedSearchMetrics(
            boolean isFromSearchWidget, String url, @PageTransition int transition) {
        mDelegate.recordLocaleBasedSearchMetrics(isFromSearchWidget, url, transition);
    }

    /**
     * Returns whether the user requires special handling.
     */
    public boolean isSpecialUser() {
        return mDelegate.isSpecialUser();
    }

    /**
     * Record metrics related to user type.
     */
    @CalledByNative
    public void recordUserTypeMetrics() {
        mDelegate.recordUserTypeMetrics();
    }

    /** Set a LocaleManagerDelegate to be used for testing. */
    @VisibleForTesting
    public void setDelegateForTest(LocaleManagerDelegate delegate) {
        mDelegate = delegate;
        mDelegate.setDefaulSearchEngineDelegate(this);
    }
}
