// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.android.webview.chromium;

import android.os.Bundle;
import android.webkit.CookieManager;
import android.webkit.GeolocationPermissions;
import android.webkit.ServiceWorkerController;
import android.webkit.WebStorage;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.UiThread;

import org.chromium.android_webview.AwBrowserContext;
import org.chromium.android_webview.AwPrefetchCallback;
import org.chromium.android_webview.AwPrefetchParameters;
import org.chromium.android_webview.common.Lifetime;
import org.chromium.base.ThreadUtils;
import org.chromium.base.TraceEvent;

import java.util.concurrent.Executor;

/**
 * An abstraction of {@link AwBrowserContext}, this class reflects the state needed for the
 * multi-profile public API.
 */
@Lifetime.Profile
public class Profile {

    @NonNull private final AwBrowserContext mBrowserContext;

    @NonNull private final String mName;

    @NonNull private final CookieManager mCookieManager;

    @NonNull private final WebStorage mWebStorage;

    @NonNull private final GeolocationPermissions mGeolocationPermissions;

    @NonNull private final ServiceWorkerController mServiceWorkerController;

    public Profile(@NonNull final AwBrowserContext browserContext) {
        assert ThreadUtils.runningOnUiThread();
        WebViewChromiumFactoryProvider factory = WebViewChromiumFactoryProvider.getSingleton();
        mBrowserContext = browserContext;
        mName = browserContext.getName();

        if (browserContext.isDefaultAwBrowserContext()) {
            mCookieManager = factory.getCookieManager();
            mWebStorage = factory.getWebStorage();
            mGeolocationPermissions = factory.getGeolocationPermissions();
            mServiceWorkerController = factory.getServiceWorkerController();
        } else {
            mCookieManager = new CookieManagerAdapter(browserContext.getCookieManager());
            mWebStorage = new WebStorageAdapter(factory, browserContext.getQuotaManagerBridge());
            mGeolocationPermissions =
                    new GeolocationPermissionsAdapter(
                            factory, browserContext.getGeolocationPermissions());
            mServiceWorkerController =
                    new ServiceWorkerControllerAdapter(browserContext.getServiceWorkerController());
        }
    }

    @NonNull
    public String getName() {
        return mName;
    }

    @NonNull
    public CookieManager getCookieManager() {
        return mCookieManager;
    }

    @NonNull
    public WebStorage getWebStorage() {
        return mWebStorage;
    }

    @NonNull
    public GeolocationPermissions getGeolocationPermissions() {
        return mGeolocationPermissions;
    }

    @NonNull
    public ServiceWorkerController getServiceWorkerController() {
        return mServiceWorkerController;
    }

    @UiThread
    public void prefetchUrl(
            String url,
            @Nullable PrefetchParams params,
            Executor callbackExecutor,
            PrefetchOperationCallback resultCallback) {
        try (TraceEvent event = TraceEvent.scoped("WebView.Profile.Prefetch.PRE_START")) {
            if (url == null) {
                throw new IllegalArgumentException("URL cannot be null for prefetch.");
            }

            if (resultCallback == null) {
                throw new IllegalArgumentException("Callback cannot be null for prefetch.");
            }

            AwPrefetchParameters awPrefetchParameters =
                    params == null ? null : params.toAwPrefetchParams();
            AwPrefetchCallback awCallback =
                    new AwPrefetchCallback() {
                        @Override
                        public void onStatusUpdated(
                                @StatusCode int statusCode, @Nullable Bundle extras) {
                            PrefetchOperationResult operationResult =
                                    PrefetchOperationResult.fromPrefetchStatusCode(
                                            statusCode, extras);
                            if (operationResult != null) {
                                switch (operationResult.statusCode) {
                                    case PrefetchOperationStatusCode.SUCCESS:
                                        resultCallback.onSuccess();
                                        break;
                                    case PrefetchOperationStatusCode.FAILURE:
                                        resultCallback.onError(
                                                new PrefetchException(
                                                        "Prefetch Request has failed"));
                                        break;
                                    case PrefetchOperationStatusCode.SERVER_FAILURE:
                                        resultCallback.onError(
                                                new PrefetchNetworkException(
                                                        "Server error",
                                                        operationResult.httpResponseStatusCode));
                                        break;
                                    default:
                                        resultCallback.onError(
                                                new PrefetchException(
                                                        "Unexpected error occurred."));
                                }
                            }
                        }

                        @Override
                        public void onError(Throwable e) {
                            // TODO: Correct this based on the type of error.
                            resultCallback.onError(new PrefetchException(e));
                        }
                    };

            mBrowserContext
                    .getPrefetchManager()
                    .startPrefetchRequest(url, awPrefetchParameters, awCallback, callbackExecutor);
        }
    }

    @UiThread
    public void clearPrefetch(String url, PrefetchOperationCallback resultCallback) {
        // TODO(334016945): do the actual implementation
    }

    @UiThread
    public void cancelPrefetch(String url) {
        // TODO(334016945): do the actual implementation
    }

    @UiThread
    public void setSpeculativeLoadingConfig(SpeculativeLoadingConfig speculativeLoadingConfig) {
        mBrowserContext
                .getPrefetchManager()
                .updatePrefetchConfiguration(
                        speculativeLoadingConfig.prefetchTTLSeconds,
                        speculativeLoadingConfig.maxPrefetches);
        if (speculativeLoadingConfig.maxPrerenders > 0) {
            mBrowserContext.setMaxPrerenders(speculativeLoadingConfig.maxPrerenders);
        }
    }
}
