// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices;

import android.os.Bundle;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.appcompat.app.AppCompatActivity;
import androidx.browser.customtabs.CustomTabsSessionToken;

import org.chromium.base.Log;
import org.chromium.chrome.browser.ChromeApplicationImpl;
import org.chromium.chrome.browser.browserservices.metrics.TrustedWebActivityUmaRecorder;
import org.chromium.chrome.browser.customtabs.CustomTabsConnection;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.webapk.lib.common.WebApkConstants;

/**
 * Launched by Trusted Web Activity apps when the user clears data.
 * Redirects to the site-settings activity showing the websites associated with the calling app.
 * The calling app's identity is established using {@link CustomTabsSessionToken} provided in the
 * intent.
 */
public class ManageTrustedWebActivityDataActivity extends AppCompatActivity {

    private static final String TAG = "TwaDataActivity";

    private static String sMockCallingPackage;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        String urlToLaunchSettingsFor = getIntent().getData().toString();
        boolean isWebApk = getIntent().getBooleanExtra(WebApkConstants.EXTRA_IS_WEBAPK, false);
        launchSettings(urlToLaunchSettingsFor, isWebApk);
        finish();
    }

    private void launchSettings(@Nullable String urlToLaunchSettingsFor, boolean isWebApk) {
        String packageName = getClientPackageName(isWebApk);
        if (packageName == null) {
            logNoPackageName();
            finish();
            return;
        }
        new TrustedWebActivityUmaRecorder(
                ChromeBrowserInitializer.getInstance()::runNowOrAfterFullBrowserStarted)
                .recordOpenedSettingsViaManageSpace();

        if (isWebApk) {
            TrustedWebActivitySettingsLauncher.launchForWebApkPackageName(
                    this, packageName, urlToLaunchSettingsFor);
        } else {
            TrustedWebActivitySettingsLauncher.launchForPackageName(this, packageName);
        }
    }

    @VisibleForTesting
    public static void setCallingPackageForTesting(String packageName) {
        sMockCallingPackage = packageName;
    }

    @Nullable
    private String getClientPackageName(boolean isWebApk) {
        if (isWebApk) {
            return sMockCallingPackage != null ? sMockCallingPackage : getCallingPackage();
        }

        CustomTabsSessionToken session =
                CustomTabsSessionToken.getSessionTokenFromIntent(getIntent());
        if (session == null) {
            return null;
        }

        CustomTabsConnection connection =
                ChromeApplicationImpl.getComponent().resolveCustomTabsConnection();
        return connection.getClientPackageNameForSession(session);
    }

    private void logNoPackageName() {
        Log.e(TAG, "Package name for incoming intent couldn't be resolved. "
                + "Was a CustomTabSession created and added to the intent?");
    }
}
