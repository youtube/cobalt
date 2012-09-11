// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.net.ConnectivityManager;
import android.net.NetworkInfo;
import android.telephony.TelephonyManager;
import android.util.Log;

import org.chromium.base.ActivityStatus;

/**
 * Used by the NetworkChangeNotifier to listens to platform changes in connectivity.
 * Note that use of this class requires that the app have the platform
 * ACCESS_NETWORK_STATE permission.
 */
public class NetworkChangeNotifierAutoDetect extends BroadcastReceiver
        implements ActivityStatus.Listener {

    private static final String TAG = "NetworkChangeNotifierAutoDetect";

    private final NetworkConnectivityIntentFilter mIntentFilter =
            new NetworkConnectivityIntentFilter();

    private final NetworkChangeNotifier mOwner;

    private final Context mContext;
    private boolean mRegistered;
    private int mConnectionType;

    public NetworkChangeNotifierAutoDetect(NetworkChangeNotifier owner, Context context) {
        mOwner = owner;
        mContext = context;
        mConnectionType = currentConnectionType(context);

        ActivityStatus status = ActivityStatus.getInstance();
        if (!status.isPaused()) {
          registerReceiver();
        }
        status.registerListener(this);
    }

    public int connectionType() {
        return mConnectionType;
    }

    public void destroy() {
        unregisterReceiver();
    }

    /**
     * Register a BroadcastReceiver in the given context.
     */
    private void registerReceiver() {
        if (!mRegistered) {
          mRegistered = true;
          mContext.registerReceiver(this, mIntentFilter);
        }
    }

    /**
     * Unregister the BroadcastReceiver in the given context.
     */
    private void unregisterReceiver() {
        if (mRegistered) {
           mRegistered = false;
           mContext.unregisterReceiver(this);
        }
    }

    private int currentConnectionType(Context context) {
        // Track exactly what type of connection we have.
        ConnectivityManager connectivityManager = (ConnectivityManager)
                context.getSystemService(Context.CONNECTIVITY_SERVICE);
        NetworkInfo activeNetworkInfo = connectivityManager.getActiveNetworkInfo();
        if (activeNetworkInfo == null) {
            return NetworkChangeNotifier.CONNECTION_NONE;
        }

        switch (activeNetworkInfo.getType()) {
            case ConnectivityManager.TYPE_ETHERNET:
                return NetworkChangeNotifier.CONNECTION_ETHERNET;
            case ConnectivityManager.TYPE_WIFI:
                return NetworkChangeNotifier.CONNECTION_WIFI;
            case ConnectivityManager.TYPE_WIMAX:
                return NetworkChangeNotifier.CONNECTION_4G;
            case ConnectivityManager.TYPE_MOBILE:
                // Use information from TelephonyManager to classify the connection.
                switch (activeNetworkInfo.getSubtype()) {
                    case TelephonyManager.NETWORK_TYPE_GPRS:
                    case TelephonyManager.NETWORK_TYPE_EDGE:
                    case TelephonyManager.NETWORK_TYPE_CDMA:
                    case TelephonyManager.NETWORK_TYPE_1xRTT:
                    case TelephonyManager.NETWORK_TYPE_IDEN:
                        return NetworkChangeNotifier.CONNECTION_2G;
                    case TelephonyManager.NETWORK_TYPE_UMTS:
                    case TelephonyManager.NETWORK_TYPE_EVDO_0:
                    case TelephonyManager.NETWORK_TYPE_EVDO_A:
                    case TelephonyManager.NETWORK_TYPE_HSDPA:
                    case TelephonyManager.NETWORK_TYPE_HSUPA:
                    case TelephonyManager.NETWORK_TYPE_HSPA:
                    case TelephonyManager.NETWORK_TYPE_EVDO_B:
                    case TelephonyManager.NETWORK_TYPE_EHRPD:
                    case TelephonyManager.NETWORK_TYPE_HSPAP:
                        return NetworkChangeNotifier.CONNECTION_3G;
                    case TelephonyManager.NETWORK_TYPE_LTE:
                        return NetworkChangeNotifier.CONNECTION_4G;
                    default:
                        return NetworkChangeNotifier.CONNECTION_UNKNOWN;
                }
            default:
                return NetworkChangeNotifier.CONNECTION_UNKNOWN;
        }
    }

    // BroadcastReceiver
    @Override
    public void onReceive(Context context, Intent intent) {
        boolean noConnection =
                intent.getBooleanExtra(ConnectivityManager.EXTRA_NO_CONNECTIVITY, false);
        int newConnectionType = noConnection ?
                NetworkChangeNotifier.CONNECTION_NONE : currentConnectionType(context);

        if (newConnectionType != mConnectionType) {
            mConnectionType = newConnectionType;
            Log.d(TAG, "Network connectivity changed, type is: " + mConnectionType);
            mOwner.notifyNativeObservers();
        }
    }

    // AcitivityStatus.Listener
    @Override
    public void onActivityStatusChanged(boolean isPaused) {
        if (isPaused) {
            unregisterReceiver();
        } else {
            registerReceiver();
        }
    }

    private static class NetworkConnectivityIntentFilter extends IntentFilter {
        NetworkConnectivityIntentFilter() {
                addAction(ConnectivityManager.CONNECTIVITY_ACTION);
        }
    }
}
