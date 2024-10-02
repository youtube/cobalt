// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tracing.settings;

import android.os.Bundle;

import androidx.annotation.VisibleForTesting;
import androidx.preference.PreferenceFragmentCompat;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.version_info.Channel;
import org.chromium.components.version_info.VersionConstants;
import org.chromium.components.version_info.VersionInfo;

/**
 * Settings fragment containing preferences aimed at Chrome and web developers.
 */
public class DeveloperSettings extends PreferenceFragmentCompat {
    private static final String UI_PREF_BETA_STABLE_HINT = "beta_stable_hint";

    // Non-translated strings:
    private static final String MSG_DEVELOPER_OPTIONS_TITLE = "Developer options";

    private static Boolean sIsEnabledForTests;

    public static boolean shouldShowDeveloperSettings() {
        // Always enabled on canary, dev and local builds, otherwise can be enabled by tapping the
        // Chrome version in Settings>About multiple times.
        if (sIsEnabledForTests != null) return sIsEnabledForTests;

        if (VersionConstants.CHANNEL <= Channel.DEV) return true;
        return SharedPreferencesManager.getInstance().readBoolean(
                ChromePreferenceKeys.SETTINGS_DEVELOPER_ENABLED, false);
    }

    public static void setDeveloperSettingsEnabled() {
        SharedPreferencesManager.getInstance().writeBoolean(
                ChromePreferenceKeys.SETTINGS_DEVELOPER_ENABLED, true);
    }

    @VisibleForTesting
    public static void setIsEnabledForTests(Boolean isEnabled) {
        sIsEnabledForTests = isEnabled;
    }

    @Override
    public void onCreatePreferences(Bundle savedInstanceState, String s) {
        getActivity().setTitle(MSG_DEVELOPER_OPTIONS_TITLE);
        SettingsUtils.addPreferencesFromResource(this, R.xml.developer_preferences);

        if (VersionInfo.isBetaBuild() || VersionInfo.isStableBuild()) {
            getPreferenceScreen().removePreference(findPreference(UI_PREF_BETA_STABLE_HINT));
        }
    }
}
