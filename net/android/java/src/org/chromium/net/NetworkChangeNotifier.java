// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import android.content.Context;

import org.chromium.base.CalledByNative;
import org.chromium.base.JNINamespace;
import org.chromium.base.NativeClassQualifiedName;

/**
 * Triggers updates to the underlying network state in native Chrome.
 * By default, connectivity is assumed and changes must pushed from
 * the embedder via the forceConnectivityState function.
 * Embedders may choose to have this class auto-detect changes in
 * network connectivity by invoking the autoDetectConnectivityState
 * function.
 */
@JNINamespace("net")
public class NetworkChangeNotifier {
    private final Context mContext;
    private int mNativeChangeNotifier;
    private boolean mIsConnected;
    private NetworkChangeNotifierAutoDetect mAutoDetector;

    private static NetworkChangeNotifier sInstance;

    // Private constructor - instances are only created via the create factory
    // function, which is only called from native.
    private NetworkChangeNotifier(Context context, int nativeChangeNotifier) {
        mContext = context;
        mNativeChangeNotifier = nativeChangeNotifier;
        mIsConnected = true;
        sInstance = this;
    }

    /**
     * Enable auto detection of the current network state based on notifications
     * from the system. Note that passing true here requires the embedding app
     * have the platform ACCESS_NETWORK_STATE permission.
     *
     * @param shouldAutoDetect true if the NetworkChangeNotifier should listen
     *     for system changes in network connectivity.
     */
    public static void setAutoDetectConnectivityState(boolean shouldAutoDetect) {
        // We should only get a call to this after the native object is created and
        // hence the singleton initialised.
        assert sInstance != null;
        sInstance.setAutoDetectConnectivityStateInternal(shouldAutoDetect);
    }

    private void setAutoDetectConnectivityStateInternal(boolean shouldAutoDetect) {
        if (shouldAutoDetect) {
            if (mAutoDetector == null) {
                mAutoDetector = new NetworkChangeNotifierAutoDetect(this, mContext);
            }
        } else {
            if (mAutoDetector != null) {
                mAutoDetector.destroy();
                mAutoDetector = null;
            }
        }
    }

    /**
     * Update the perceived network state when not auto-detecting
     * changes to connectivity.
     *
     * @param networkAvailable True if the NetworkChangeNotifier should
     *     perceive a "connected" state, false implies "disconnected".
     */
    public static void forceConnectivityState(boolean networkAvailable) {
        assert sInstance != null;
        setAutoDetectConnectivityState(false);
        sInstance.forceConnectivityStateInternal(networkAvailable);
    }

    private void forceConnectivityStateInternal(boolean forceOnline) {
        if (mIsConnected != forceOnline) {
            mIsConnected = forceOnline;
            notifyNativeObservers();
        }
    }

    void notifyNativeObservers() {
        if (mNativeChangeNotifier != 0) {
            nativeNotifyObservers(mNativeChangeNotifier);
        }
    }

    @CalledByNative
    private boolean isConnected() {
        if (mAutoDetector != null) {
            return mAutoDetector.isConnected();
        }
        return mIsConnected;
    }

    @CalledByNative
    private void destroy() {
        if (mAutoDetector != null) {
            mAutoDetector.destroy();
        }
        mNativeChangeNotifier = 0;
        sInstance = null;
    }

    @CalledByNative
    private static NetworkChangeNotifier create(Context context, int nativeNetworkChangeNotifier) {
        return new NetworkChangeNotifier(context, nativeNetworkChangeNotifier);
    }

    @NativeClassQualifiedName("android::NetworkChangeNotifier")
    private native void nativeNotifyObservers(int nativePtr);

    // For testing only.
    public static NetworkChangeNotifierAutoDetect getAutoDetectorForTest() {
        return sInstance.mAutoDetector;
    }
}
