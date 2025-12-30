package dev.cobalt.coat;

import static dev.cobalt.util.Log.TAG;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.SystemClock;
import android.view.KeyEvent;
import org.chromium.base.Log;
import org.chromium.content.browser.input.ImeAdapterImpl;
import org.chromium.content_public.browser.WebContents;

/** VolumeStateReceiver monitors Android media broadcast to capture volume button events. */
final class VolumeStateReceiver extends BroadcastReceiver {

  public static final String VOLUME_CHANGED_ACTION = "android.media.VOLUME_CHANGED_ACTION";
  public static final String EXTRA_VOLUME_STREAM_VALUE = "android.media.EXTRA_VOLUME_STREAM_VALUE";
  public static final String EXTRA_PREV_VOLUME_STREAM_VALUE =
      "android.media.EXTRA_PREV_VOLUME_STREAM_VALUE";

  public static final String STREAM_MUTE_CHANGED_ACTION =
      "android.media.STREAM_MUTE_CHANGED_ACTION";

  private WebContents mWebContents;

  VolumeStateReceiver(Context appContext) {
    IntentFilter filter = new IntentFilter();
    filter.addAction(VOLUME_CHANGED_ACTION);
    filter.addAction(STREAM_MUTE_CHANGED_ACTION);
    appContext.registerReceiver(this, filter);
  }

  protected void dispatchKeyDownEvent(int keyCode) {
    long eventTime = SystemClock.uptimeMillis();
    if (mWebContents == null) {
      return;
    }
    ImeAdapterImpl imeAdapter = ImeAdapterImpl.fromWebContents(mWebContents);
    if (imeAdapter == null) {
      return;
    }
    imeAdapter.dispatchKeyEvent(new KeyEvent(eventTime, eventTime, KeyEvent.ACTION_DOWN, keyCode, 0));
  }

  public void setWebContents(WebContents webContents) {
    this.mWebContents = webContents;
  }

  @Override
  public void onReceive(Context context, Intent intent) {
    if (intent.getAction().equals(VOLUME_CHANGED_ACTION)) {
      int newVolume = intent.getIntExtra(EXTRA_VOLUME_STREAM_VALUE, 0);
      int oldVolume = intent.getIntExtra(EXTRA_PREV_VOLUME_STREAM_VALUE, 0);

      int volumeDelta = newVolume - oldVolume;
      Log.d(TAG, "VolumeStateReceiver capture volume changed, volumeDelta:" + volumeDelta);
      int keyCode = volumeDelta > 0 ? KeyEvent.KEYCODE_VOLUME_UP : KeyEvent.KEYCODE_VOLUME_DOWN;
      dispatchKeyDownEvent(keyCode);
    } else if (intent.getAction().equals(STREAM_MUTE_CHANGED_ACTION)) {
      Log.d(TAG, "VolumeStateReceiver capture mute changed.");
      dispatchKeyDownEvent(KeyEvent.KEYCODE_VOLUME_MUTE);
    }
  }
}
