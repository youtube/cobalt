package dev.cobalt.coat;

import static dev.cobalt.util.Log.TAG;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.view.KeyEvent;
import androidx.annotation.Nullable;
import dev.cobalt.util.Log;
import org.chromium.content_public.browser.WebContents;

/**
 * VolumeStateReceiver monitors Android media system broadcast events to capture volume changes.
 *
 * This is necessary because physical remote key presses are handled at the Android OS level,
 * bypassing the native focused window key event propagation.
 */
final class VolumeStateReceiver extends BroadcastReceiver {

  public static final String VOLUME_CHANGED_ACTION = "android.media.VOLUME_CHANGED_ACTION";
  public static final String EXTRA_VOLUME_STREAM_VALUE = "android.media.EXTRA_VOLUME_STREAM_VALUE";
  public static final String EXTRA_PREV_VOLUME_STREAM_VALUE =
      "android.media.EXTRA_PREV_VOLUME_STREAM_VALUE";

  public static final String STREAM_MUTE_CHANGED_ACTION =
      "android.media.STREAM_MUTE_CHANGED_ACTION";

  private enum VolumeKeyMapping {
    UP(KeyEvent.KEYCODE_VOLUME_UP, "AudioVolumeUp", 175),
    DOWN(KeyEvent.KEYCODE_VOLUME_DOWN, "AudioVolumeDown", 174),
    MUTE(KeyEvent.KEYCODE_VOLUME_MUTE, "AudioVolumeMute", 173);

    final int androidKeyCode;
    final String domKey;
    final int domKeyCode;

    VolumeKeyMapping(int androidKeyCode, String domKey, int domKeyCode) {
      this.androidKeyCode = androidKeyCode;
      this.domKey = domKey;
      this.domKeyCode = domKeyCode;
    }

    static @Nullable VolumeKeyMapping fromAndroidKeyCode(int code) {
      for (VolumeKeyMapping mapping : values()) {
        if (mapping.androidKeyCode == code) {
          return mapping;
        }
      }
      return null;
    }
  }

  // Reusable parameterized JS injection script evaluated against DOM window targets
  private static final String JS_EVENT_TEMPLATE =
      "(function(k, key) {"
      + "  const target = document.activeElement || document.body || document;"
      + "  for (const type of ['keydown', 'keyup']) {"
      + "    const event = new KeyboardEvent(type, {"
      + "      key: key,"
      + "      code: key,"
      + "      keyCode: k,"
      + "      which: k,"
      + "      bubbles: true,"
      + "      cancelable: true"
      + "    });"
      + "    Object.defineProperties(event, {"
      + "      keyCode: { value: k, enumerable: true },"
      + "      which: { value: k, enumerable: true }"
      + "    });"
      + "    target.dispatchEvent(event);"
      + "  }"
      + "})(%d, '%s');";

  private WebContents mWebContents;

  VolumeStateReceiver(Context appContext) {
    IntentFilter filter = new IntentFilter();
    filter.addAction(VOLUME_CHANGED_ACTION);
    filter.addAction(STREAM_MUTE_CHANGED_ACTION);
    appContext.registerReceiver(this, filter);
  }

  /**
   * Synthesizes custom JavaScript KeyboardEvents and injects them directly into the DOM.
   * Native delivery routes using the IME pipeline fail because interactive focus are not
   * targeted at an editable input node. Web engines drop synthesized system key actions
   * when evaluated inside simple display layouts.
   *
   * To get a signal from volume key events, we need to bypass the suppression and send a
   * payload directly to the JavaScript layer.
   */
  protected void forwardVolumeKeyToJS(int keyCode) {
    VolumeKeyMapping mapping = VolumeKeyMapping.fromAndroidKeyCode(keyCode);
    if (mapping == null) {
      return;
    }

    if (mWebContents == null) {
      Log.w(TAG, "mWebContents not set in VolumeStateReceiver.");
      return;
    }

    String jsPayload = String.format(JS_EVENT_TEMPLATE, mapping.domKeyCode, mapping.domKey);
    Log.i(TAG, "Evaluating synthesized JS event for DOM player: " + mapping.domKey);
    mWebContents.evaluateJavaScript(jsPayload, null);
  }

  public void setWebContents(WebContents webContents) {
    this.mWebContents = webContents;
  }

  @Override
  public void onReceive(Context context, Intent intent) {
    String action = intent.getAction();
    Log.d(TAG, "VolumeStateReceiver intent caught: " + action);

    if (VOLUME_CHANGED_ACTION.equals(action)) {
      int newVolume = intent.getIntExtra(EXTRA_VOLUME_STREAM_VALUE, 0);
      int oldVolume = intent.getIntExtra(EXTRA_PREV_VOLUME_STREAM_VALUE, 0);

      int volumeDelta = newVolume - oldVolume;
      Log.d(TAG, "VolumeStateReceiver capture volume changed, volumeDelta:" + volumeDelta);
      int keyCode = volumeDelta > 0 ? KeyEvent.KEYCODE_VOLUME_UP : KeyEvent.KEYCODE_VOLUME_DOWN;
      forwardVolumeKeyToJS(keyCode);
    } else if (STREAM_MUTE_CHANGED_ACTION.equals(action)) {
      Log.d(TAG, "VolumeStateReceiver capture mute changed.");
      forwardVolumeKeyToJS(KeyEvent.KEYCODE_VOLUME_MUTE);
    }
  }
}
