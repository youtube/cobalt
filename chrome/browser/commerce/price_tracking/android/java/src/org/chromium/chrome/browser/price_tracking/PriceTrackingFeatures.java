// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.price_tracking;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.FeatureList;
import org.chromium.chrome.browser.commerce.ShoppingServiceFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.UnifiedConsentServiceBridge;
import org.chromium.components.commerce.core.ShoppingService;
import org.chromium.components.signin.identitymanager.ConsentLevel;

import java.util.concurrent.TimeUnit;

/** Flag configuration for price tracking features. */
public class PriceTrackingFeatures {
    @VisibleForTesting
    public static final String ALLOW_DISABLE_PRICE_ANNOTATIONS_PARAM =
            "allow_disable_price_annotations";
    @VisibleForTesting
    public static final String PRICE_DROP_IPH_ENABLED_PARAM = "enable_price_drop_iph";
    private static final String PRICE_DROP_BADGE_ENABLED_PARAM = "enable_price_drop_badge";
    private static final String PRICE_ANNOTATIONS_ENABLED_METRICS_WINDOW_DURATION_PARAM =
            "price_annotations_enabled_metrics_window_duration_ms";

    private static Boolean sIsSignedInAndSyncEnabledForTesting;
    private static Boolean sPriceTrackingEnabledForTesting;

    /**
     * @return Whether the price tracking feature is eligible to work. Now it is used to determine
     *         whether the menu item "track prices" is visible and whether the tab has {@link
     *         TabProperties#SHOPPING_PERSISTED_TAB_DATA_FETCHER}.
     */
    // TODO(b:277218890): Currently the method isPriceTrackingEnabled() is gating some
    // infrastructure setup such as registering the message card in the tab switcher and adding
    // observers for the price annotation preference, while the method isPriceTrackingEligible()
    // requires users to sign in and enable MSBB and the returned value can change at runtime. We
    // should implement this method in native as well and rename isPriceTrackingEnabled() to be less
    // confusing.
    public static boolean isPriceTrackingEligible() {
        if (sIsSignedInAndSyncEnabledForTesting != null) {
            return isPriceTrackingEnabled() && sIsSignedInAndSyncEnabledForTesting;
        }
        return isPriceTrackingEnabled() && isSignedIn() && isAnonymizedUrlDataCollectionEnabled();
    }

    /** Wrapper function for ShoppingService.isCommercePriceTrackingEnabled(). */
    public static boolean isPriceTrackingEnabled() {
        if (sPriceTrackingEnabledForTesting != null) return sPriceTrackingEnabledForTesting;
        if (!ProfileManager.isInitialized()) return false;

        // TODO(b:277218890): Pass profile into this method/class instead of calling the
        // Profile.getLastUsedRegularProfile() method.
        Profile profile = Profile.getLastUsedRegularProfile();
        if (profile == null) return false;
        ShoppingService service = ShoppingServiceFactory.getForProfile(profile);
        if (service == null) return false;
        return service.isCommercePriceTrackingEnabled();
    }

    private static boolean isSignedIn() {
        return IdentityServicesProvider.get()
                .getIdentityManager(Profile.getLastUsedRegularProfile())
                .hasPrimaryAccount(ConsentLevel.SYNC);
    }

    private static boolean isAnonymizedUrlDataCollectionEnabled() {
        return UnifiedConsentServiceBridge.isUrlKeyedAnonymizedDataCollectionEnabled(
                Profile.getLastUsedRegularProfile());
    }

    @VisibleForTesting
    public static void setIsSignedInAndSyncEnabledForTesting(Boolean isSignedInAndSyncEnabled) {
        sIsSignedInAndSyncEnabledForTesting = isSignedInAndSyncEnabled;
    }

    /**
     * @return how frequent we want to record metrics on whether user enables the price tracking
     *         annotations.
     */
    public static int getAnnotationsEnabledMetricsWindowDurationMilliSeconds() {
        int defaultDuration = (int) TimeUnit.DAYS.toMillis(1);
        if (FeatureList.isInitialized()) {
            return ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                    ChromeFeatureList.COMMERCE_PRICE_TRACKING,
                    PRICE_ANNOTATIONS_ENABLED_METRICS_WINDOW_DURATION_PARAM, defaultDuration);
        }
        return defaultDuration;
    }

    /**
     * @return whether we allow users to disable the price annotations feature.
     */
    public static boolean allowUsersToDisablePriceAnnotations() {
        if (FeatureList.isInitialized()) {
            return isPriceTrackingEligible()
                    && ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                            ChromeFeatureList.COMMERCE_PRICE_TRACKING,
                            ALLOW_DISABLE_PRICE_ANNOTATIONS_PARAM, true);
        }
        return isPriceTrackingEligible();
    }

    public static boolean isPriceDropIphEnabled() {
        if (FeatureList.isInitialized()) {
            return isPriceTrackingEligible()
                    && ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                            ChromeFeatureList.COMMERCE_PRICE_TRACKING, PRICE_DROP_IPH_ENABLED_PARAM,
                            false);
        }
        return isPriceTrackingEligible();
    }

    public static boolean isPriceDropBadgeEnabled() {
        if (FeatureList.isInitialized()) {
            return isPriceTrackingEligible()
                    && ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                            ChromeFeatureList.COMMERCE_PRICE_TRACKING,
                            PRICE_DROP_BADGE_ENABLED_PARAM, false);
        }
        return isPriceTrackingEligible();
    }

    @VisibleForTesting
    public static void setPriceTrackingEnabledForTesting(Boolean enabled) {
        sPriceTrackingEnabledForTesting = enabled;
    }
}
