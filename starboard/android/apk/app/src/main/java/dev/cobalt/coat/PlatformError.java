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

/** Shows an ErrorDialog to inform the user of a Starboard platform error. */
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

  private final Holder<Activity> activityHolder;
  private final @ErrorType int errorType;
  private final long data;
  private final Handler uiThreadHandler;

  private Dialog dialog;
  private int response;

  public PlatformError(Holder<Activity> activityHolder, @ErrorType int errorType, long data) {
    this.activityHolder = activityHolder;
    this.errorType = errorType;
    this.data = data;
    uiThreadHandler = new Handler(Looper.getMainLooper());
    response = CANCELLED;
  }

  /** Display the error. */
  public void raise() {
    uiThreadHandler.post(
        new Runnable() {
          @Override
          public void run() {
            showDialogOnUiThread();
          }
        });
  }

  private void showDialogOnUiThread() {
    Activity activity = activityHolder.get();
    if (activity == null) {
      sendResponse(CANCELLED, data);
      return;
    }
    ErrorDialog.Builder dialogBuilder = new ErrorDialog.Builder(activity);
    switch (errorType) {
      case CONNECTION_ERROR:
        dialogBuilder
            .setMessage(R.string.starboard_platform_connection_error)
            .addButton(RETRY_BUTTON, R.string.starboard_platform_retry)
            .addButton(NETWORK_SETTINGS_BUTTON, R.string.starboard_platform_network_settings);
        break;
      default:
        Log.e(TAG, "Unknown platform error " + errorType);
        return;
    }
    dialog = dialogBuilder.setButtonClickListener(this).setOnDismissListener(this).create();
    dialog.show();
  }

  @Override
  public void onClick(DialogInterface dialogInterface, int whichButton) {
    if (errorType == CONNECTION_ERROR) {
      switch (whichButton) {
        case NETWORK_SETTINGS_BUTTON:
          Activity activity = activityHolder.get();
          if (activity != null) {
            activity.startActivity(new Intent(Settings.ACTION_WIFI_SETTINGS));
          }
          break;
        case RETRY_BUTTON:
          response = POSITIVE;
          dialog.dismiss();
          break;
        default: // fall out
      }
    }
  }

  @Override
  public void onDismiss(DialogInterface dialogInterface) {
    dialog = null;
    sendResponse(response, data);
  }

  /** Informs Starboard when the error is dismissed. */
  protected void sendResponse(@PlatformError.Response int response, long data) {
    nativeSendResponse(response, data);
  }

  private native void nativeSendResponse(@PlatformError.Response int response, long data);
}
