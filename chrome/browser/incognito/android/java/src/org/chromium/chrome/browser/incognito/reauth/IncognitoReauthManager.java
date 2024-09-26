// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.incognito.reauth;

import android.os.Build;

import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.chrome.browser.device.DeviceClassManager;
import org.chromium.chrome.browser.device_reauth.DeviceAuthRequester;
import org.chromium.chrome.browser.device_reauth.ReauthenticatorBridge;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.user_prefs.UserPrefs;

/**
 * This class is responsible for managing the Incognito re-authentication flow.
 */
public class IncognitoReauthManager {
    private static Boolean sIsIncognitoReauthFeatureAvailableForTesting;
    private ReauthenticatorBridge mReauthenticatorBridge;

    /**
     * A callback interface which is used for re-authentication in {@link
     * IncognitoReauthManager#startReauthenticationFlow(IncognitoReauthCallback)}.
     */
    public interface IncognitoReauthCallback {
        // This is invoked when either the Incognito re-authentication feature is not available or
        // the device screen lock is not setup or there's an authentication already in progress.
        void onIncognitoReauthNotPossible();
        // This is invoked when the Incognito re-authentication resulted in success.
        void onIncognitoReauthSuccess();
        // This is invoked when the Incognito re-authentication resulted in failure.
        void onIncognitoReauthFailure();
    }

    public IncognitoReauthManager() {
        this(ReauthenticatorBridge.create(DeviceAuthRequester.INCOGNITO_REAUTH_PAGE));
    }

    public IncognitoReauthManager(ReauthenticatorBridge reauthenticatorBridge) {
        mReauthenticatorBridge = reauthenticatorBridge;
    }

    /**
     * Starts the authentication flow. This is an asynchronous method call which would invoke the
     * passed {@link IncognitoReauthCallback} parameter once executed.
     *
     * @param incognitoReauthCallback A {@link IncognitoReauthCallback} callback that
     *         would be run once the authentication is executed.
     */
    public void startReauthenticationFlow(
            @NonNull IncognitoReauthCallback incognitoReauthCallback) {
        if (!mReauthenticatorBridge.canUseAuthentication()
                || !isIncognitoReauthFeatureAvailable()) {
            incognitoReauthCallback.onIncognitoReauthNotPossible();
            return;
        }

        mReauthenticatorBridge.reauthenticate(success -> {
            if (success) {
                incognitoReauthCallback.onIncognitoReauthSuccess();
            } else {
                incognitoReauthCallback.onIncognitoReauthFailure();
            }
        }, /*useLastValidAuth=*/false);
    }
    /**
     * @return A boolean indicating whether the platform version supports reauth and the
     *         corresponding Chrome feature flag is on;
     *
     * For a more complete check, rely on the method {@link
     * IncognitoReauthManager#isIncognitoReauthEnabled(Profile)} instead.
     *
     * TODO(crbug.com/1227656): Remove the check on accessibility once the GTS is fully rolled out
     * to accessibility users.
     */
    public static boolean isIncognitoReauthFeatureAvailable() {
        if (sIsIncognitoReauthFeatureAvailableForTesting != null) {
            return sIsIncognitoReauthFeatureAvailableForTesting;
        }
        // The implementation relies on {@link BiometricManager} which was introduced in API
        // level 29. Android Q is not supported due to a potential bug in BiometricPrompt.
        return (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R)
                && !DeviceClassManager.enableAccessibilityLayout(
                        ContextUtils.getApplicationContext())
                && ChromeFeatureList.sIncognitoReauthenticationForAndroid.isEnabled();
    }

    /**
     * @param profile The {@link Profile} which is used to query the preference value of the
     *         Incognito lock setting.
     *
     * @return A boolean indicating if Incognito re-authentication is possible or not.
     */
    public static boolean isIncognitoReauthEnabled(@NonNull Profile profile) {
        return isIncognitoReauthFeatureAvailable()
                && IncognitoReauthSettingUtils.isDeviceScreenLockEnabled()
                && isIncognitoReauthSettingEnabled(profile);
    }

    @VisibleForTesting
    public static void setIsIncognitoReauthFeatureAvailableForTesting(Boolean isAvailable) {
        sIsIncognitoReauthFeatureAvailableForTesting = isAvailable;
    }

    private static boolean isIncognitoReauthSettingEnabled(Profile profile) {
        return UserPrefs.get(profile).getBoolean(Pref.INCOGNITO_REAUTHENTICATION_FOR_ANDROID);
    }
}
