// Copyright 2018 The Cobalt Authors. All Rights Reserved.
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

import android.Manifest;
import android.app.Activity;
import android.content.Context;
import android.content.pm.PackageManager;
import androidx.core.app.ActivityCompat;
import androidx.core.content.ContextCompat;
import dev.cobalt.util.Holder;
import dev.cobalt.util.Log;
import org.chromium.base.annotations.CalledByNative;

/** Helper class that requests the record audio permission. */
public class AudioPermissionRequester {
  private final Context context;
  private final Holder<Activity> activityHolder;
  // Only use in synchronized methods.
  private boolean requestAudioPermissionStarted;

  public AudioPermissionRequester(Context context, Holder<Activity> activityHolder) {
    this.context = context;
    this.activityHolder = activityHolder;
  }

  /**
   * Requests the RECORD_AUDIO permission. Returns true if the permission is granted; returns false
   * if the permission is not granted yet and starts to request the RECORD_AUDIO permission.
   */
  @SuppressWarnings("unused")
  @CalledByNative
  public synchronized boolean requestRecordAudioPermission() {
    Activity activity = activityHolder.get();
    if (activity == null) {
      return false;
    }

    if (ContextCompat.checkSelfPermission(activity, Manifest.permission.RECORD_AUDIO)
        == PackageManager.PERMISSION_GRANTED) {
      return true;
    }

    if (!requestAudioPermissionStarted) {
      ActivityCompat.requestPermissions(
          activity, new String[] {Manifest.permission.RECORD_AUDIO}, R.id.rc_record_audio);
      requestAudioPermissionStarted = true;
    }

    return false;
  }

  /** Handles the RECORD_AUDIO request result. */
  public synchronized void onRequestPermissionsResult(
      int requestCode, String[] permissions, int[] grantResults) {
    if (requestCode == R.id.rc_record_audio) {
      boolean success = grantResults.length > 0 && grantResults[0] == PackageManager.PERMISSION_GRANTED;
      Log.i(TAG, "RECORD_AUDIO permission request " + (success ? "GRANTED" : "DENIED"));
      requestAudioPermissionStarted = false;
    }
  }
}
