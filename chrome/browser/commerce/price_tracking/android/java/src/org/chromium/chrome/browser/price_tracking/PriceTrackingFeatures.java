// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.price_tracking;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.FeatureList;
import org.chromium.base.ResettersForTesting;
import org.chromium.chrome.browser.commerce.ShoppingServiceFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.UnifiedConsentServiceBridge;
import org.chromium.components.commerce.core.CommerceFeatureUtils;
import org.chromium.components.commerce.core.ShoppingService;
import org.chromium.components.signin.identitymanager.ConsentLevel;

import java.util.concurrent.TimeUnit;

/** Flag configuration for price tracking features. */
public class PriceTrackingFeatures {
    @VisibleForTesting
    public static final String ALLOW_DISABLE_PRICE_ANNOTATIONS_PARAM =
            "allow_disable_price_annotations";

    private static final String PRICE_ANNOTATIONS_ENABLED_METRICS_WINDOW_DURATION_PARAM =
            "price_annotations_enabled_metrics_window_duration_ms";

    private static Boolean sIsSignedInAndSyncEnabledForTesting;
    private static Boolean sPriceAnnotationsEnabledForTesting;

    /**
     * @return Whether the price annotations feature is eligible to work. Now it is used to
     *     determine whether the tab has {@link TabProperties#SHOPPING_PERSISTED_TAB_DATA_FETCHER}.
     */
    // TODO(b:277218890): Currently the method isPriceAnnotationsEnabled() is gating some
    // infrastructure setup such as registering the message card in the tab switcher and adding
    // observers for the price annotation preference, while the method isPriceAnnotationsEligible()
    // requires users to sign in and enable MSBB and the returned value can change at runtime. We
    // should implement this method in native.
    public static boolean isPriceAnnotationsEligible(Profile profile) {
        if (sIsSignedInAndSyncEnabledForTesting != null) {
            return isPriceAnnotationsEnabled(profile) && sIsSignedInAndSyncEnabledForTesting;
        }
        return isPriceAnnotationsEnabled(profile)
                && isSignedIn(profile)
                && isAnonymizedUrlDataCollectionEnabled(profile);
    }

    /** Wrapper function for ShoppingService.isCommercePriceTrackingEnabled(). */
    public static boolean isPriceAnnotationsEnabled(Profile profile) {
        if (sPriceAnnotationsEnabledForTesting != null) {
            return sPriceAnnotationsEnabledForTesting;
        }
        if (profile == null) return false;
        ShoppingService service = ShoppingServiceFactory.getForProfile(profile);
        if (service == null) return false;
        return CommerceFeatureUtils.isPriceAnnotationsEnabled(service);
    }

    private static boolean isSignedIn(Profile profile) {
        // Always return false for incognito profiles.
        if (profile.isOffTheRecord()) {
            return false;
        }
        return IdentityServicesProvider.get()
                .getIdentityManager(profile)
                .hasPrimaryAccount(ConsentLevel.SIGNIN);
    }

    private static boolean isAnonymizedUrlDataCollectionEnabled(Profile profile) {
        return UnifiedConsentServiceBridge.isUrlKeyedAnonymizedDataCollectionEnabled(profile);
    }

    public static void setIsSignedInAndSyncEnabledForTesting(Boolean isSignedInAndSyncEnabled) {
        sIsSignedInAndSyncEnabledForTesting = isSignedInAndSyncEnabled;
        ResettersForTesting.register(() -> sIsSignedInAndSyncEnabledForTesting = null);
    }

    /**
     * @return how frequent we want to record metrics on whether user enables the price tracking
     *         annotations.
     */
    public static int getAnnotationsEnabledMetricsWindowDurationMilliSeconds() {
        int defaultDuration = (int) TimeUnit.DAYS.toMillis(1);
        if (FeatureList.isInitialized()) {
            return ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                    ChromeFeatureList.PRICE_ANNOTATIONS,
                    PRICE_ANNOTATIONS_ENABLED_METRICS_WINDOW_DURATION_PARAM,
                    defaultDuration);
        }
        return defaultDuration;
    }

    /**
     * @return whether we allow users to disable the price annotations feature.
     */
    public static boolean allowUsersToDisablePriceAnnotations(Profile profile) {
        if (FeatureList.isInitialized()) {
            return isPriceAnnotationsEligible(profile)
                    && ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                            ChromeFeatureList.PRICE_ANNOTATIONS,
                            ALLOW_DISABLE_PRICE_ANNOTATIONS_PARAM,
                            true);
        }
        return isPriceAnnotationsEligible(profile);
    }

    public static void setPriceAnnotationsEnabledForTesting(Boolean enabled) {
        sPriceAnnotationsEnabledForTesting = enabled;
        ResettersForTesting.register(() -> sPriceAnnotationsEnabledForTesting = null);
    }
}
