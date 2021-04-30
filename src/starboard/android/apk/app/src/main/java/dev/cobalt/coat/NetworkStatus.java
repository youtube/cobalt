// Copyright 2021 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

package dev.cobalt.coat;

import static dev.cobalt.util.Log.TAG;

import android.content.Context;
import android.net.ConnectivityManager;
import android.net.ConnectivityManager.NetworkCallback;
import android.net.Network;
import android.net.NetworkCapabilities;
import android.os.Build;
import android.os.Handler;
import android.os.Looper;
import dev.cobalt.util.Log;

/** Class to help Cobalt monitor status change. */
public class NetworkStatus {

  private NetworkCallback networkCallback;
  private ConnectivityManager connectivityManager;
  private final Handler mainHandler = new Handler(Looper.getMainLooper());
  private boolean callbackAdded = false;

  static class CobaltNetworkCallback extends ConnectivityManager.NetworkCallback {
    private final NetworkStatus networkStatus;

    /** Constructor for CobaltNetworkCallback */
    public CobaltNetworkCallback(NetworkStatus networkStatus, Handler handler) {
      super();
      this.networkStatus = networkStatus;
    }

    @Override
    public void onLost(Network network) {
      this.networkStatus.sendStatusChange(false);
    }

    @Override
    public void onAvailable(Network network) {
      this.networkStatus.sendStatusChange(true);
    }
  };

  public void sendStatusChange(final boolean online) {
    this.mainHandler.post(
        new Runnable() {
          @Override
          public void run() {
            sendStatusChangeInternal(online);
          }
        });
  }

  private void sendStatusChangeInternal(boolean online) {
    nativeOnNetworkStatusChange(online);
  }

  public NetworkStatus(Context appContext) {
    connectivityManager =
        (ConnectivityManager) appContext.getSystemService(Context.CONNECTIVITY_SERVICE);
    Log.i(TAG, "Opening NetworkStatus");
    networkCallback = new CobaltNetworkCallback(this, mainHandler);
    if (Build.VERSION.SDK_INT >= 24) {
      connectivityManager.registerDefaultNetworkCallback(networkCallback);
      callbackAdded = true;
    }
  }

  public void beforeStartOrResume() {
    if (connectivityManager != null && !callbackAdded) {
      if (Build.VERSION.SDK_INT >= 24) {
        connectivityManager.registerDefaultNetworkCallback(networkCallback);
        callbackAdded = true;
      }
    }
  }

  public void beforeSuspend() {
    if (connectivityManager != null && callbackAdded) {
      if (Build.VERSION.SDK_INT >= 24) {
        connectivityManager.unregisterNetworkCallback(networkCallback);
        callbackAdded = false;
      }
    }
  }

  public boolean isConnected() {
    if (connectivityManager != null && Build.VERSION.SDK_INT >= 24) {
      // Return the current network bandwidth when client pings the NetworkStatus service.
      NetworkCapabilities cap =
          connectivityManager.getNetworkCapabilities(connectivityManager.getActiveNetwork());
      return cap != null
          && (cap.hasTransport(NetworkCapabilities.TRANSPORT_WIFI)
              || cap.hasTransport(NetworkCapabilities.TRANSPORT_CELLULAR)
              || cap.hasTransport(NetworkCapabilities.TRANSPORT_ETHERNET)
              || cap.hasTransport(NetworkCapabilities.TRANSPORT_BLUETOOTH)
              || cap.hasTransport(NetworkCapabilities.TRANSPORT_VPN)
              || cap.hasTransport(NetworkCapabilities.TRANSPORT_WIFI_AWARE)
              || cap.hasTransport(NetworkCapabilities.TRANSPORT_LOWPAN));
    } else {
      return true;
    }
  }

  private native void nativeOnNetworkStatusChange(boolean online);
}
