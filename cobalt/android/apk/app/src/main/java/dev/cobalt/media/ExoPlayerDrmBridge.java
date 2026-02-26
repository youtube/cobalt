// Copyright 2026 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

package dev.cobalt.media;

import static dev.cobalt.media.Log.TAG;

import android.content.Context;
import android.content.Context;
import android.net.Uri;
import android.os.Build;
import android.util.Base64;
import androidx.media3.common.C;
import androidx.media3.datasource.DataSpec;
import androidx.media3.exoplayer.drm.DefaultDrmSessionManager;
import androidx.media3.exoplayer.drm.ExoMediaDrm;
import androidx.media3.exoplayer.drm.FrameworkMediaDrm;
import androidx.media3.exoplayer.drm.MediaDrmCallback;
import androidx.media3.exoplayer.drm.MediaDrmCallbackException;
import androidx.media3.exoplayer.drm.UnsupportedDrmException;
import androidx.media3.exoplayer.source.DefaultMediaSourceFactory;
import androidx.media3.exoplayer.source.MediaSource;
import dev.cobalt.util.Log;
import java.lang.reflect.InvocationHandler;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.lang.reflect.Proxy;
import java.util.Collections;
import java.util.UUID;
import java.util.concurrent.CompletableFuture;
import java.util.concurrent.TimeUnit;
import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

/**
 * Manages DRM sessions for ExoPlayer and acts as a bridge between ExoPlayer's MediaDrmCallback
 * and the native SbDrmSystem.
 *
 * This class implements {@link MediaDrmCallback} to handle key and provisioning requests,
 * forwarding them to the native Starboard implementation. It also owns and manages the lifecycle
 * of the {@link DefaultDrmSessionManager} used by ExoPlayer.
 */
@JNINamespace("starboard")
public class ExoPlayerDrmBridge {
    private final long mNativeDrmSystemExoplayer;
    private final NativeMediaDrmCallback mMediaDrmCallback;
    private final DefaultDrmSessionManager mDrmSessionManager;
    private final MediaSource.Factory mMediaSourceFactory;
    private CobaltExoMediaDrm mMediaDrm;
    private byte[] mSessionId;

    // Arbitrarily chosen durations to wait for a response from the license server.
    private static final long PROVISION_REQUEST_TIMEOUT_MS = 2000;
    private static final long KEY_REQUEST_TIMEOUT_MS = 2000;

    /**
     * Interface to allow the proxy to expose internal state and custom listeners.
     * This extends {@link ExoMediaDrm} so the resulting proxy can be used by ExoPlayer
     * while also providing Cobalt-specific extension methods.
     */
    private interface CobaltExoMediaDrm extends ExoMediaDrm {
        /** Returns the session ID captured during the last openSession() call. */
        byte[] getSessionId();
        /** Sets a second listener for key status changes, used by the native layer. */
        void setCobaltOnKeyStatusChangeListener(OnKeyStatusChangeListener listener);
    }

    /**
     * A Dynamic Proxy handler that delegates all calls to a {@link FrameworkMediaDrm} instance
     * while intercepting specific methods to manage session state and event multiplexing.
     *
     * This pattern is used to avoid creating a full decorator (wrapper) class for ExoMediaDrm,
     * which would require implementing dozens of boilerplate methods and would be fragile
     * to upstream API changes in the media3 library.
     */
    private static class ExoMediaDrmInvocationHandler implements InvocationHandler {
        private final FrameworkMediaDrm mRealDrm;
        private byte[] mSessionId;
        private ExoMediaDrm.OnKeyStatusChangeListener mExoListener;
        private ExoMediaDrm.OnKeyStatusChangeListener mCobaltListener;

        public ExoMediaDrmInvocationHandler(FrameworkMediaDrm realDrm) {
            mRealDrm = realDrm;
        }

        @Override
        public Object invoke(Object proxy, Method method, Object[] args) throws Throwable {
            String methodName = method.getName();
            try {
                // Intercept openSession to capture the sessionId. This allows the bridge
                // to provide the sessionId to the native layer during asynchronous callbacks
                // (like OnKeyRequest) where ExoPlayer doesn't explicitly pass it.
                if (methodName.equals("openSession")) {
                    mSessionId = (byte[]) method.invoke(mRealDrm, args);
                    return mSessionId;
                }

                // Extension method defined in CobaltExoMediaDrm interface.
                else if (methodName.equals("getSessionId")) {
                    return mSessionId;
                }

                // Intercept ExoPlayer's listener registration so we can multiplex it
                // with our own native status listener.
                else if (methodName.equals("setOnKeyStatusChangeListener")) {
                    mExoListener = (ExoMediaDrm.OnKeyStatusChangeListener) args[0];
                    updateOnKeyStatusChangeListener(proxy);
                    return null;
                }

                // Extension method defined in CobaltExoMediaDrm interface.
                else if (methodName.equals("setCobaltOnKeyStatusChangeListener")) {
                    mCobaltListener = (ExoMediaDrm.OnKeyStatusChangeListener) args[0];
                    updateOnKeyStatusChangeListener(proxy);
                    return null;
                }

                // Default: delegate all other calls (e.g., queryKeyStatus, closeSession)
                // directly to the underlying FrameworkMediaDrm.
                return method.invoke(mRealDrm, args);
            } catch (InvocationTargetException e) {
                // Unpack the real exception from the reflection wrapper to ensure
                // ExoPlayer's error handling sees the original exception.
                throw e.getCause();
            }
        }

