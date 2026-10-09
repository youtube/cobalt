// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

import android.app.Activity;
import android.app.Dialog;
import android.content.ActivityNotFoundException;
import android.content.DialogInterface;
import android.content.Intent;
import android.net.ConnectivityManager;
import android.net.Network;
import android.net.NetworkCapabilities;
import android.os.Handler;
import android.os.Looper;
import android.provider.Settings;
import dev.cobalt.util.Holder;
import dev.cobalt.util.Log;
import org.jni_zero.NativeMethods;

/**
 * The AOSP version of PlatformError. It mostly matches the Android TV one, minus the parts AOSP
 * doesn't build or need. It also retries when the device gets back online, which Android TV does in
 * CobaltActivity.
 *
 * <p>StarboardBridge creates one for each platform error. It lives until the error gets a response.
 * raise() can be called from any thread, and everything else runs on the main thread.
 */
public class PlatformError
    implements DialogInterface.OnClickListener, DialogInterface.OnDismissListener {

  // This must be kept in sync with starboard/android/shared/system_platform_error.cc
  public static final int CONNECTION_ERROR = 0;

  public static final int NEGATIVE = -1;
  public static final int CANCELLED = 0;
  public static final int POSITIVE = 1;

  // Button IDs for CONNECTION_ERROR
  private static final int RETRY_BUTTON = 1;
  private static final int NETWORK_SETTINGS_BUTTON = 2;
  private static final int DISMISS_BUTTON = 3;

  private final Holder<Activity> mActivityHolder;
  private final int mErrorType;
  private final long mData;
  private final boolean mDisableDismissButton;
  private final Handler mUiThreadHandler;

  private Dialog mDialog;
  private int mResponse;
  private boolean mResponded;
  private ConnectivityManager mConnectivityManager;
  private ConnectivityManager.NetworkCallback mNetworkCallback;

  public PlatformError(
      Holder<Activity> activityHolder, int errorType, long data, boolean disableDismissButton) {
    mActivityHolder = activityHolder;
    mErrorType = errorType;
    mData = data;
    mDisableDismissButton = disableDismissButton;
    mUiThreadHandler = new Handler(Looper.getMainLooper());
    mResponse = CANCELLED;
  }

  /** Displays the error. Without an activity, it waits until raise() is called again. */
  public void raise() {
    mUiThreadHandler.post(this::showDialogOnUiThread);
  }

  /** Tells if the error still needs to be shown. Main thread only. */
  public boolean isPending() {
    return !mResponded && mDialog == null;
  }

  private void showDialogOnUiThread() {
    if (!isPending()) {
      return;
    }
    Activity activity = mActivityHolder.get();
    if (activity == null) {
      // The app is in the background. StarboardBridge raises it again when an activity starts.
      return;
    }
    ErrorDialog.Builder dialogBuilder = new ErrorDialog.Builder(activity);
    switch (mErrorType) {
      case CONNECTION_ERROR:
        dialogBuilder
            .setMessage(R.string.starboard_platform_connection_error)
            .addButton(RETRY_BUTTON, R.string.starboard_platform_retry)
            .addButton(NETWORK_SETTINGS_BUTTON, R.string.starboard_platform_network_settings);
        if (!mDisableDismissButton) {
          dialogBuilder.addButton(DISMISS_BUTTON, R.string.starboard_platform_dismiss);
        }
        break;
      default:
        Log.e(TAG, "Unknown platform error " + mErrorType);
        sendResponse(CANCELLED, mData);
        return;
    }
    mDialog = dialogBuilder.setButtonClickListener(this).setOnDismissListener(this).create();
    mDialog.show();
    if (mErrorType == CONNECTION_ERROR) {
      registerNetworkCallback(activity);
    }
  }

  /**
   * Retries once the default network has internet access again. A device that was already online
   * when the error was raised doesn't retry, so an unreachable server doesn't cause a retry loop.
   */
  private void registerNetworkCallback(Activity activity) {
    // Kept for unregistering, since the activity can be gone by then.
    ConnectivityManager connectivityManager =
        activity.getApplicationContext().getSystemService(ConnectivityManager.class);
    if (connectivityManager == null) {
      return;
    }
    NetworkCapabilities capabilities =
        connectivityManager.getNetworkCapabilities(connectivityManager.getActiveNetwork());
    final boolean wasOnline =
        capabilities != null
            && capabilities.hasCapability(NetworkCapabilities.NET_CAPABILITY_VALIDATED);
    mNetworkCallback =
        new ConnectivityManager.NetworkCallback() {
          private boolean mOnline = wasOnline;

          @Override
          public void onCapabilitiesChanged(Network network, NetworkCapabilities capabilities) {
            boolean online =
                capabilities.hasCapability(NetworkCapabilities.NET_CAPABILITY_VALIDATED);
            if (online && !mOnline) {
              mUiThreadHandler.post(PlatformError.this::retryOnNetworkOnline);
            }
            mOnline = online;
          }

          @Override
          public void onLost(Network network) {
            mOnline = false;
          }
        };
    connectivityManager.registerDefaultNetworkCallback(mNetworkCallback);
    mConnectivityManager = connectivityManager;
  }

  private void unregisterNetworkCallback() {
    if (mNetworkCallback == null) {
      return;
    }
    mConnectivityManager.unregisterNetworkCallback(mNetworkCallback);
    mNetworkCallback = null;
    mConnectivityManager = null;
  }

  private void retryOnNetworkOnline() {
    if (mDialog == null) {
      return;
    }
    Log.i(TAG, "Network is online again, retrying.");
    // Same as Try again: the positive response makes Cobalt reload the page.
    mResponse = POSITIVE;
    mDialog.dismiss();
  }

  @Override
  public void onClick(DialogInterface dialogInterface, int whichButton) {
    if (mErrorType != CONNECTION_ERROR) {
      return;
    }
    switch (whichButton) {
      case NETWORK_SETTINGS_BUTTON:
        mResponse = POSITIVE;
        Activity activity = mActivityHolder.get();
        if (activity != null) {
          try {
            activity.startActivity(new Intent(Settings.ACTION_WIFI_SETTINGS));
          } catch (ActivityNotFoundException e) {
            Log.e(TAG, "Failed to start activity for ACTION_WIFI_SETTINGS.");
          }
        }
        break;
      case RETRY_BUTTON:
        mResponse = POSITIVE;
        mDialog.dismiss();
        break;
      case DISMISS_BUTTON:
        mResponse = NEGATIVE;
        mDialog.dismiss();
        break;
      default: // fall out
    }
  }

  @Override
  public void onDismiss(DialogInterface dialogInterface) {
    unregisterNetworkCallback();
    mDialog = null;
    sendResponse(mResponse, mData);
  }

  /** Informs Starboard when the error dialog is dismissed. */
  private void sendResponse(int response, long data) {
    mResponded = true;
    PlatformErrorJni.get().sendResponse(response, data);
  }

  @NativeMethods
  interface Natives {
    void sendResponse(int response, long data);
  }
}
