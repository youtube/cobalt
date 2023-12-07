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
import android.content.pm.ServiceInfo;
import android.os.Build.VERSION;
import android.os.IBinder;
import android.os.RemoteException;
import androidx.annotation.RequiresApi;
import androidx.core.app.NotificationCompat;
import dev.cobalt.util.Log;
import dev.cobalt.util.UsedByNative;

/** Implementation of the MediaPlaybackService used for Background mode media playing. */
public class MediaPlaybackService extends Service {

  private static final int NOTIFICATION_ID = 193266736; // CL number for uniqueness.
  private static final String NOTIFICATION_CHANNEL_ID = "dev.cobalt.coat media playback service";
  private static final String NOTIFICATION_CHANNEL_NAME = "Media playback service";
  private boolean channelCreated = true;
  private NotificationManager notificationManager = null;

  @Override
  public void onCreate() {
    Log.i(TAG, "Creating a Media playback foreground service.");
    super.onCreate();
    if (getStarboardBridge() != null) {
      getStarboardBridge().onServiceStart(this);
    }
    this.notificationManager =
        (NotificationManager) this.getSystemService(Context.NOTIFICATION_SERVICE);
    this.channelCreated = createNotificationChannel();
  }

  @Override
  public int onStartCommand(Intent intent, int flags, int startId) {
    Log.i(TAG, "Cold start - Starting the service.");
    startService();
    // It is better for background media playback service.
    return START_STICKY;
  }

  @Override
  public IBinder onBind(Intent intent) {
    // Do not support binding.
    return null;
  }

  @Override
  public void onDestroy() {
    Log.i(TAG, "Destroying the Media playback service.");

    if (VERSION.SDK_INT >= 26 && this.channelCreated) {
      this.notificationManager.deleteNotificationChannel(NOTIFICATION_CHANNEL_ID);
    }

    if (getStarboardBridge() != null) {
      getStarboardBridge().onServiceDestroy(this);
    }
    super.onDestroy();
  }

  public void startService() {
    if (this.channelCreated) {
      try {
        if (VERSION.SDK_INT >= 29) {
          startForeground(
              NOTIFICATION_ID, buildNotification(), ServiceInfo.FOREGROUND_SERVICE_TYPE_MANIFEST);
        } else {
          startForeground(NOTIFICATION_ID, buildNotification());
        }
      } catch (IllegalStateException e) {
        Log.e(TAG, "Failed to start Foreground Service", e);
      }
    }
  }

  public void stopService() {
    // Let service itself handle notification deletion.
    if (this.channelCreated) {
      stopForeground(true);
    }
    stopSelf();
  }

  private boolean createNotificationChannel() {
    if (VERSION.SDK_INT >= 26) {
      try {
        createNotificationChannelInternalV26();
      } catch (RemoteException e) {
        Log.e(TAG, "Failed to create Notification Channel.", e);
        return false;
      }
    }
    return true;
  }

  @RequiresApi(26)
  private void createNotificationChannelInternalV26() throws RemoteException {
    NotificationChannel channel =
        new NotificationChannel(
            NOTIFICATION_CHANNEL_ID,
            NOTIFICATION_CHANNEL_NAME,
            notificationManager.IMPORTANCE_DEFAULT);
    channel.setDescription("Channel for showing persistent notification");
    this.notificationManager.createNotificationChannel(channel);
  }

  Notification buildNotification() {
    String channelId = "";
    if (VERSION.SDK_INT >= 26) {
      // Channel with ID=NOTIFICATION_CHANNEL_ID is created for version >= 26
      channelId = NOTIFICATION_CHANNEL_ID;
    }

    NotificationCompat.Builder builder =
        new NotificationCompat.Builder(this, channelId)
            .setShowWhen(false)
            .setPriority(NotificationCompat.PRIORITY_MIN)
            .setSmallIcon(android.R.drawable.stat_sys_warning)
            .setContentTitle("Media playback service")
            .setContentText("Media playback service is running");

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
