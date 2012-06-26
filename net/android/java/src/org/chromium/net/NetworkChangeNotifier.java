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
import org.chromium.base.CalledByNative;
import org.chromium.base.JNINamespace;
import org.chromium.base.NativeClassQualifiedName;

/**
 * Triggers updates to the underlying network state in native Chrome
 */
@JNINamespace("net")
public class NetworkChangeNotifier extends BroadcastReceiver implements ActivityStatus.Listener {

    private static final String TAG = "NetworkChangeNotifier";

    private final NetworkConnectivityIntentFilter mIntentFilter =
            new NetworkConnectivityIntentFilter();

    private final Context mContext;
    private final int mNativeChangeNotifier;
    private boolean mRegistered;
    private boolean mIsConnected;

    private static NetworkChangeNotifier sNetworkChangeNotifierForTest;

    public static NetworkChangeNotifier getNetworkChangeNotifierForTest() {
      return sNetworkChangeNotifierForTest;
    }

    private NetworkChangeNotifier(Context context, int nativeChangeNotifier) {
        mContext = context;
        mNativeChangeNotifier = nativeChangeNotifier;
        mIsConnected = checkIfConnected(mContext);
        ActivityStatus status = ActivityStatus.getInstance();
        if (!status.isPaused()) {
          registerReceiver();
        }
        status.registerListener(this);
        sNetworkChangeNotifierForTest = this;
    }

    @Override
    public void onReceive(Context context, Intent intent) {
        if (intent.getBooleanExtra(ConnectivityManager.EXTRA_NO_CONNECTIVITY, false)) {
            if (mIsConnected) {
              mIsConnected = false;
              Log.d(TAG, "Network connectivity changed, no connectivity.");
              nativeNotifyObservers(mNativeChangeNotifier);
            }
        } else {
            boolean isConnected = checkIfConnected(context);
            if (isConnected != mIsConnected) {
              mIsConnected = isConnected;
              Log.d(TAG, "Network connectivity changed, status is: " + isConnected);
              nativeNotifyObservers(mNativeChangeNotifier);
            }
        }
    }

    private boolean checkIfConnected(Context context) {
      ConnectivityManager manager = (ConnectivityManager)
              context.getSystemService(Context.CONNECTIVITY_SERVICE);
      boolean isConnected = false;
      for (NetworkInfo info: manager.getAllNetworkInfo()) {
          if (info.isConnected()) {
              isConnected = true;
              break;
          }
      }
      return isConnected;
    }

    @CalledByNative
    private boolean isConnected() {
      return mIsConnected;
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
    @CalledByNative
    private void unregisterReceiver() {
        if (mRegistered) {
           mRegistered = false;
           mContext.unregisterReceiver(this);
        }
    }

    @Override
    public void onActivityStatusChanged(boolean isPaused) {
        if (isPaused) {
            unregisterReceiver();
        } else {
            registerReceiver();
        }
    }

    @CalledByNative
    static NetworkChangeNotifier create(Context context, int nativeNetworkChangeNotifier) {
        return new NetworkChangeNotifier(context, nativeNetworkChangeNotifier);
    }

    private static class NetworkConnectivityIntentFilter extends IntentFilter {
      NetworkConnectivityIntentFilter() {
          addAction(ConnectivityManager.CONNECTIVITY_ACTION);
      }
    }

    @NativeClassQualifiedName("android::NetworkChangeNotifier")
    private native void nativeNotifyObservers(int nativePtr);
}
