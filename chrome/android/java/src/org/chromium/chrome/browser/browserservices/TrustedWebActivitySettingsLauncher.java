// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices;

import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.os.Bundle;

import org.chromium.base.Log;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.settings.SettingsLauncherImpl;
import org.chromium.chrome.browser.webapps.ChromeWebApkHost;
import org.chromium.components.browser_ui.settings.SettingsLauncher;
import org.chromium.components.browser_ui.site_settings.AllSiteSettings;
import org.chromium.components.browser_ui.site_settings.SettingsNavigationSource;
import org.chromium.components.browser_ui.site_settings.SingleWebsiteSettings;
import org.chromium.components.browser_ui.site_settings.SiteSettingsCategory;
import org.chromium.components.webapk.lib.client.WebApkValidator;

import java.util.ArrayList;
import java.util.Collection;

/**
 * Helper functions for launching site-settings for websites associated with a Trusted Web Activity.
 */
public class TrustedWebActivitySettingsLauncher {
    private static final String TAG = "TwaSettingsLauncher";

    /**
     * Launches site-settings for a Trusted Web Activity app with a given package name. If the
     * app has multiple origins associated with it, the user will see a list of origins and will be
     * able to work with each of them.
     */
    public static void launchForPackageName(Context context, String packageName) {
        Integer applicationUid = getApplicationUid(context, packageName);
        if (applicationUid == null) return;

        InstalledWebappDataRegister register = new InstalledWebappDataRegister();
        Collection<String> domains = register.getDomainsForRegisteredUid(applicationUid);
        Collection<String> origins = register.getOriginsForRegisteredUid(applicationUid);
        if (domains.isEmpty() || origins.isEmpty()) {
            Log.d(TAG, "Package " + packageName + " is not associated with any origins");
            return;
        }
        launch(context, origins, domains);
    }

    /**
     * Launches site-settings for a WebApk with a given package name and associated url.
     */
    public static void launchForWebApkPackageName(
            Context context, String packageName, String webApkUrl) {
        // Handle the case when settings are selected but Chrome was not running.
        ChromeWebApkHost.init();
        if (!WebApkValidator.canWebApkHandleUrl(context, packageName, webApkUrl)) {
            Log.d(TAG, "WebApk " + packageName + " can't handle url " + webApkUrl);
            return;
        }
        if (getApplicationUid(context, packageName) == null) return;

        openSingleWebsitePrefs(context, webApkUrl);
    }

    private static Integer getApplicationUid(Context context, String packageName) {
        int applicationUid;
        try {
            applicationUid = context.getPackageManager().getApplicationInfo(packageName, 0).uid;
        } catch (PackageManager.NameNotFoundException e) {
            Log.d(TAG, "Package " + packageName + " not found");
            return null;
        }
        return applicationUid;
    }

    /**
     * Same as above, but with list of associated origins and domains already retrieved.
     */
    public static void launch(Context context, Collection<String> origins,
            Collection<String> domains) {
        if (origins.size() == 1) {
            // When launched with EXTRA_SITE_ADDRESS, SingleWebsiteSettings will merge the
            // settings for top-level origin, so that given https://peconn.github.io and
            // peconn.github.io, we'll get the permission and data settings of both.
            openSingleWebsitePrefs(context, origins.iterator().next());
        } else {
            // Since there might be multiple entries per origin in the "All sites" screen,
            // such as https://peconn.github.io and peconn.github.io, we filter by domains
            // instead of origins.
            openFilteredAllSiteSettings(context, domains);
        }
    }

    private static void openSingleWebsitePrefs(Context context, String origin) {
        context.startActivity(createIntentForSingleWebsitePreferences(
                context, origin, SettingsNavigationSource.TWA_CLEAR_DATA_DIALOG));
    }

    private static void openFilteredAllSiteSettings(Context context, Collection<String> domains) {
        Bundle extras = new Bundle();
        extras.putString(AllSiteSettings.EXTRA_CATEGORY,
                SiteSettingsCategory.preferenceKey(SiteSettingsCategory.Type.ALL_SITES));
        extras.putString(AllSiteSettings.EXTRA_TITLE,
                context.getString(R.string.twa_clear_data_site_selection_title));
        extras.putStringArrayList(AllSiteSettings.EXTRA_SELECTED_DOMAINS, new ArrayList<>(domains));
        extras.putInt(SettingsNavigationSource.EXTRA_KEY,
                SettingsNavigationSource.TWA_CLEAR_DATA_DIALOG);

        SettingsLauncher settingsLauncher = new SettingsLauncherImpl();
        settingsLauncher.launchSettingsActivity(context, AllSiteSettings.class, extras);
    }

    /**
     * Creates an intent to launch single website preferences for the specified {@param url}.
     */
    private static Intent createIntentForSingleWebsitePreferences(
            Context context, String url, @SettingsNavigationSource int navigationSource) {
        Bundle args = SingleWebsiteSettings.createFragmentArgsForSite(url);
        args.putInt(SettingsNavigationSource.EXTRA_KEY, navigationSource);
        SettingsLauncher settingsLauncher = new SettingsLauncherImpl();
        return settingsLauncher.createSettingsActivityIntent(
                context, SingleWebsiteSettings.class.getName(), args);
    }
}