        /**
         * Registers a single multiplexing listener on the real {@link FrameworkMediaDrm}
         * that forwards events to both the ExoPlayer internal listener and the Cobalt
         * native listener if they are present.
         */
        private void updateOnKeyStatusChangeListener(Object proxy) {
            if (mExoListener == null && mCobaltListener == null) {
                mRealDrm.setOnKeyStatusChangeListener(null);
                return;
            }
            mRealDrm.setOnKeyStatusChangeListener(
                    (mediaDrm, sessionId, keyInformation, hasNewUsableKey) -> {
                        if (mExoListener != null) {
                            mExoListener.onKeyStatusChange(
                                    (ExoMediaDrm) proxy, sessionId, keyInformation, hasNewUsableKey);
                        }
                        if (mCobaltListener != null) {
                            mCobaltListener.onKeyStatusChange(
                                    (ExoMediaDrm) proxy, sessionId, keyInformation, hasNewUsableKey);
                        }
                    });
        }
    }

    private final class NativeMediaDrmCallback implements MediaDrmCallback {
        private volatile CompletableFuture<byte[]> mPendingProvisionRequestResponse;
        private volatile CompletableFuture<byte[]> mPendingKeyRequestResponse;

    /**
     * Executes a provisioning request by forwarding it to the native Starboard layer.
     *
     * This method blocks until a response is received from the native layer or the timeout
     * is reached.
     */
    @Override
    public byte[] executeProvisionRequest(UUID uuid, androidx.media3.exoplayer.drm.ExoMediaDrm.ProvisionRequest request)
            throws MediaDrmCallbackException {
        Log.i(TAG, "Called onProvisionRequest()");

        if (mMediaDrm != null) {
            mSessionId = mMediaDrm.getSessionId();
        }

        mPendingProvisionRequestResponse = new CompletableFuture<>();
        ExoPlayerDrmBridgeJni.get().onProvisionRequest(
                mNativeDrmSystemExoplayer, request.getData(), mSessionId);

        try {
            return mPendingProvisionRequestResponse.get(
                    PROVISION_REQUEST_TIMEOUT_MS, TimeUnit.MILLISECONDS);
        } catch (InterruptedException e) {
            Thread.currentThread().interrupt();
            throw new MediaDrmCallbackException(
                    new DataSpec(Uri.EMPTY), Uri.EMPTY, Collections.emptyMap(), 0, e);
        } catch (Exception e) {
            throw new MediaDrmCallbackException(
                    new DataSpec(Uri.EMPTY), Uri.EMPTY, Collections.emptyMap(), 0, e);
        }
    }

    /**
     * Executes a key request by forwarding it to the native Starboard layer.
     *
     * This method blocks until a response is received from the native layer or the timeout
     * is reached.
     */
    @Override
    public byte[] executeKeyRequest(UUID uuid, androidx.media3.exoplayer.drm.ExoMediaDrm.KeyRequest request)
            throws MediaDrmCallbackException {
        Log.i(TAG, "Called onKeyRequest()");

        if (mMediaDrm != null) {
            mSessionId = mMediaDrm.getSessionId();
        }

        if (mSessionId == null) {
            Log.e(TAG, "Key request fired but mSessionId is still null!");
        }

        mPendingKeyRequestResponse = new CompletableFuture<>();
        ExoPlayerDrmBridgeJni.get().onKeyRequest(mNativeDrmSystemExoplayer,
                request.getRequestType(), request.getData(), mSessionId);

        try {
            return mPendingKeyRequestResponse.get(
                    KEY_REQUEST_TIMEOUT_MS, TimeUnit.MILLISECONDS);
        } catch (InterruptedException e) {
            Thread.currentThread().interrupt();
            throw new MediaDrmCallbackException(
                    new DataSpec(Uri.EMPTY), Uri.EMPTY, Collections.emptyMap(), 0, e);
        } catch (Exception e) {
            throw new MediaDrmCallbackException(
                    new DataSpec(Uri.EMPTY), Uri.EMPTY, Collections.emptyMap(), 0, e);
        }
    }

        public void setProvisionRequestResponse(byte[] response) {
            if (mPendingProvisionRequestResponse == null) {
                Log.e(TAG, "Received NULL provision request response.");
                return;
            }
            mPendingProvisionRequestResponse.complete(response);
        }

        public void setKeyRequestResponse(byte[] response) {
            if (mPendingKeyRequestResponse == null) {
                Log.e(TAG, "Received NULL key request response.");
                return;
            }
            mPendingKeyRequestResponse.complete(response);
        }
    }

