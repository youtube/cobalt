// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import android.content.Context;

import org.chromium.base.CalledByNative;
import org.chromium.base.JNINamespace;
import org.chromium.base.NativeClassQualifiedName;

import java.util.concurrent.CopyOnWriteArrayList;

/**
 * Triggers updates to the underlying network state in Chrome.
 * By default, connectivity is assumed and changes must pushed from
 * the embedder via the forceConnectivityState function.
 * Embedders may choose to have this class auto-detect changes in
 * network connectivity by invoking the autoDetectConnectivityState
 * function.
 * This class is not thread-safe.
 */
@JNINamespace("net")
public class NetworkChangeNotifier {
    /**
     * Alerted when the connection type of the network changes.
     * The alert is fired on the UI thread.
     */
    public interface ConnectionTypeObserver {
        public void onConnectionTypeChanged(int connectionType);
    }

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
    private final CopyOnWriteArrayList<ConnectionTypeObserver> mConnectionTypeObservers;
    private NetworkChangeNotifierAutoDetect mAutoDetector;

    private static NetworkChangeNotifier sInstance;

    private NetworkChangeNotifier(Context context, int nativeChangeNotifier) {
        mContext = context;
        mNativeChangeNotifier = nativeChangeNotifier;
        mConnectionTypeObservers = new CopyOnWriteArrayList<ConnectionTypeObserver>();
    }

    private void destroy() {
        if (mAutoDetector != null) {
            mAutoDetector.destroy();
        }
        mNativeChangeNotifier = 0;
        mConnectionTypeObservers.clear();
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
        getInstance().setAutoDetectConnectivityStateInternal(shouldAutoDetect);
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
        setAutoDetectConnectivityState(false);
        getInstance().forceConnectivityStateInternal(networkAvailable);
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

    /**
     * Alerts all observers of a connection change.
     */
    void notifyObserversOfConnectionTypeChange(int newConnectionType) {
        if (mNativeChangeNotifier != 0) {
            nativeNotifyConnectionTypeChanged(mNativeChangeNotifier, newConnectionType);
        }

        for (ConnectionTypeObserver observer : mConnectionTypeObservers) {
            observer.onConnectionTypeChanged(newConnectionType);
        }
    }

    /**
     * Adds an observer for any connection type changes.
     */
    public static void addConnectionTypeObserver(ConnectionTypeObserver observer) {
        getInstance().addConnectionTypeObserverInternal(observer);
    }

    private void addConnectionTypeObserverInternal(ConnectionTypeObserver observer) {
        if (!mConnectionTypeObservers.contains(observer))
            mConnectionTypeObservers.add(observer);
    }

    /**
     * Removes an observer for any connection type changes.
     */
    public static boolean removeConnectionTypeObserver(ConnectionTypeObserver observer) {
        return getInstance().removeConnectionTypeObserverInternal(observer);
    }

    private boolean removeConnectionTypeObserverInternal(ConnectionTypeObserver observer) {
        return mConnectionTypeObservers.remove(observer);
    }

    @NativeClassQualifiedName("NetworkChangeNotifierDelegateAndroid")
    private native void nativeNotifyConnectionTypeChanged(int nativePtr, int newConnectionType);

    @NativeClassQualifiedName("NetworkChangeNotifierDelegateAndroid")
    private native int nativeGetConnectionType(int nativePtr);

    // For testing only.
    public static NetworkChangeNotifierAutoDetect getAutoDetectorForTest() {
        return getInstance().mAutoDetector;
    }
}
