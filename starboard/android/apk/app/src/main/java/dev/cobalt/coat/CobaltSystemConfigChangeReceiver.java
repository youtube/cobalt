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

  CobaltSystemConfigChangeReceiver(Context appContext, Runnable stopRequester) {
    this.isForeground = true;
    this.stopRequester = stopRequester;
    appContext.registerReceiver(this, new IntentFilter(Intent.ACTION_LOCALE_CHANGED));
  }

  @Override
  public void onReceive(Context context, Intent intent) {
    if ((!Intent.ACTION_LOCALE_CHANGED.equals(intent.getAction())) || isForeground) return;

    Log.w(TAG, "System locale settings have changed.");
    stopRequester.run();
  }

  public void setForeground(final boolean isForeground) {
    this.isForeground = isForeground;
  }
}
