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
import android.media.MediaDrm;
import android.os.Build;
import android.util.Base64;
import androidx.media3.common.C;
import androidx.media3.exoplayer.drm.DefaultDrmSessionManager;
import androidx.media3.exoplayer.drm.ExoMediaDrm;
import androidx.media3.exoplayer.drm.ExoMediaDrm.KeyRequest;
import androidx.media3.exoplayer.drm.ExoMediaDrm.ProvisionRequest;
import androidx.media3.exoplayer.drm.FrameworkMediaDrm;
import androidx.media3.exoplayer.drm.MediaDrmCallback;
import androidx.media3.exoplayer.drm.MediaDrmCallbackException;
import androidx.media3.exoplayer.drm.UnsupportedDrmException;
import androidx.media3.exoplayer.source.DefaultMediaSourceFactory;
import androidx.media3.exoplayer.source.MediaSource;
import java.util.UUID;
import java.util.concurrent.CompletableFuture;
import java.util.concurrent.TimeUnit;
import dev.cobalt.util.Log;
import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

@JNINamespace("starboard")
public class ExoPlayerDrmBridge {
    private final long mNativeDrmSystemExoplayer;
    private final NativeMediaDrmCallback mMediaDrmCallback;
    private final DefaultDrmSessionManager mDrmSessionManager;
    private final MediaSource.Factory mMediaSourceFactory;
    private FrameworkMediaDrm mMediaDrm;
    private byte[] mSessionId;

    private final class NativeMediaDrmCallback implements MediaDrmCallback {
        private CompletableFuture<byte[]> pendingProvisionRequestResponse;
        private CompletableFuture<byte[]> pendingKeyRequestResponse;

        @Override
        public byte[] executeProvisionRequest(UUID uuid, ProvisionRequest request)
                throws MediaDrmCallbackException {
            Log.i(TAG, "Called executeProvisionRequest()");
            pendingProvisionRequestResponse = new CompletableFuture<>();
            ExoPlayerDrmBridgeJni.get().executeProvisionRequest(
                    mNativeDrmSystemExoplayer, request.getData(), mSessionId);

            try {
                return pendingProvisionRequestResponse.get(10, TimeUnit.SECONDS);
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
        }

        @Override
        public byte[] executeKeyRequest(UUID uuid, KeyRequest request)
                throws MediaDrmCallbackException {
            Log.i(TAG, "Called executeKeyRequest()");
            pendingKeyRequestResponse = new CompletableFuture<>();
            ExoPlayerDrmBridgeJni.get().executeKeyRequest(
                    mNativeDrmSystemExoplayer, request.getData(), mSessionId);

            try {
                return pendingKeyRequestResponse.get(10, TimeUnit.SECONDS);
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
        }

        public void setProvisionRequestResponse(byte[] response) {
            if (pendingProvisionRequestResponse != null) {
                pendingProvisionRequestResponse.complete(response);
            }
        }

        public void setKeyRequestResponse(byte[] response) {
            if (pendingKeyRequestResponse != null) {
                pendingKeyRequestResponse.complete(response);
            }
        }
    }

    private class MediaDrmProvider implements ExoMediaDrm.Provider {
        @Override
        public ExoMediaDrm acquireExoMediaDrm(UUID uuid) {
            try {
                FrameworkMediaDrm mediaDrm = FrameworkMediaDrm.newInstance(uuid);
                mediaDrm.setOnEventListener((exoMediaDrm, sessionId, event, extra, data) -> {
                    if (event == MediaDrm.EVENT_KEY_REQUIRED) {
                        Log.i(TAG, "SETTING SESSION ID");
                        mSessionId = sessionId;
                    }
                });
                mMediaDrm = mediaDrm;
                return mediaDrm;
            } catch (UnsupportedDrmException e) {
                Log.e(TAG,
                        String.format(
                                "Could not create mediaDrm instance, error: %s", e.toString()));
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
     * Passes a provision request response to the drm callback.
     * @param response Response data.
     */
    @CalledByNative
    void SetProvisionRequestResponse(byte[] response) {
        mMediaDrmCallback.setProvisionRequestResponse(response);
    }

    /**
     * Passes a key request response to the drm callback.
     * @param response Response data.
     */
    @CalledByNative
    void SetKeyRequestResponse(byte[] response) {
        mMediaDrmCallback.setKeyRequestResponse(response);
    }

    @NativeMethods
    interface Natives {
        void executeProvisionRequest(long nativeDrmSystemExoPlayer, byte[] data, byte[] sessionId);

        void executeKeyRequest(long nativeDrmSystemExoPlayer, byte[] data, byte[] sessionId);
    }
}
