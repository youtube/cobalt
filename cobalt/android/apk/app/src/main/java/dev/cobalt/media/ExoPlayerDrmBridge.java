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
import android.os.Build;
import android.util.Base64;
import androidx.media3.exoplayer.drm.DefaultDrmSessionManager;
import androidx.media3.exoplayer.drm.ExoMediaDrm;
import androidx.media3.exoplayer.drm.MediaDrmCallback;
import androidx.media3.exoplayer.drm.MediaDrmCallbackException;
import androidx.media3.exoplayer.drm.UnsupportedDrmException;
import androidx.media3.exoplayer.source.DefaultMediaSourceFactory;
import androidx.media3.exoplayer.source.MediaSource;
import dev.cobalt.util.Log;
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
 * <p>This class implements {@link MediaDrmCallback} to handle key and provisioning requests,
 * forwarding them to the native Starboard implementation. It also owns and manages the lifecycle
 * of the {@link DefaultDrmSessionManager} used by ExoPlayer.
 */
@JNINamespace("starboard")
public class ExoPlayerDrmBridge {
    private final long mNativeDrmSystemExoplayer;
    private final NativeMediaDrmCallback mMediaDrmCallback;
    private final DefaultDrmSessionManager mDrmSessionManager;
    private final MediaSource.Factory mMediaSourceFactory;
    private ExoMediaDrmWrapper mMediaDrm;
    private byte[] mSessionId;

    // Arbitrarily chosen durations to wait for a response from the license server.
    private static final long PROVISION_REQUEST_TIMEOUT_MS = 2000;
    private static final long KEY_REQUEST_TIMEOUT_MS = 2000;

    private static final UUID WIDEVINE_UUID =
            UUID.fromString("edef8ba9-79d6-4ace-a3c8-27dcd51d21ed");

    private final class NativeMediaDrmCallback implements MediaDrmCallback {
        private CompletableFuture<byte[]> mPendingProvisionRequestResponse;
        private CompletableFuture<byte[]> mPendingKeyRequestResponse;

        /**
         * Executes a provisioning request.
         *
         * @param uuid The UUID of the content protection scheme.
         * @param request The provisioning request.
         * @return The response data.
         * @throws MediaDrmCallbackException If an error occurs.
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
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
        }

        /**
         * Executes a key request.
         *
         * @param uuid The UUID of the content protection scheme.
         * @param request The key request.
         * @return The response data.
         * @throws MediaDrmCallbackException If an error occurs.
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
            } catch (Exception e) {
                throw new RuntimeException(e);
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
        @Override
        public ExoMediaDrm acquireExoMediaDrm(UUID uuid) {
            try {
                mMediaDrm = new ExoMediaDrmWrapper(uuid);
                mMediaDrm.setCobaltOnKeyStatusChangeListener(
                        (mediaDrm, sessionId, keyInformation, hasNewUsableKey) -> {
                            ExoPlayerDrmBridgeJni.get().onKeyStatusChanged(
                                    mNativeDrmSystemExoplayer, sessionId,
                                    keyInformation.stream()
                                            .map(keyStatus
                                                    -> new ExoPlayerKeyStatus(keyStatus.getKeyId(),
                                                            keyStatus.getStatusCode()))
                                            .toArray(ExoPlayerKeyStatus[] ::new));
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
                        .setUuidAndExoMediaDrmProvider(WIDEVINE_UUID, new MediaDrmProvider())
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
