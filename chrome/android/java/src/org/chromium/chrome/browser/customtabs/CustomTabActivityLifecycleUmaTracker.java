// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static org.chromium.chrome.browser.dependency_injection.ChromeCommonQualifiers.SAVED_INSTANCE_SUPPLIER;

import android.app.Activity;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.os.SystemClock;
import android.text.TextUtils;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.IntentUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.customtabs.features.TabInteractionRecorder;
import org.chromium.chrome.browser.customtabs.features.sessionrestore.SessionRestoreUtils;
import org.chromium.chrome.browser.dependency_injection.ActivityScope;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.NativeInitObserver;
import org.chromium.chrome.browser.lifecycle.PauseResumeWithNativeObserver;
import org.chromium.chrome.browser.lifecycle.StartStopWithNativeObserver;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.webapps.WebappCustomTabTimeSpentLogger;

import javax.inject.Inject;
import javax.inject.Named;

/**
 * Handles recording User Metrics for Custom Tab Activity.
 */
@ActivityScope
public class CustomTabActivityLifecycleUmaTracker
        implements PauseResumeWithNativeObserver, StartStopWithNativeObserver, NativeInitObserver {
    private final BrowserServicesIntentDataProvider mIntentDataProvider;
    private final Supplier<Bundle> mSavedInstanceStateSupplier;
    private final Activity mActivity;
    private final CustomTabsConnection mConnection;

    private WebappCustomTabTimeSpentLogger mWebappTimeSpentLogger;
    private boolean mIsInitialResume = true;

    private void recordIncognitoLaunchReason() {
        IncognitoCustomTabIntentDataProvider incognitoProvider =
                (IncognitoCustomTabIntentDataProvider) mIntentDataProvider;

        @IntentHandler.IncognitoCCTCallerId
        int incognitoCCTCallerId = incognitoProvider.getFeatureIdForMetricsCollection();
        RecordHistogram.recordEnumeratedHistogram("CustomTabs.IncognitoCCTCallerId",
                incognitoCCTCallerId, IntentHandler.IncognitoCCTCallerId.NUM_ENTRIES);

        // Record which 1P app launched Incognito CCT.
        if (incognitoCCTCallerId == IntentHandler.IncognitoCCTCallerId.GOOGLE_APPS) {
            String sendersPackageName = incognitoProvider.getSendersPackageName();
            @IntentHandler.ExternalAppId
            int externalId = IntentHandler.mapPackageToExternalAppId(sendersPackageName);
            if (externalId != IntentHandler.ExternalAppId.OTHER) {
                RecordHistogram.recordEnumeratedHistogram("CustomTabs.ClientAppId.Incognito",
                        externalId, IntentHandler.ExternalAppId.NUM_ENTRIES);
            } else {
                // Using package name didn't give any meaningful insight on who launched the
                // Incognito CCT, falling back to check if they provided EXTRA_APPLICATION_ID.
                externalId =
                        IntentHandler.determineExternalIntentSource(incognitoProvider.getIntent());
                RecordHistogram.recordEnumeratedHistogram("CustomTabs.ClientAppId.Incognito",
                        externalId, IntentHandler.ExternalAppId.NUM_ENTRIES);
            }
        }
    }

    private void recordUserAction() {
        if (mIntentDataProvider.isOpenedByChrome()) {
            RecordUserAction.record("ChromeGeneratedCustomTab.StartedInitially");
        } else {
            RecordUserAction.record("CustomTabs.StartedInitially");
        }
    }

    private void recordMetrics() {
        if (mIntentDataProvider.isIncognito()) {
            recordIncognitoLaunchReason();
        } else {
            @IntentHandler.ExternalAppId
            int externalId =
                    IntentHandler.determineExternalIntentSource(mIntentDataProvider.getIntent());
            RecordHistogram.recordEnumeratedHistogram(
                    "CustomTabs.ClientAppId", externalId, IntentHandler.ExternalAppId.NUM_ENTRIES);
        }
    }

    @Inject
    public CustomTabActivityLifecycleUmaTracker(ActivityLifecycleDispatcher lifecycleDispatcher,
            BrowserServicesIntentDataProvider intentDataProvider, Activity activity,
            @Named(SAVED_INSTANCE_SUPPLIER) Supplier<Bundle> savedInstanceStateSupplier,
            CustomTabsConnection connection) {
        mIntentDataProvider = intentDataProvider;
        mActivity = activity;
        mSavedInstanceStateSupplier = savedInstanceStateSupplier;
        mConnection = connection;

        lifecycleDispatcher.register(this);
    }

    @Override
    public void onResumeWithNative() {
        if (mSavedInstanceStateSupplier.get() != null || !mIsInitialResume) {
            if (mIntentDataProvider.isOpenedByChrome()) {
                RecordUserAction.record("ChromeGeneratedCustomTab.StartedReopened");
            } else {
                RecordUserAction.record("CustomTabs.StartedReopened");
            }
        } else {
            SharedPreferencesManager preferences = SharedPreferencesManager.getInstance();
            String lastUrl =
                    preferences.readString(ChromePreferenceKeys.CUSTOM_TABS_LAST_URL, null);
            String urlToLoad = mIntentDataProvider.getUrlToLoad();
            boolean launchWithSameUrl = lastUrl != null && lastUrl.equals(urlToLoad);
            if (launchWithSameUrl) {
                RecordUserAction.record("CustomTabsMenuOpenSameUrl");
            } else {
                preferences.writeString(ChromePreferenceKeys.CUSTOM_TABS_LAST_URL, urlToLoad);
            }

            String clientPackage = mIntentDataProvider.getClientPackageName();
            String referrer = getReferrerUriString(mActivity);
            int taskId = mActivity.getTaskId();

            recordForRetainableSessions(
                    clientPackage, referrer, taskId, preferences, launchWithSameUrl);
            TabInteractionRecorder.resetTabInteractionRecords();

            recordUserAction();
            recordMetrics();
        }

        mIsInitialResume = false;

        mWebappTimeSpentLogger = WebappCustomTabTimeSpentLogger.createInstanceAndStartTimer(
                mIntentDataProvider.getIntent().getIntExtra(
                        CustomTabIntentDataProvider.EXTRA_BROWSER_LAUNCH_SOURCE,
                        CustomTabIntentDataProvider.LaunchSourceType.OTHER));
    }

    @Override
    public void onPauseWithNative() {
        if (mWebappTimeSpentLogger != null) {
            mWebappTimeSpentLogger.onPause();
        }
    }

    @Override
    public void onStartWithNative() {
        mConnection.setCustomTabIsInForeground(mIntentDataProvider.getSession(), true);
    }

    @Override
    public void onStopWithNative() {
        mConnection.setCustomTabIsInForeground(mIntentDataProvider.getSession(), false);
    }

    @Override
    public void onFinishNativeInitialization() {
        if (mWebappTimeSpentLogger != null) {
            mWebappTimeSpentLogger.onPause();
        }
    }

    /**
     * Update shared preferences and record histogram when a retainable CCT session is launched back
     * to back with same embedded app and URL, and user action was seen in the previous CCT.
     *
     * @param clientPackage Package name get from CCT service
     * @param referrer Referrer of the CCT activity.
     * @param taskId The task Id of CCT activity.
     * @param preferences Instance from {@link SharedPreferencesManager#getInstance()}.
     */
    @VisibleForTesting
    static void recordForRetainableSessions(String clientPackage, String referrer, int taskId,
            SharedPreferencesManager preferences, boolean launchWithSameUrl) {
        String prevClientPackage =
                preferences.readString(ChromePreferenceKeys.CUSTOM_TABS_LAST_CLIENT_PACKAGE, null);
        String prevReferrer =
                preferences.readString(ChromePreferenceKeys.CUSTOM_TABS_LAST_REFERRER, null);
        int prevTaskId = preferences.readInt(ChromePreferenceKeys.CUSTOM_TABS_LAST_TASK_ID);

        updateSessionPreferences(preferences, clientPackage, referrer, taskId);

        if (!launchWithSameUrl
                || !preferences.readBoolean(
                        ChromePreferenceKeys.CUSTOM_TABS_LAST_CLOSE_TAB_INTERACTION, false)) {
            return;
        }

        String histogramSuffix = SessionRestoreUtils.getClientIdentifierType(
                clientPackage, prevClientPackage, referrer, prevReferrer, taskId, prevTaskId);
        String histogramPrefix = "CustomTabs.RetainableSessionsV2.TimeBetweenLaunch";
        long time = SystemClock.uptimeMillis();
        long lastClosedTime =
                preferences.readLong(ChromePreferenceKeys.CUSTOM_TABS_LAST_CLOSE_TIMESTAMP);
        if (lastClosedTime != 0 && lastClosedTime < time) {
            RecordHistogram.recordLongTimesHistogram(
                    histogramPrefix + histogramSuffix, time - lastClosedTime);
        }
    }

    /**
     * Get the referrer for the given activity. If the activity is launched through launcher
     * activity, the referrer is set through {@link IntentHandler#EXTRA_ACTIVITY_REFERRER}; if not,
     * check {@link Activity#getReferrer()}; if both return empty, fallback to
     * {@link IntentHandler#getReferrerUrlIncludingExtraHeaders(Intent)}.
     * TODO(https://crbug.com/1350252): Move this to IntentHandler.
     */
    public static String getReferrerUriString(Activity activity) {
        if (activity == null || activity.getIntent() == null) {
            return "";
        }

        Intent intent = activity.getIntent();
        String extraReferrer =
                IntentUtils.safeGetStringExtra(intent, IntentHandler.EXTRA_ACTIVITY_REFERRER);
        if (extraReferrer != null) {
            return extraReferrer;
        }

        Uri activityReferrer = activity.getReferrer();
        if (activityReferrer != null) {
            return activityReferrer.toString();
        }
        return IntentHandler.getReferrerUrlIncludingExtraHeaders(intent);
    }

    /**
     * Updates the SharedPreferences for CCT session restore such that the current session is what
     * will be compared back to as the previous session in the next potentially restorable
     * Custom Tab activity.
     *
     * @param preferences Instance from {@link SharedPreferencesManager#getInstance()}.
     * @param clientPackage Package name from CCT service.
     * @param referrer Referrer of the CCT activity.
     * @param taskId The task Id of CCT activity.
     */
    static void updateSessionPreferences(SharedPreferencesManager preferences, String clientPackage,
            String referrer, int taskId) {
        preferences.writeInt(ChromePreferenceKeys.CUSTOM_TABS_LAST_TASK_ID, taskId);
        if (TextUtils.isEmpty(clientPackage) && TextUtils.isEmpty(referrer)) {
            preferences.removeKey(ChromePreferenceKeys.CUSTOM_TABS_LAST_CLIENT_PACKAGE);
            preferences.removeKey(ChromePreferenceKeys.CUSTOM_TABS_LAST_REFERRER);
        } else if (TextUtils.isEmpty(clientPackage)) {
            preferences.removeKey(ChromePreferenceKeys.CUSTOM_TABS_LAST_CLIENT_PACKAGE);
            preferences.writeString(ChromePreferenceKeys.CUSTOM_TABS_LAST_REFERRER, referrer);
        } else {
            preferences.writeString(
                    ChromePreferenceKeys.CUSTOM_TABS_LAST_CLIENT_PACKAGE, clientPackage);
            preferences.removeKey(ChromePreferenceKeys.CUSTOM_TABS_LAST_REFERRER);
        }
    }
}
