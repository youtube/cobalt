// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.firstrun;

import android.app.Activity;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.os.SystemClock;
import android.text.TextUtils;

import androidx.annotation.Nullable;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.IntentUtils;
import org.chromium.chrome.browser.AppHooks;
import org.chromium.chrome.browser.LaunchIntentDispatcher;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.intents.WebApkExtras;
import org.chromium.chrome.browser.browserservices.intents.WebappExtras;
import org.chromium.chrome.browser.webapps.WebappLauncherActivity;
import org.chromium.ui.base.DeviceFormFactor;

/**
 * This class makes a decision what FRE type to launch and creates a corresponding intent. Should be
 * instantiated using {@link AppHooks#createFreIntentCreator}.
 */
public class FreIntentCreator {
    /**
     * Creates an intent to launch the First Run Experience.
     *
     * @param caller               Activity instance that is requesting the first run.
     * @param fromIntent           Intent used to launch the caller.
     * @param requiresBroadcast    Whether or not the Intent triggers a BroadcastReceiver.
     * @param preferLightweightFre Whether to prefer the Lightweight First Run Experience.
     * @return Intent to launch First Run Experience.
     */
    public Intent create(Context caller, Intent fromIntent, boolean requiresBroadcast,
            boolean preferLightweightFre) {
        @Nullable
        BrowserServicesIntentDataProvider webApkIntentDataProvider =
                WebappLauncherActivity.maybeSlowlyGenerateWebApkIntentDataProviderFromIntent(
                        fromIntent);

        String associatedAppName = null;
        Intent intentToLaunchAfterFreComplete = fromIntent;
        if (webApkIntentDataProvider != null
                && webApkIntentDataProvider.getWebApkExtras() != null) {
            WebappExtras webappExtras = webApkIntentDataProvider.getWebappExtras();
            associatedAppName = webappExtras.shortName;

            WebApkExtras webApkExtras = webApkIntentDataProvider.getWebApkExtras();
            intentToLaunchAfterFreComplete = WebappLauncherActivity.createRelaunchWebApkIntent(
                    fromIntent, webApkExtras.webApkPackageName, webappExtras.url);
        }

        Intent result = createInternal(caller, fromIntent, preferLightweightFre, associatedAppName);
        addPendingIntent(caller, result, intentToLaunchAfterFreComplete, requiresBroadcast);
        return result;
    }

    /**
     * Selects one specific FRE implementation and creates an intent to launch this implementation.
     * Called by {@link #create} that also adds some final touches to the returned intent.
     *
     * @param caller               Activity instance that is requesting the first run.
     * @param fromIntent           Intent used to launch the caller.
     * @param preferLightweightFre Whether to prefer the Lightweight First Run Experience.
     * @param associatedAppName    WebAPK short name if this FRE flow was triggered by launching a
     *                             WebAPK. Null otherwise.
     * @return Intent to launch First Run Experience.
     */
    protected Intent createInternal(Context caller, Intent fromIntent, boolean preferLightweightFre,
            @Nullable String associatedAppName) {
        // Launch the Generic First Run Experience if it was previously active.
        boolean isGenericFreActive = checkIsGenericFreActive();
        if (preferLightweightFre && !isGenericFreActive) {
            return createLightweightFirstRunIntent(caller, associatedAppName);
        } else {
            return createGenericFirstRunIntent(caller, fromIntent);
        }
    }

    /**
     * Returns an intent to show the lightweight first run activity.
     * @param context                        The context.
     * @param associatedAppName              WebAPK short name if this FRE flow was triggered by
     *                                       launching a WebAPK. Null otherwise.
     */
    private static Intent createLightweightFirstRunIntent(
            Context context, @Nullable String associatedAppName) {
        Intent intent = new Intent(context, LightweightFirstRunActivity.class);
        if (associatedAppName != null) {
            intent.putExtra(
                    LightweightFirstRunActivity.EXTRA_ASSOCIATED_APP_NAME, associatedAppName);
        }
        return intent;
    }