    /** A wrapper of the android MediaDrm.KeyStatus class to be used by JNI. */
    public static class ExoPlayerKeyStatus {
        private final byte[] mKeyId;
        private final int mStatusCode;

        public ExoPlayerKeyStatus(byte[] keyId, int statusCode) {
            mKeyId = (keyId == null) ? null : keyId.clone();
            mStatusCode = statusCode;
        }

        @CalledByNative("ExoPlayerKeyStatus")
        private byte[] getKeyId() {
            return (mKeyId == null) ? null : mKeyId.clone();
        }

        @CalledByNative("ExoPlayerKeyStatus")
        private int getStatusCode() {
            return mStatusCode;
        }
    }

    private class MediaDrmProvider implements ExoMediaDrm.Provider {
        /**
         * Acquires an {@link ExoMediaDrm} instance wrapped in a Dynamic Proxy.
         *
         * The proxy intercepts session lifecycle events (like openSession) to track the active
         * session ID and multiplexes key status changes to both ExoPlayer's internal listeners
         * and the native Starboard layer.
         */
        @Override
        public ExoMediaDrm acquireExoMediaDrm(UUID uuid) {
            try {
                FrameworkMediaDrm realDrm = FrameworkMediaDrm.newInstance(uuid);
                ExoMediaDrmInvocationHandler handler = new ExoMediaDrmInvocationHandler(realDrm);
                mMediaDrm = (CobaltExoMediaDrm) Proxy.newProxyInstance(
                        ExoPlayerDrmBridge.class.getClassLoader(),
                        new Class<?>[] {CobaltExoMediaDrm.class},
                        handler);

                mMediaDrm.setCobaltOnKeyStatusChangeListener(
                        (mediaDrm, sessionId, keyInformation, hasNewUsableKey) -> {
                            ExoPlayerKeyStatus[] keyStatuses =
                                    new ExoPlayerKeyStatus[keyInformation.size()];
                            for (int i = 0; i < keyInformation.size(); i++) {
                                ExoMediaDrm.KeyStatus keyStatus = keyInformation.get(i);
                                keyStatuses[i] = new ExoPlayerKeyStatus(
                                        keyStatus.getKeyId(), keyStatus.getStatusCode());
                            }

                            ExoPlayerDrmBridgeJni.get().onKeyStatusChanged(
                                    mNativeDrmSystemExoplayer, sessionId, keyStatuses);
                        });
                return mMediaDrm;
            } catch (UnsupportedDrmException e) {
                Log.e(TAG, String.format("Could not create MediaDrm instance, error: %s", e));
                throw new RuntimeException("Failed to instantiate MediaDrm", e);
            }
        }
    }

    public ExoPlayerDrmBridge(Context context, long nativeDrmSystem) {
        mNativeDrmSystemExoplayer = nativeDrmSystem;
        mMediaDrmCallback = new NativeMediaDrmCallback();
        mDrmSessionManager =
                new DefaultDrmSessionManager.Builder()
                        .setUuidAndExoMediaDrmProvider(C.WIDEVINE_UUID, new MediaDrmProvider())
                        .build(mMediaDrmCallback);
        mMediaSourceFactory = new DefaultMediaSourceFactory(context).setDrmSessionManagerProvider(
                mediaItem -> mDrmSessionManager);
    }

    public MediaSource.Factory getMediaSourceFactory() {
        return mMediaSourceFactory;
    }

    @CalledByNative
    public DefaultDrmSessionManager getDrmSessionManager() {
        return mDrmSessionManager;
    }

    @CalledByNative
    public void release() {
        if (mDrmSessionManager != null) {
            mDrmSessionManager.release();
        }
    }

    @CalledByNative
    byte[] getMetricsInBase64() {
        if (Build.VERSION.SDK_INT < 28) {
            return null;
        }
        byte[] metrics;
        try {
            metrics = mMediaDrm.getPropertyByteArray("metrics");
        } catch (Exception e) {
            Log.e(TAG, "Failed to retrieve DRM Metrics.");
            return null;
        }
        return Base64.encode(metrics, Base64.NO_PADDING | Base64.NO_WRAP | Base64.URL_SAFE);
    }

    /**
     * Passes a key request response to the drm callback.
     * @param response Response data.
     */
    @CalledByNative
    void setKeyRequestResponse(byte[] response) {
        mMediaDrmCallback.setKeyRequestResponse(response);
    }

    @NativeMethods
    interface Natives {
        void onProvisionRequest(long nativeDrmSystemExoPlayer, byte[] data, byte[] sessionId);

        void onKeyRequest(
                long nativeDrmSystemExoPlayer, int requestType, byte[] data, byte[] sessionId);

        void onKeyStatusChanged(
                long nativeDrmSystemExoPlayer, byte[] sessionId, ExoPlayerKeyStatus[] keyInformation);
    }
}
