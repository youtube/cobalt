package dev.cobalt.coat;

import static dev.cobalt.util.Log.TAG;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.util.Log;
import android.media.AudioManager;


/** VolumeStateReceiver monitors Android media broadcast to capture volume button events. */
final class VolumeStateReceiver extends BroadcastReceiver {

  public static final String VOLUME_CHANGED_ACTION = "android.media.VOLUME_CHANGED_ACTION";
  public static final String EXTRA_VOLUME_STREAM_VALUE = "android.media.EXTRA_VOLUME_STREAM_VALUE";
  public static final String EXTRA_PREV_VOLUME_STREAM_VALUE =
      "android.media.EXTRA_PREV_VOLUME_STREAM_VALUE";


  public static final String STREAM_MUTE_CHANGED_ACTION =
      "android.media.STREAM_MUTE_CHANGED_ACTION";

  public static final String EXTRA_VOLUME_STREAM_TYPE =
      "android.media.EXTRA_VOLUME_STREAM_TYPE";
  public static final String EXTRA_STREAM_VOLUME_MUTED =
      "android.media.EXTRA_STREAM_VOLUME_MUTED";

  VolumeStateReceiver(Context appContext) {
    IntentFilter filter = new IntentFilter();
    filter.addAction(VOLUME_CHANGED_ACTION);
    filter.addAction(STREAM_MUTE_CHANGED_ACTION);
    appContext.registerReceiver(this, filter);
  }

  @Override
  public void onReceive(Context context, Intent intent) {
    if (intent.getAction().equals(VOLUME_CHANGED_ACTION)) {
      int newVolume = intent.getIntExtra(EXTRA_VOLUME_STREAM_VALUE, 0);
      int oldVolume = intent.getIntExtra(EXTRA_PREV_VOLUME_STREAM_VALUE, 0);

      int volumeDelta = newVolume - oldVolume;
      Log.d(TAG, "VolumeStateReceiver capture volume changed, volumeDelta:" + volumeDelta);
      nativeVolumeChanged(volumeDelta);
    } else if (intent.getAction().equals(STREAM_MUTE_CHANGED_ACTION)) {

      int streamType  = intent.getIntExtra(EXTRA_VOLUME_STREAM_TYPE, -1);
      boolean isMuted = intent.getBooleanExtra(EXTRA_STREAM_VOLUME_MUTED, false);
      Log.d(TAG, "streamType == "+streamType);
      if(streamType == AudioManager.STREAM_MUSIC){
        Log.d(TAG, "VolumeStateReceiver capture mute changed isMuted == "+isMuted);
        nativeMuteChanged();
      }

    }
  }

  private native void nativeVolumeChanged(int volumeDelta);

  private native void nativeMuteChanged();
}
