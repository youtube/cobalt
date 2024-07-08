// Copyright 2019 The Cobalt Authors. All Rights Reserved.
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

import static androidx.core.content.ContextCompat.RECEIVER_EXPORTED;
import static dev.cobalt.util.Log.TAG;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;

import dev.cobalt.util.Log;

// Helper class to receive and handle broadcast notifications of system locale
// changes.
final class CobaltSystemConfigChangeReceiver extends BroadcastReceiver {
  private boolean isForeground;
  private final Runnable stopRequester;

  private static final String INTENT_SETTINGS_UPDATE =
  "dev.cobalt.coat.UPDATE_SETTING";

  CobaltSystemConfigChangeReceiver(Context appContext, Runnable stopRequester) {
    this.isForeground = true;
    this.stopRequester = stopRequester;
    IntentFilter filter = new IntentFilter();
    filter.addAction(Intent.ACTION_LOCALE_CHANGED);
    filter.addAction(Intent.ACTION_TIMEZONE_CHANGED);
    filter.addAction(Intent.ACTION_TIME_CHANGED);
    filter.addAction(Intent.ACTION_DATE_CHANGED);
    filter.addAction(INTENT_SETTINGS_UPDATE);
    appContext.registerReceiver(this, filter, Context.RECEIVER_EXPORTED);
  }

  @Override
  public void onReceive(Context context, Intent intent) {
    switch (intent.getAction()) {
      case INTENT_SETTINGS_UPDATE:
        String key = intent.getStringExtra("key");
        String value = intent.getStringExtra("value");
        Log.w(TAG, "Settings update intent: " + key + " = " + value);
        Helpers.setAppSetting(context,key,value);
        return;
    }

    if (isForeground) {
      Log.i(TAG, "Ignoring system config change intent while in foreground.");
      return;
    }

    switch (intent.getAction()) {
      case Intent.ACTION_TIMEZONE_CHANGED:
      case Intent.ACTION_TIME_CHANGED:
      case Intent.ACTION_DATE_CHANGED:
        Log.w(TAG, "System Date or Time have changed.");
        break;
      case Intent.ACTION_LOCALE_CHANGED:
        Log.w(TAG, "System locale settings have changed.");
        stopRequester.run();
        break;
      default:
        Log.w(TAG, "Unknown intent.");
    }
  }

  public void setForeground(final boolean isForeground) {
    this.isForeground = isForeground;
  }

}