    /**
     * Returns a generic intent to show the First Run Activity.
     * @param context                        The context.
     * @param fromIntent                     The intent that was used to launch Chrome.
     */
    private static Intent createGenericFirstRunIntent(Context context, Intent fromIntent) {
        Class<?> activityClass = shouldSwitchToTabbedMode(context)
                ? TabbedModeFirstRunActivity.class
                : FirstRunActivity.class;
        Intent intent = new Intent(context, activityClass);
        intent.putExtra(FirstRunActivity.EXTRA_COMING_FROM_CHROME_ICON,
                TextUtils.equals(fromIntent.getAction(), Intent.ACTION_MAIN));
        intent.putExtra(FirstRunActivity.EXTRA_CHROME_LAUNCH_INTENT_IS_CCT,
                LaunchIntentDispatcher.isCustomTabIntent(fromIntent));
        intent.putExtra(FirstRunActivityBase.EXTRA_FRE_INTENT_CREATION_ELAPSED_REALTIME_MS,
                SystemClock.elapsedRealtime());

        // Copy extras bundle from intent which was used to launch Chrome. Copying the extras
        // enables the FirstRunActivity to locate the associated CustomTabsSession (if there
        // is one) and to notify the connection of whether the FirstRunActivity was completed.
        Bundle fromIntentExtras = fromIntent.getExtras();
        if (fromIntentExtras != null) {
            Bundle copiedFromExtras = new Bundle(fromIntentExtras);
            intent.putExtra(FirstRunActivity.EXTRA_CHROME_LAUNCH_INTENT_EXTRAS, copiedFromExtras);
        }

        return intent;
    }

    /**
     * Adds fromIntent as a PendingIntent to the firstRunIntent. This should be used to add a
     * PendingIntent that will be sent when first run is completed.
     *
     * @param context                        The context that corresponds to the Intent.
     * @param firstRunIntent                 The intent that will be used to start first run.
     * @param intentToLaunchAfterFreComplete The intent to launch when the user completes the FRE.
     * @param requiresBroadcast              Whether or not the fromIntent must be broadcasted.
     */
    private static void addPendingIntent(Context context, Intent firstRunIntent,
            Intent intentToLaunchAfterFreComplete, boolean requiresBroadcast) {
        final PendingIntent pendingIntent;
        int pendingIntentFlags = PendingIntent.FLAG_UPDATE_CURRENT | PendingIntent.FLAG_ONE_SHOT
                | IntentUtils.getPendingIntentMutabilityFlag(false);
        if (requiresBroadcast) {
            pendingIntent = PendingIntent.getBroadcast(
                    context, 0, intentToLaunchAfterFreComplete, pendingIntentFlags);
        } else {
            pendingIntent = PendingIntent.getActivity(
                    context, 0, intentToLaunchAfterFreComplete, pendingIntentFlags);
        }
        firstRunIntent.putExtra(FirstRunActivity.EXTRA_FRE_COMPLETE_LAUNCH_INTENT, pendingIntent);
    }

    /** Returns whether the generic FRE is active. */
    private static boolean checkIsGenericFreActive() {
        for (Activity activity : ApplicationStatus.getRunningActivities()) {
            // TabbedModeFirstRunActivity extends FirstRunActivity. LightweightFirstRunActivity
            // does not.
            if (activity instanceof FirstRunActivity) {
                return true;
            }
        }
        return false;
    }

    /**
     * On tablets, where FRE activity is a dialog, transitions from fillscreen activities
     * (the ones that use Theme.Chromium.TabbedMode, e.g. ChromeTabbedActivity) look ugly, because
     * when FRE is started from CTA.onCreate(), currently running animation for CTA window
     * is aborted. This is perceived as a flash of white and doesn't look good.
     *
     * To solve this, we added TabbedMode FRE activity, which has the same window background
     * as Theme.Chromium.TabbedMode activities, but shows content in a FRE-like dialog.
     *
     * This function returns whether to use the TabbedModeFRE.
     */
    private static boolean shouldSwitchToTabbedMode(Context caller) {
        // Caller must be an activity.
        if (!(caller instanceof Activity)) return false;

        // We must be on a tablet (where FRE is a dialog).
        if (!DeviceFormFactor.isNonMultiDisplayContextOnTablet(caller)) return false;

        return true;
    }
}
