// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.common;

import android.content.Context;
import android.os.Handler;
import android.os.HandlerThread;

import androidx.annotation.NonNull;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;

/**
 * This class manages platform-specific services. (i.e. Google Services) The platform
 * should extend this class and use this base class to fetch their specialized version.
 */
public abstract class PlatformServiceBridge {
    private static final String TAG = "PlatformServiceBrid-";

    private static PlatformServiceBridge sInstance;
    private static final Object sInstanceLock = new Object();

    private static HandlerThread sHandlerThread;
    private static Handler sHandler;
    private static final Object sHandlerLock = new Object();

    protected PlatformServiceBridge() {}

    @SuppressWarnings("unused")
    public static PlatformServiceBridge getInstance() {
        synchronized (sInstanceLock) {
            if (sInstance == null) {
                // Load an instance of PlatformServiceBridgeImpl. Because this can change
                // depending on the GN configuration, this may not be the PlatformServiceBridgeImpl
                // defined upstream.
                sInstance = new PlatformServiceBridgeImpl();
            }
            return sInstance;
        }
    }

    // Provide a mocked PlatformServiceBridge for testing.
    public static void injectInstance(PlatformServiceBridge testBridge) {
        // Although reference assignments are atomic, we still wouldn't want to assign it in the
        // middle of getInstance().
        synchronized (sInstanceLock) {
            sInstance = testBridge;
        }
    }

    // Return a handler appropriate for executing blocking Platform Service tasks.
    public static Handler getHandler() {
        synchronized (sHandlerLock) {
            if (sHandler == null) {
                sHandlerThread = new HandlerThread("PlatformServiceBridgeHandlerThread");
                sHandlerThread.start();
                sHandler = new Handler(sHandlerThread.getLooper());
            }
        }
        return sHandler;
    }

    // Can WebView use Google Play Services (a.k.a. GMS)?
    public boolean canUseGms() {
        return false;
    }

    // Overriding implementations may call "callback" asynchronously, on any thread.
    public void querySafeBrowsingUserConsent(@NonNull final Callback<Boolean> callback) {
        // User opt-in preference depends on a SafetyNet API. In purely upstream builds (which don't
        // communicate with GMS), assume the user has not opted in.
        callback.onResult(false);
    }

    // Overriding implementations may call "callback" asynchronously. For simplicity (and not
    // because of any technical limitation) we require that "queryMetricsSetting" and "callback"
    // both get called on WebView's UI thread.
    public void queryMetricsSetting(Callback<Boolean> callback) {
        ThreadUtils.assertOnUiThread();
        callback.onResult(false);
    }

    public void setSafeBrowsingHandler() {
        // We don't have this specialized service.
    }

    public void warmUpSafeBrowsing(Context context, @NonNull final Callback<Boolean> callback) {
        callback.onResult(false);
    }

    // Takes an uncompressed, serialized UMA proto and logs it via a platform-specific mechanism.
    public void logMetrics(byte[] data, boolean useDefaultUploadQos) {}

    /**
     * Similar to {@link logMetrics}, logs a serialized UMA proto via a platform-specific mechanism
     * but blocks until the operation finishes.
     *
     * @param data uncompressed, serialized UMA proto.
     * @param useDefaultUploadQos whether to use an experimental change that increases upload
     *         frequency
     * @return Status code of the logging operation. The status codes are:
     * - Success cache (went to the devices cache): -1
     * - Success: 0
     * - Internal error: 8
     * - Interrupted: 14
     * - Timeout: 15
     * - Cancelled: 16
     * - API not connected (probably means the API is not available on device): 17
     */
    public int logMetricsBlocking(byte[] data, boolean useDefaultUploadQos) {
        // TODO(crbug.com/1248039): remove this once downstream implementation lands.
        logMetrics(data, useDefaultUploadQos);
        return 0;
    }

    /**
     * Checks if app recovery mitigations are currently required and initializes SafeMode if needed.
     * This should only be called from the ":webview_service" process. All other processes should
     * query SafeModeController to receive mitigation steps.
     */
    public void checkForAppRecovery() {}
}
