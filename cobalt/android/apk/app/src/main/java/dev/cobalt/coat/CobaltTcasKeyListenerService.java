package dev.cobalt.coat;

import static dev.cobalt.util.Log.TAG;

import android.content.Context;
import android.os.Bundle;
import android.os.SystemClock;
import android.view.KeyEvent;
import com.google.android.apps.tv.tcas.awareness.IContextEventListener;
import com.google.android.libraries.tv.contextawareness.ContextAwarenessClient;
import com.google.android.libraries.tv.contextawareness.EventConstants;
import dev.cobalt.util.Log;
import org.chromium.content.browser.input.ImeAdapterImpl;
import org.chromium.content_public.browser.WebContents;

/** Service to manage TCAS generic key press and volume key press events for Cobalt. */
public final class CobaltTcasKeyListenerService {
  private final ContextAwarenessClient mContextAwarenessClient;
  private final VolumeStateReceiver mVolumeStateReceiver;
  private WebContents mWebContents;

  private final IContextEventListener mKeyPressListener =
      new IContextEventListener.Stub() {
        @Override
        public void onContextEvent(int eventType, int eventSubtype) {
          // Check for generic key press events if needed in the future
        }

        @Override
        public void onContextEventWithData(int eventType, int eventSubtype, Bundle data) {
          if (eventType == EventConstants.TYPE_KEY_PRESS
              && eventSubtype == EventConstants.SUBTYPE_KEY_PRESS_VOLUME_CHANGE) {
            if (data == null) {
              return;
            }
            for (String key : data.keySet()) {
              Object val = data.get(key);
              Log.i(TAG, "Charley: TCAS Bundle key: '" + key + "' = " + val + " (class: " + (val != null ? val.getClass().getSimpleName() : "null") + ")");
            }

            int volumeEventType = 0;
            if (data.containsKey("direction")) {
              volumeEventType = data.getInt("direction", 0);
            } else if (data.containsKey("volume event type")) {
              volumeEventType = data.getInt("volume event type", 0);
            } else if (data.containsKey("volume_event_type")) {
              volumeEventType = data.getInt("volume_event_type", 0);
            } else if (data.containsKey("volumeEventType")) {
              volumeEventType = data.getInt("volumeEventType", 0);
            } else if (data.containsKey("type")) {
              volumeEventType = data.getInt("type", 0);
            }

            int count = 1;
            if (data.containsKey("aggregated event count")) {
              count = data.getInt("aggregated event count", 1);
            } else if (data.containsKey("aggregated_event_count")) {
              count = data.getInt("aggregated_event_count", 1);
            } else if (data.containsKey("count")) {
              count = data.getInt("count", 1);
            }

            Log.i(TAG, "Charley: TCAS volume change event received, parsed type: " + volumeEventType + ", count: " + count);

            int keyCode = 0;
            // 1: UP, 2: DOWN, 3: MUTE, 4: UNMUTE
            if (volumeEventType == 1) {
              keyCode = KeyEvent.KEYCODE_VOLUME_UP;
            } else if (volumeEventType == 2) {
              keyCode = KeyEvent.KEYCODE_VOLUME_DOWN;
            } else if (volumeEventType == 3 || volumeEventType == 4) {
              keyCode = KeyEvent.KEYCODE_VOLUME_MUTE;
            }

            if (keyCode != 0) {
              Log.i(TAG, "Charley: TCAS dispatching keyCode " + keyCode + " (" + count + " times) to WebContents directly from TCAS service");
              for (int i = 0; i < count; i++) {
                dispatchKeyEvent(keyCode);
              }
            } else {
              Log.w(TAG, "Charley: TCAS did not dispatch event because volumeEventType (" + volumeEventType + ") did not map to a valid volume keyCode.");
            }
          }
        }
      };

  public CobaltTcasKeyListenerService(Context context, VolumeStateReceiver volumeStateReceiver) {
    this.mContextAwarenessClient = new ContextAwarenessClient(context);
    this.mVolumeStateReceiver = volumeStateReceiver;
    this.mContextAwarenessClient.setConnectionListener(
        new ContextAwarenessClient.ConnectionListener() {
          @Override
          public void onConnected() {
            Log.i(TAG, "Charley: TCAS connected, disabling legacy VolumeStateReceiver broadcast handling.");
            if (mVolumeStateReceiver != null) {
              mVolumeStateReceiver.setTcasActive(true);
            }
          }

          @Override
          public void onDisconnected() {
            Log.i(TAG, "Charley: TCAS disconnected, enabling legacy VolumeStateReceiver broadcast handling.");
            if (mVolumeStateReceiver != null) {
              mVolumeStateReceiver.setTcasActive(false);
            }
          }
        });
  }

  /** Starts the TCAS listener. Call this in Activity/Service onStart(). */
  public void start() {
    try {
      mContextAwarenessClient.registerContextEventListener(
          mKeyPressListener,
          EventConstants.TYPE_KEY_PRESS,
          EventConstants.SUBTYPE_KEY_PRESS_ANY);

      mContextAwarenessClient.registerContextEventListener(
          mKeyPressListener,
          EventConstants.TYPE_KEY_PRESS,
          EventConstants.SUBTYPE_KEY_PRESS_VOLUME_CHANGE);
    } catch (Exception e) {
      Log.w(TAG, "Failed to start TCAS listener service", e);
    }
  }

  /** Stops the TCAS listener. Call this in Activity/Service onStop(). */
  public void stop() {
    try {
      if (mContextAwarenessClient != null && mKeyPressListener != null) {
        mContextAwarenessClient.unregisterContextEventListener(
            mKeyPressListener,
            EventConstants.TYPE_KEY_PRESS,
            EventConstants.SUBTYPE_KEY_PRESS_ANY);

        mContextAwarenessClient.unregisterContextEventListener(
            mKeyPressListener,
            EventConstants.TYPE_KEY_PRESS,
            EventConstants.SUBTYPE_KEY_PRESS_VOLUME_CHANGE);
      }
      if (mVolumeStateReceiver != null) {
        mVolumeStateReceiver.setTcasActive(false);
      }
    } catch (Exception e) {
      Log.w(TAG, "Failed to stop TCAS listener service", e);
    }
  }

  public void setWebContents(WebContents webContents) {
    this.mWebContents = webContents;
  }

  private void dispatchKeyEvent(final int keyCode) {
    new android.os.Handler(android.os.Looper.getMainLooper()).post(new Runnable() {
      @Override
      public void run() {
        Log.i(TAG, "Charley: TCAS dispatching both KEY_DOWN and KEY_UP events on Main UI Thread for keyCode: " + keyCode);
        long eventTime = SystemClock.uptimeMillis();
        if (mWebContents == null) {
          Log.w(TAG, "Charley: Cannot dispatch TCAS key event, mWebContents is null");
          return;
        }
        ImeAdapterImpl imeAdapter = ImeAdapterImpl.fromWebContents(mWebContents);
        if (imeAdapter == null) {
          Log.w(TAG, "Charley: Cannot dispatch TCAS key event, imeAdapter is null");
          return;
        }
        Log.i(TAG, "Charley: TCAS dispatching ACTION_DOWN (key down) for keyCode: " + keyCode);
        imeAdapter.dispatchKeyEvent(new KeyEvent(eventTime, eventTime, KeyEvent.ACTION_DOWN, keyCode, 0));
        Log.i(TAG, "Charley: TCAS dispatching ACTION_UP (key up) for keyCode: " + keyCode);
        imeAdapter.dispatchKeyEvent(new KeyEvent(eventTime, eventTime, KeyEvent.ACTION_UP, keyCode, 0));
      }
    });
  }
}
