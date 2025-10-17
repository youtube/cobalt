// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

import android.accessibilityservice.AccessibilityServiceInfo;
import android.content.Context;
import android.os.Handler;
import android.os.HandlerThread;
import android.speech.tts.TextToSpeech;
import android.view.accessibility.AccessibilityManager;
import dev.cobalt.util.Log;
import java.util.ArrayList;
import java.util.List;
import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

/**
 * Helper class to implement the SbSpeechSynthesis* Starboard API for Audio accessibility.
 *
 * <p>This class is intended to be a singleton in the system. It creates a single static Handler
 * thread in lieu of other synchronization options.
 */
@JNINamespace("starboard")
class CobaltTextToSpeechHelper
    implements TextToSpeech.OnInitListener,
        AccessibilityManager.AccessibilityStateChangeListener,
        AccessibilityManager.TouchExplorationStateChangeListener {
  private final Context mContext;
  private final HandlerThread mThread;
  private final Handler mHandler;

  // The TTS engine should be used only on the background thread.
  private TextToSpeech mTtsEngine;

  private boolean mWasScreenReaderEnabled;

  private enum State {
    PENDING,
    INITIALIZED,
    FAILED
  }

  // These are only accessed inside the Handler Thread
  private State mState = State.PENDING;
  private long mNextUtteranceId;
  private final List<String> mPendingUtterances = new ArrayList<>();

  CobaltTextToSpeechHelper(Context context) {
    this.mContext = context;

    mThread = new HandlerThread("CobaltTextToSpeechHelper");
    mThread.start();
    mHandler = new Handler(mThread.getLooper());

    AccessibilityManager accessibilityManager =
        (AccessibilityManager) context.getSystemService(Context.ACCESSIBILITY_SERVICE);
    mWasScreenReaderEnabled = isScreenReaderEnabled();
    accessibilityManager.addAccessibilityStateChangeListener(this);
    accessibilityManager.addTouchExplorationStateChangeListener(this);
  }

  public void shutdown() {

    mHandler.post(
        new Runnable() {
          @Override
          public void run() {
            if (mTtsEngine != null) {
              mTtsEngine.shutdown();
            }
          }
        });
    mThread.quitSafely();

    AccessibilityManager accessibilityManager =
        (AccessibilityManager) mContext.getSystemService(Context.ACCESSIBILITY_SERVICE);
    accessibilityManager.removeAccessibilityStateChangeListener(this);
    accessibilityManager.removeTouchExplorationStateChangeListener(this);
  }

  /** Returns whether a screen reader is currently enabled */
  @CalledByNative
  public boolean isScreenReaderEnabled() {
    AccessibilityManager am =
        (AccessibilityManager) mContext.getSystemService(Context.ACCESSIBILITY_SERVICE);
    final List<AccessibilityServiceInfo> screenReaders =
        am.getEnabledAccessibilityServiceList(AccessibilityServiceInfo.FEEDBACK_SPOKEN);
    return !screenReaders.isEmpty();
  }

  /** Implementation of TextToSpeech.OnInitListener */
  @Override
  public void onInit(final int status) {
    mHandler.post(
        new Runnable() {
          @Override
          public void run() {
            if (status != TextToSpeech.SUCCESS) {
              Log.e(TAG, "TextToSpeech.onInit failure: " + status);
              mState = State.FAILED;
              return;
            }
            mState = State.INITIALIZED;
            for (String utterance : mPendingUtterances) {
              speak(utterance);
            }
            mPendingUtterances.clear();
          }
        });
  }

  /**
   * Speaks the given text, enqueuing it if something is already speaking. Java-layer implementation
   * of Starboard's SbSpeechSynthesisSpeak.
   */
  void speak(final String text) {
    mHandler.post(
        new Runnable() {
          @Override
          public void run() {

            if (mTtsEngine == null) {
              mTtsEngine = new TextToSpeech(mContext, CobaltTextToSpeechHelper.this);
            }

            switch (mState) {
              case PENDING:
                mPendingUtterances.add(text);
                break;
              case INITIALIZED:
                int success =
                    mTtsEngine.speak(
                        text, TextToSpeech.QUEUE_ADD, null, Long.toString(mNextUtteranceId++));

                if (success != TextToSpeech.SUCCESS) {
                  Log.e(TAG, "TextToSpeech.speak error: " + success);
                  return;
                }
                break;
              case FAILED:
                break;
            }
          }
        });
  }

  /** Cancels all speaking. Java-layer implementation of Starboard's SbSpeechSynthesisCancel. */
  void cancel() {
    mHandler.post(
        new Runnable() {
          @Override
          public void run() {
            if (mTtsEngine != null) {
              mTtsEngine.stop();
            }
            mPendingUtterances.clear();
          }
        });
  }

  @Override
  public void onAccessibilityStateChanged(boolean enabled) {
    // Note that this callback isn't perfect since it only tells us if accessibility was entirely
    // enabled/disabled, but it's better than nothing. For example, it won't be called if the screen
    // reader is enabled/disabled while text magnification remains enabled.
    finishIfScreenReaderChanged();
  }

  @Override
  public void onTouchExplorationStateChanged(boolean enabled) {
    // We also listen for talkback changes because it's the standard (but not only) screen reader,
    // and we can get a better signal than just listening for accessibility being enabled/disabled.
    finishIfScreenReaderChanged();
  }

  /**
   * Quit the app if screen reader settings changed so we respect the new setting the next time the
   * app is run. This should only happen while stopped in the background since the user has to leave
   * the app to change the setting.
   */
  private void finishIfScreenReaderChanged() {
    if (mWasScreenReaderEnabled != isScreenReaderEnabled()) {
      mWasScreenReaderEnabled = isScreenReaderEnabled();
      CobaltTextToSpeechHelperJni.get().sendTTSChangedEvent();
    }
  }

  @NativeMethods
  interface Natives {
    void sendTTSChangedEvent();
  }
}
