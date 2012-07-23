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
    private boolean mIsConnected;

    public NetworkChangeNotifierAutoDetect(NetworkChangeNotifier owner, Context context) {
        mOwner = owner;
        mContext = context;
        mIsConnected = checkIfConnected(mContext);

        ActivityStatus status = ActivityStatus.getInstance();
        if (!status.isPaused()) {
          registerReceiver();
        }
        status.registerListener(this);
    }

    public boolean isConnected() {
        return mIsConnected;
    }

    public void destroy() {
        unregisterReceiver();
    }

    private boolean checkIfConnected(Context context) {
        ConnectivityManager manager = (ConnectivityManager)
                context.getSystemService(Context.CONNECTIVITY_SERVICE);
        for (NetworkInfo info: manager.getAllNetworkInfo()) {
            if (info.isConnected()) {
                return true;
            }
        }
        return false;
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

    // BroadcastReceiver
    @Override
    public void onReceive(Context context, Intent intent) {
        if (intent.getBooleanExtra(ConnectivityManager.EXTRA_NO_CONNECTIVITY, false)) {
            if (mIsConnected) {
                mIsConnected = false;
                Log.d(TAG, "Network connectivity changed, no connectivity.");
                mOwner.notifyNativeObservers();
            }
        } else {
            boolean isConnected = checkIfConnected(context);
            if (isConnected != mIsConnected) {
                mIsConnected = isConnected;
                Log.d(TAG, "Network connectivity changed, status is: " + isConnected);
                mOwner.notifyNativeObservers();
            }
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
