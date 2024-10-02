// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;

import org.chromium.base.ContextUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.browserservices.intents.WebappInfo;
import org.chromium.components.webapk.lib.client.WebApkValidator;
import org.chromium.components.webapps.ShortcutSource;

/**
 * Delegate for retrieving WebApkInfo.
 */
public class WebApkHandlerDelegate {
    private long mNativePointer;

    @NativeMethods
    interface Natives {
        void onWebApkInfoRetrieved(long nativeWebApkHandlerDelegate, String name, String shortName,
                String packageName, String id, int shellApkVersion, int versionCode, String uri,
                String scope, String manifestUrl, String manifestStartUrl, String manifestId,
                int displayMode, int orientation, long themeColor, long backgroundColor,
                long lastUpdateCheckTimeMs, long lastUpdateCompletionTimeMs, boolean relaxUpdates,
                String backingBrowserPackageName, boolean isBackingBrowser, String updateStatus);
    }

    private WebApkHandlerDelegate(long nativePointer) {
        mNativePointer = nativePointer;
    }

    /**
     * Creates a new instance of a WebApkHandlerDelegate with a native WebApkHandlerDelegate at
     * |nativePointer|.
     */
    @CalledByNative
    public static WebApkHandlerDelegate create(long nativePointer) {
        return new WebApkHandlerDelegate(nativePointer);
    }

    /**
     * Invalidates the native pointer to WebApkHandlerDelegate.
     */
    @CalledByNative
    public void reset() {
        mNativePointer = 0;
    }

    /**
     * Calls the native WebApkHandlerDelegate with information for each installed WebAPK.
     */
    @CalledByNative
    @SuppressWarnings("QueryPermissionsNeeded")
    public void retrieveWebApks() {
        if (mNativePointer == 0) {
            return;
        }
        Context context = ContextUtils.getApplicationContext();
        PackageManager packageManager = context.getPackageManager();
        for (PackageInfo packageInfo : packageManager.getInstalledPackages(0)) {
            if (WebApkValidator.isValidWebApk(context, packageInfo.packageName)) {
                ChromeWebApkHost.checkChromeBacksWebApkAsync(
                        packageInfo.packageName, (backedByBrowser, backingBrowser) -> {
                            onGotBackingBrowser(packageInfo, backedByBrowser, backingBrowser);
                        });
            }
        }
    }

    private void onGotBackingBrowser(
            PackageInfo packageInfo, boolean isBackingBrowser, String backingBrowserPackageName) {
        if (mNativePointer == 0) {
            return;
        }
        // Pass non-null URL parameter so that {@link WebApkInfo#create()}
        // return value is non-null
        WebappInfo webApkInfo = WebappInfo.create(WebApkIntentDataProviderFactory.create(
                new Intent(), packageInfo.packageName, "", ShortcutSource.UNKNOWN,
                false /* forceNavigation */, false /* isSplashProvidedByWebApk */,
                null /* shareData */, null /* shareDataActivityClassName */));
        if (webApkInfo == null) {
            return;
        }

        WebappDataStorage storage =
                WebappRegistry.getInstance().getWebappDataStorage(webApkInfo.id());
        long lastUpdateCheckTimeMsForStorage = 0;
        long lastUpdateCompletionTimeMsInStorage = 0;
        boolean relaxUpdatesForStorage = false;
        String updateStatus = WebappDataStorage.NOT_UPDATABLE;
        if (storage != null) {
            lastUpdateCheckTimeMsForStorage = storage.getLastCheckForWebManifestUpdateTimeMs();
            lastUpdateCompletionTimeMsInStorage =
                    storage.getLastWebApkUpdateRequestCompletionTimeMs();
            relaxUpdatesForStorage = storage.shouldRelaxUpdates();
            updateStatus = storage.getUpdateStatus();
        }
        WebApkHandlerDelegateJni.get().onWebApkInfoRetrieved(mNativePointer, webApkInfo.name(),
                webApkInfo.shortName(), webApkInfo.webApkPackageName(), webApkInfo.id(),
                webApkInfo.shellApkVersion(), packageInfo.versionCode, webApkInfo.url(),
                webApkInfo.scopeUrl(), webApkInfo.manifestUrl(), webApkInfo.manifestStartUrl(),
                webApkInfo.manifestId(), webApkInfo.displayMode(), webApkInfo.orientation(),
                webApkInfo.toolbarColor(), webApkInfo.backgroundColor(),
                lastUpdateCheckTimeMsForStorage, lastUpdateCompletionTimeMsInStorage,
                relaxUpdatesForStorage, backingBrowserPackageName, isBackingBrowser, updateStatus);
    }
}
