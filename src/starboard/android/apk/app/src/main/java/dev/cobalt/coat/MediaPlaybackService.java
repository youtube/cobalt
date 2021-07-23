// Copyright 2020 The Cobalt Authors. All Rights Reserved.
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

import static dev.cobalt.media.Log.TAG;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.os.Build;
import android.os.Build.VERSION;
import android.os.IBinder;
import androidx.annotation.RequiresApi;
import androidx.core.app.NotificationCompat;
import dev.cobalt.util.Log;
import dev.cobalt.util.UsedByNative;

public class MediaPlaybackService extends Service {

  private static final int NOTIFICATION_ID = 193266736; // CL number for uniqueness.
  private static final String NOTIFICATION_CHANNEL_ID = "dev.cobalt.coat media playback service";
  private static final String NOTIFICATION_CHANNEL_NAME = "Media playback service";

  @Override
  public void onCreate() {
    super.onCreate();
    Log.i(TAG, "Creating a Media playback foreground service.");
    if (getStarboardBridge() == null) {
      Log.e(TAG, "StarboardBridge already destroyed.");
      return;
    }
    getStarboardBridge().onServiceStart(this);
  }

  @Override
  public int onStartCommand(Intent intent, int flags, int startId) {
    Log.i(TAG, "Cold start - Starting the service.");
    startService();
    // We don't want the system to recreate a service for us.
    return START_NOT_STICKY;
  }

  @Override
  public IBinder onBind(Intent intent) {
    // Do not support binding.
    return null;
  }

  @Override
  public void onDestroy() {
    if (getStarboardBridge() == null) {
      Log.e(TAG, "StarboardBridge already destroyed.");
      return;
    }
    getStarboardBridge().onServiceDestroy(this);
    super.onDestroy();
    Log.i(TAG, "Destroying the Media playback service.");
  }

  public void startService() {
    createChannel();
    startForeground(NOTIFICATION_ID, buildNotification());
  }

  public void stopService() {
    // Do not remove notification here.
    stopForeground(false);
    stopSelf();
    // Delete notification after foreground stopped.
    deleteChannel();
    hideNotification();
  }

  private void hideNotification() {
    Log.i(TAG, "Hiding notification after stopped the service");
    NotificationManager notificationManager =
        (NotificationManager) this.getSystemService(Context.NOTIFICATION_SERVICE);
    notificationManager.cancel(NOTIFICATION_ID);
  }

  private void createChannel() {
    if (Build.VERSION.SDK_INT >= 26) {
      createChannelInternalV26();
    }
  }

  @RequiresApi(26)
  private void createChannelInternalV26() {
    NotificationManager notificationManager =
        (NotificationManager) this.getSystemService(Context.NOTIFICATION_SERVICE);
    NotificationChannel channel =
        new NotificationChannel(
            NOTIFICATION_CHANNEL_ID,
            NOTIFICATION_CHANNEL_NAME,
            notificationManager.IMPORTANCE_DEFAULT);
    channel.setDescription("Channel for showing persistent notification");
    try {
      notificationManager.createNotificationChannel(channel);
    } catch (IllegalArgumentException e) {
      // intentional empty.
    }
  }

  public void deleteChannel() {
    if (Build.VERSION.SDK_INT >= 26) {
      deleteChannelInternalV26();
    }
  }

  @RequiresApi(26)
  private void deleteChannelInternalV26() {
    NotificationManager notificationManager =
        (NotificationManager) this.getSystemService(Context.NOTIFICATION_SERVICE);
    notificationManager.deleteNotificationChannel(NOTIFICATION_CHANNEL_ID);
  }

  Notification buildNotification() {
    NotificationCompat.Builder builder =
        new NotificationCompat.Builder(this, NOTIFICATION_CHANNEL_ID)
            .setShowWhen(false)
            .setPriority(NotificationCompat.PRIORITY_MIN)
            .setSmallIcon(android.R.drawable.stat_sys_warning)
            .setContentTitle("Media playback service")
            .setContentText("Media playback service is running");

    if (VERSION.SDK_INT >= 26) {
      builder.setChannelId(NOTIFICATION_CHANNEL_ID);
    }
    return builder.build();
  }

  @UsedByNative
  protected StarboardBridge getStarboardBridge() {
    if (getApplication() == null) {
      Log.e(TAG, "Application already destroyed.");
      return null;
    }
    return ((StarboardBridge.HostApplication) getApplication()).getStarboardBridge();
  }
}
