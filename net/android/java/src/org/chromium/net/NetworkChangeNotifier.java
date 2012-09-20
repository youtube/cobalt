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
    // These constants must always match the ones in network_change_notifier.h.
    public static final int CONNECTION_UNKNOWN = 0;
    public static final int CONNECTION_ETHERNET = 1;
    public static final int CONNECTION_WIFI = 2;
    public static final int CONNECTION_2G = 3;
    public static final int CONNECTION_3G = 4;
    public static final int CONNECTION_4G = 5;
    public static final int CONNECTION_NONE = 6;

    private final Context mContext;
    private int mNativeChangeNotifier;
    private NetworkChangeNotifierAutoDetect mAutoDetector;

    private static NetworkChangeNotifier sInstance;

    // Private constructor - instances are only created via the create factory
    // function, which is only called from native.
    private NetworkChangeNotifier(Context context, int nativeChangeNotifier) {
        mContext = context;
        mNativeChangeNotifier = nativeChangeNotifier;
    }

    private void destroy() {
        if (mAutoDetector != null) {
            mAutoDetector.destroy();
        }
        mNativeChangeNotifier = 0;
    }

    /**
     * Creates the singleton used by the native-side NetworkChangeNotifier.
     */
    @CalledByNative
    static NetworkChangeNotifier createInstance(Context context, int nativeChangeNotifier) {
        assert sInstance == null;
        sInstance = new NetworkChangeNotifier(context, nativeChangeNotifier);
        return sInstance;
    }

    /**
     * Destroys the singleton used by the native-side NetworkChangeNotifier.
     */
    @CalledByNative
    private static void destroyInstance() {
        assert sInstance != null;
        sInstance.destroy();
        sInstance = null;
    }

    /**
     * Returns the instance used by the native-side NetworkChangeNotifier.
     */
    public static NetworkChangeNotifier getInstance() {
        assert sInstance != null;
        return sInstance;
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

    private void destroyAutoDetector() {
        if (mAutoDetector != null) {
            mAutoDetector.destroy();
            mAutoDetector = null;
        }
    }

    private void setAutoDetectConnectivityStateInternal(boolean shouldAutoDetect) {
        if (shouldAutoDetect) {
            if (mAutoDetector == null) {
                mAutoDetector = new NetworkChangeNotifierAutoDetect(
                    new NetworkChangeNotifierAutoDetect.Observer() {
                        @Override
                        public void onConnectionTypeChanged(int newConnectionType) {
                            notifyObserversOfConnectionTypeChange(newConnectionType);
                        }
                    },
                    mContext);
            }
        } else {
            destroyAutoDetector();
        }
    }

    /**
     * Update the perceived network state when not auto-detecting
     * changes to connectivity.
     *
     * @param networkAvailable True if the NetworkChangeNotifier should
     *     perceive a "connected" state, false implies "disconnected".
     */
    @CalledByNative
    public static void forceConnectivityState(boolean networkAvailable) {
        assert sInstance != null;
        setAutoDetectConnectivityState(false);
        sInstance.forceConnectivityStateInternal(networkAvailable);
    }

    private void forceConnectivityStateInternal(boolean forceOnline) {
        if (mNativeChangeNotifier == 0) {
            return;
        }
        boolean connectionCurrentlyExists =
            nativeGetConnectionType(mNativeChangeNotifier) != CONNECTION_NONE;
        if (connectionCurrentlyExists != forceOnline) {
            notifyObserversOfConnectionTypeChange(
                    forceOnline ? CONNECTION_UNKNOWN : CONNECTION_NONE);
        }
    }

    void notifyObserversOfConnectionTypeChange(int newConnectionType) {
        if (mNativeChangeNotifier != 0) {
            nativeNotifyObserversOfConnectionTypeChange(mNativeChangeNotifier, newConnectionType);
        }
    }

    @NativeClassQualifiedName("NetworkChangeNotifierAndroid")
    private native void nativeNotifyObserversOfConnectionTypeChange(
            int nativePtr, int newConnectionType);

    @NativeClassQualifiedName("NetworkChangeNotifierAndroid")
    private native int nativeGetConnectionType(int nativePtr);

    // For testing only.
    public static NetworkChangeNotifierAutoDetect getAutoDetectorForTest() {
        assert sInstance != null;
        return sInstance.mAutoDetector;
    }
}
