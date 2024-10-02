// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webapk.shell_apk;

import android.app.Activity;
import android.content.ActivityNotFoundException;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ResolveInfo;
import android.net.Uri;
import android.os.Bundle;
import android.text.TextUtils;
import android.util.Log;

import org.chromium.components.webapk.lib.common.WebApkMetaDataKeys;

import java.util.Map;

/** Selects host browser to launch, showing a dialog to select browser if necessary. */
public class LaunchHostBrowserSelector {
    private static final String LAST_RESORT_HOST_BROWSER = "com.android.chrome";
    private static final String LAST_RESORT_HOST_BROWSER_APPLICATION_NAME = "Google Chrome";
    private static final String TAG = "cr_LaunchHostBrowserSelector";

    private Context mContext;

    /** Parent activity for any dialogs. */
    private Activity mParentActivity;

    /**
     * Called once {@link #selectHostBrowser()} has selected the host browser either
     * via a shared preferences/<meta-data> lookup or via the user selecting the host
     * browser from a dialog.
     */
    public static interface Callback {
        void onBrowserSelected(String hostBrowserPackageName, boolean dialogShown);
    }

    public LaunchHostBrowserSelector(Activity parentActivity) {
        mParentActivity = parentActivity;
        mContext = parentActivity.getApplicationContext();
    }

    /**
     * Creates install Intent.
     * @param packageName Package to install.
     * @return The intent.
     */
    private static Intent createInstallIntent(String packageName) {
        String marketUrl = "market://details?id=" + packageName;
        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse(marketUrl));
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        return intent;
    }

    /**
     * Selects host browser to launch, showing a dialog to select browser if necessary. Calls
     * {@link selectCallback} with the result.
     */
    public void selectHostBrowser(Callback selectCallback) {
        Bundle metadata = WebApkUtils.readMetaData(mContext);
        if (metadata == null) {
            selectCallback.onBrowserSelected(null, false /* dialogShown */);
            return;
        }

        String packageName = mContext.getPackageName();
        Log.v(TAG, "Package name of the WebAPK:" + packageName);

        String runtimeHost =
                HostBrowserUtils.computeHostBrowserPackageClearCachedDataOnChange(mContext);
        if (!TextUtils.isEmpty(runtimeHost)) {
            selectCallback.onBrowserSelected(runtimeHost, false /* dialogShown */);
            return;
        }

        Map<String, ResolveInfo> infos =
                WebApkUtils.getInstalledBrowserResolveInfos(mContext.getPackageManager());
        if (!infos.isEmpty()) {
            showChooseHostBrowserDialog(infos, selectCallback);
        } else {
            showInstallHostBrowserDialog(metadata, selectCallback);
        }
    }

    /**
     * Launches the Play Store with the host browser's page.
     */
    private void installBrowser(String hostBrowserPackageName) {
        try {
            mParentActivity.startActivity(createInstallIntent(hostBrowserPackageName));
        } catch (ActivityNotFoundException e) {
        }
    }

    /** Shows a dialog to choose the host browser. */
    private void showChooseHostBrowserDialog(
            Map<String, ResolveInfo> infos, Callback selectCallback) {
        ChooseHostBrowserDialog.DialogListener listener =
                new ChooseHostBrowserDialog.DialogListener() {
                    @Override
                    public void onHostBrowserSelected(String selectedHostBrowser) {
                        HostBrowserUtils.writeHostBrowserToSharedPref(
                                mContext, selectedHostBrowser);
                        selectCallback.onBrowserSelected(
                                selectedHostBrowser, true /* dialogShown */);
                    }
                    @Override
                    public void onQuit() {
                        selectCallback.onBrowserSelected(null, true /* dialogShown */);
                    }
                };
        ChooseHostBrowserDialog.show(
                mParentActivity, listener, infos, mContext.getString(R.string.name));
    }

    /** Shows a dialog to install the host browser. */
    private void showInstallHostBrowserDialog(Bundle metadata, Callback selectCallback) {
        String lastResortHostBrowserPackageName =
                metadata.getString(WebApkMetaDataKeys.RUNTIME_HOST);
        String lastResortHostBrowserApplicationName =
                metadata.getString(WebApkMetaDataKeys.RUNTIME_HOST_APPLICATION_NAME);

        if (TextUtils.isEmpty(lastResortHostBrowserPackageName)) {
            // WebAPKs without runtime host specified in the AndroidManifest.xml always install
            // Google Chrome as the default host browser.
            lastResortHostBrowserPackageName = LAST_RESORT_HOST_BROWSER;
            lastResortHostBrowserApplicationName = LAST_RESORT_HOST_BROWSER_APPLICATION_NAME;
        }

        InstallHostBrowserDialog.DialogListener listener =
                new InstallHostBrowserDialog.DialogListener() {
                    @Override
                    public void onConfirmInstall(String packageName) {
                        installBrowser(packageName);
                        HostBrowserUtils.writeHostBrowserToSharedPref(mContext, packageName);
                        selectCallback.onBrowserSelected(null, true /* dialogShown */);
                    }
                    @Override
                    public void onConfirmQuit() {
                        selectCallback.onBrowserSelected(null, true /* dialogShown */);
                    }
                };

        InstallHostBrowserDialog.show(mParentActivity, listener, mContext.getString(R.string.name),
                lastResortHostBrowserPackageName, lastResortHostBrowserApplicationName,
                R.drawable.last_resort_runtime_host_logo);
    }
}
