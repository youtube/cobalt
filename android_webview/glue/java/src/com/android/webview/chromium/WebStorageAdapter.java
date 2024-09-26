// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.android.webview.chromium;

import android.webkit.ValueCallback;
import android.webkit.WebStorage;

import com.android.webview.chromium.WebViewChromium.ApiCall;

import org.chromium.android_webview.AwQuotaManagerBridge;
import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;

import java.util.HashMap;
import java.util.Map;

/**
 * Chromium implementation of WebStorage -- forwards calls to the
 * chromium internal implementation.
 */
@SuppressWarnings("deprecation")
final class WebStorageAdapter extends WebStorage {
    private final WebViewChromiumFactoryProvider mFactory;
    private final AwQuotaManagerBridge mQuotaManagerBridge;

    WebStorageAdapter(
            WebViewChromiumFactoryProvider factory, AwQuotaManagerBridge quotaManagerBridge) {
        mFactory = factory;
        mQuotaManagerBridge = quotaManagerBridge;
    }

    @Override
    public void getOrigins(final ValueCallback<Map> callback) {
        final Callback<AwQuotaManagerBridge.Origins> awOriginsCallback =
                new Callback<AwQuotaManagerBridge.Origins>() {
                    @Override
                    public void onResult(AwQuotaManagerBridge.Origins origins) {
                        Map<String, Origin> originsMap = new HashMap<String, Origin>();
                        for (int i = 0; i < origins.mOrigins.length; ++i) {
                            Origin origin = new Origin(
                                    origins.mOrigins[i], origins.mQuotas[i], origins.mUsages[i]) {
                                // Intentionally empty to work around cross-package protected
                                // visibility
                                // of Origin constructor.
                            };
                            originsMap.put(origins.mOrigins[i], origin);
                        }
                        callback.onReceiveValue(originsMap);
                    }
                };
        if (checkNeedsPost()) {
            mFactory.addTask(new Runnable() {
                @Override
                public void run() {
                    WebViewChromium.recordWebViewApiCall(ApiCall.WEB_STORAGE_GET_ORIGINS);
                    mQuotaManagerBridge.getOrigins(awOriginsCallback);
                }

            });
            return;
        }
        WebViewChromium.recordWebViewApiCall(ApiCall.WEB_STORAGE_GET_ORIGINS);
        mQuotaManagerBridge.getOrigins(awOriginsCallback);
    }

    @Override
    public void getUsageForOrigin(final String origin, final ValueCallback<Long> callback) {
        if (checkNeedsPost()) {
            mFactory.addTask(new Runnable() {
                @Override
                public void run() {
                    WebViewChromium.recordWebViewApiCall(ApiCall.WEB_STORAGE_GET_USAGE_FOR_ORIGIN);
                    mQuotaManagerBridge.getUsageForOrigin(
                            origin, CallbackConverter.fromValueCallback(callback));
                }

            });
            return;
        }
        WebViewChromium.recordWebViewApiCall(ApiCall.WEB_STORAGE_GET_USAGE_FOR_ORIGIN);
        mQuotaManagerBridge.getUsageForOrigin(
                origin, CallbackConverter.fromValueCallback(callback));
    }

    @Override
    public void getQuotaForOrigin(final String origin, final ValueCallback<Long> callback) {
        if (checkNeedsPost()) {
            mFactory.addTask(new Runnable() {
                @Override
                public void run() {
                    WebViewChromium.recordWebViewApiCall(ApiCall.WEB_STORAGE_GET_QUOTA_FOR_ORIGIN);
                    mQuotaManagerBridge.getQuotaForOrigin(
                            origin, CallbackConverter.fromValueCallback(callback));
                }

            });
            return;
        }
        WebViewChromium.recordWebViewApiCall(ApiCall.WEB_STORAGE_GET_QUOTA_FOR_ORIGIN);
        mQuotaManagerBridge.getQuotaForOrigin(
                origin, CallbackConverter.fromValueCallback(callback));
    }

    @Override
    public void setQuotaForOrigin(String origin, long quota) {
        // Intentional no-op for deprecated method.
    }

    @Override
    public void deleteOrigin(final String origin) {
        if (checkNeedsPost()) {
            mFactory.addTask(new Runnable() {
                @Override
                public void run() {
                    WebViewChromium.recordWebViewApiCall(ApiCall.WEB_STORAGE_DELETE_ORIGIN);
                    mQuotaManagerBridge.deleteOrigin(origin);
                }

            });
            return;
        }
        WebViewChromium.recordWebViewApiCall(ApiCall.WEB_STORAGE_DELETE_ORIGIN);
        mQuotaManagerBridge.deleteOrigin(origin);
    }

    @Override
    public void deleteAllData() {
        if (checkNeedsPost()) {
            mFactory.addTask(new Runnable() {
                @Override
                public void run() {
                    WebViewChromium.recordWebViewApiCall(ApiCall.WEB_STORAGE_DELETE_ALL_DATA);
                    mQuotaManagerBridge.deleteAllData();
                }

            });
            return;
        }
        WebViewChromium.recordWebViewApiCall(ApiCall.WEB_STORAGE_DELETE_ALL_DATA);
        mQuotaManagerBridge.deleteAllData();
    }

    private static boolean checkNeedsPost() {
        // Init is guaranteed to have happened if a WebStorageAdapter is created, so do not
        // need to check WebViewChromiumFactoryProvider.hasStarted.
        return !ThreadUtils.runningOnUiThread();
    }
}
