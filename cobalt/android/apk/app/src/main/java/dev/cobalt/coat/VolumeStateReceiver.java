package dev.cobalt.coat;

import static dev.cobalt.util.Log.TAG;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.util.Log;

/** VolumeStateReceiver monitors Android media broadcast to capture volume button events. */
final class VolumeStateReceiver extends BroadcastReceiver {

  public static final String VOLUME_CHANGED_ACTION = "android.media.VOLUME_CHANGED_ACTION";
  public static final String EXTRA_VOLUME_STREAM_VALUE = "android.media.EXTRA_VOLUME_STREAM_VALUE";
  public static final String EXTRA_PREV_VOLUME_STREAM_VALUE =
      "android.media.EXTRA_PREV_VOLUME_STREAM_VALUE";

  public static final String STREAM_MUTE_CHANGED_ACTION =
      "android.media.STREAM_MUTE_CHANGED_ACTION";

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
      Log.d(TAG, "VolumeStateReceiver capture mute changed.");
      nativeMuteChanged();
    }
  }

  private native void nativeVolumeChanged(int volumeDelta);

  private native void nativeMuteChanged();
}
