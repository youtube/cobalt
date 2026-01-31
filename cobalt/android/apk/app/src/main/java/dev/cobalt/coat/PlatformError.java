// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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
import android.os.Handler;
import android.os.Looper;
import android.provider.Settings;
import androidx.annotation.IntDef;
import dev.cobalt.util.Holder;
import dev.cobalt.util.Log;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import org.chromium.content_public.browser.WebContents;
import org.jni_zero.NativeMethods;

/**
 * Shows an ErrorDialog to inform the user of a network platform error.
 * The dialog should appear if the device has no wifi or ethernet connection.
 * It should also appear in cases of "weak" internet (ie. connected to a network
 * that doesn't have internet like a router that was just unplugged) as well as if
 * the connection is experiencing DNS resolution errors. Prompts the user to retry
 * or to navigate to the device's network settings menu.
*/
public class PlatformError
    implements DialogInterface.OnClickListener, DialogInterface.OnDismissListener {

  @Retention(RetentionPolicy.SOURCE)
  @IntDef({CONNECTION_ERROR})
  @interface ErrorType {}

  // This must be kept in sync with starboard/android/shared/system_platform_error.cc
  public static final int CONNECTION_ERROR = 0;

  @Retention(RetentionPolicy.SOURCE)
  @IntDef({CANCELLED, NEGATIVE, POSITIVE})
  @interface Response {}

  public static final int NEGATIVE = -1;
  public static final int CANCELLED = 0;
  public static final int POSITIVE = 1;

  // Button IDs for CONNECTION_ERROR
  private static final int RETRY_BUTTON = 1;
  private static final int NETWORK_SETTINGS_BUTTON = 2;
  private static final int DISMISS_BUTTON = 3;

  private final Holder<Activity> mActivityHolder;
  private final @ErrorType int mErrorType;
  private final long mData;
  private final Handler mUiThreadHandler;

  private Dialog mDialog;
  private int mResponse;

  public PlatformError(Holder<Activity> activityHolder, @ErrorType int errorType, long data) {
    mActivityHolder = activityHolder;
    mErrorType = errorType;
    mData = data;
    mUiThreadHandler = new Handler(Looper.getMainLooper());
    mResponse = CANCELLED;
  }

  /** Display the error. */
  public void raise() {
    mUiThreadHandler.post(
        new Runnable() {
          @Override
          public void run() {
            showDialogOnUiThread();
          }
        });
  }

  public void setResponse(@Response int response) {
    mResponse = response;
  }

  private void showDialogOnUiThread() {
    Activity activity = mActivityHolder.get();
    if (activity == null) {
      sendResponse(CANCELLED, mData);
      return;
    }
    ErrorDialog.Builder dialogBuilder = new ErrorDialog.Builder(activity);
    switch (mErrorType) {
      case CONNECTION_ERROR:
        dialogBuilder
            .setMessage(R.string.starboard_platform_connection_error)
            .addButton(RETRY_BUTTON, R.string.starboard_platform_retry)
            .addButton(NETWORK_SETTINGS_BUTTON, R.string.starboard_platform_network_settings)
            .addButton(DISMISS_BUTTON, R.string.starboard_platform_dismiss);
        break;
      default:
        Log.e(TAG, "Unknown platform error " + mErrorType);
        return;
    }
    mDialog = dialogBuilder.setButtonClickListener(this).setOnDismissListener(this).create();
    mDialog.show();
  }

  public void dismiss() {
    mUiThreadHandler.post(
        () -> {
          if (mDialog != null) {
            mDialog.dismiss();
          }
        });
  }

  public boolean isShowing() {
    return mDialog != null && mDialog.isShowing();
  }

  @Override
  public void onClick(DialogInterface dialogInterface, int whichButton) {
    if (mErrorType == CONNECTION_ERROR) {
      CobaltActivity cobaltActivity = (CobaltActivity) mActivityHolder.get();
      switch (whichButton) {
        case NETWORK_SETTINGS_BUTTON:
          mResponse = POSITIVE;
          if (cobaltActivity != null) {
            try {
              cobaltActivity.startActivity(new Intent(Settings.ACTION_WIFI_SETTINGS));
            } catch (ActivityNotFoundException e) {
              Log.e(TAG, "Failed to start activity for ACTION_WIFI_SETTINGS.");
            }
          }
          break;
        case RETRY_BUTTON:
          mResponse = POSITIVE;
          mDialog.dismiss();
          reloadWebContents(cobaltActivity);
          break;
        case DISMISS_BUTTON:
          mResponse = NEGATIVE;
          mDialog.dismiss();
          break;
        default: // fall out
      }
    }
  }

  @Override
  public void onDismiss(DialogInterface dialogInterface) {
    mDialog = null;
    if (mResponse == CANCELLED) {
      CobaltActivity cobaltActivity = (CobaltActivity) mActivityHolder.get();
      if (cobaltActivity != null) {
        cobaltActivity.getStarboardBridge().requestSuspend();
        reloadWebContents(cobaltActivity);
      }
    }
  }

  /** Informs Starboard when the error is dismissed. */
  protected void sendResponse(@PlatformError.Response int response, long data) {
    PlatformErrorJni.get().sendResponse(response, data);
  }

  @NativeMethods
  interface Natives {
    void sendResponse(@PlatformError.Response int response, long data);
  }

  /** Reloads the web contents if available */
  private void reloadWebContents(CobaltActivity cobaltActivity) {
    if (cobaltActivity != null) {
      WebContents webContents = cobaltActivity.getActiveWebContents();
      if (webContents != null) {
        webContents.getNavigationController().reload(true);
      } else {
        Log.e(TAG, "WebContents is null and not available to reload the application.");
      }
    }
  }

}
