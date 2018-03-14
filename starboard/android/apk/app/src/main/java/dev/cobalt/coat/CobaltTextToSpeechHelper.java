// Copyright 2017 Google Inc. All Rights Reserved.
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
import android.util.Log;
import android.view.accessibility.AccessibilityManager;
import dev.cobalt.util.UsedByNative;
import java.util.ArrayList;
import java.util.List;

/**
 * Helper class to implement the SbSpeechSynthesis* Starboard API for Audio accessiblity.
 *
 * <p>This class is intended to be a singleton in the system. It creates a single static Handler
 * thread in lieu of other synchronization options.
 */
class CobaltTextToSpeechHelper implements
    TextToSpeech.OnInitListener,
    AccessibilityManager.AccessibilityStateChangeListener,
    AccessibilityManager.TouchExplorationStateChangeListener {
  private final Context context;
  private final Runnable stopRequester;
  private final HandlerThread thread;
  private final Handler handler;
  private final TextToSpeech ttsEngine;

  private boolean wasScreenReaderEnabled;

  private enum State {
    PENDING,
    INITIALIZED,
    FAILED
  }

  // These are only accessed inside the Handler Thread
  private State state = State.PENDING;
  private long nextUtteranceId;
  private final List<String> pendingUtterances = new ArrayList<>();

  CobaltTextToSpeechHelper(Context context, Runnable stopRequester) {
    this.context = context;
    this.stopRequester = stopRequester;

    thread = new HandlerThread("CobaltTextToSpeechHelper");
    thread.start();
    handler = new Handler(thread.getLooper());
    ttsEngine = new TextToSpeech(context, this);

    AccessibilityManager accessibilityManager =
        (AccessibilityManager) context.getSystemService(Context.ACCESSIBILITY_SERVICE);
    wasScreenReaderEnabled = isScreenReaderEnabled();
    accessibilityManager.addAccessibilityStateChangeListener(this);
    accessibilityManager.addTouchExplorationStateChangeListener(this);
  }

  public void shutdown() {
    ttsEngine.shutdown();
    thread.quit();

    AccessibilityManager accessibilityManager =
        (AccessibilityManager) context.getSystemService(Context.ACCESSIBILITY_SERVICE);
    accessibilityManager.removeAccessibilityStateChangeListener(this);
    accessibilityManager.removeTouchExplorationStateChangeListener(this);
  }

  /** Returns whether a screen reader is currently enabled */
  @SuppressWarnings("unused")
  @UsedByNative
  public boolean isScreenReaderEnabled() {
    AccessibilityManager am =
        (AccessibilityManager) context.getSystemService(Context.ACCESSIBILITY_SERVICE);
    final List<AccessibilityServiceInfo> screenReaders =
        am.getEnabledAccessibilityServiceList(AccessibilityServiceInfo.FEEDBACK_SPOKEN);
    return !screenReaders.isEmpty();
  }

  /** Implementation of TextToSpeech.OnInitListener */
  @Override
  public void onInit(final int status) {
    handler.post(
        new Runnable() {
          @Override
          public void run() {
            if (status != TextToSpeech.SUCCESS) {
              Log.e(TAG, "TextToSpeech.onInit failure: " + status);
              state = State.FAILED;
              return;
            }
            state = State.INITIALIZED;
            for (String utterance : pendingUtterances) {
              speak(utterance);
            }
            pendingUtterances.clear();
          }
        });
  }

  /**
   * Speaks the given text, enqueuing it if something is already speaking.
   * Java-layer implementation of Starboard's SbSpeechSynthesisSpeak.
   */
  @SuppressWarnings("unused")
  @UsedByNative
  void speak(final String text) {
    handler.post(
        new Runnable() {
          @Override
          public void run() {
            switch (state) {
              case PENDING:
                pendingUtterances.add(text);
                break;
              case INITIALIZED:
                int success =
                    ttsEngine.speak(
                        text, TextToSpeech.QUEUE_ADD, null, Long.toString(nextUtteranceId++));

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

  /**
   * Cancels all speaking.
   * Java-layer implementation of Starboard's SbSpeechSynthesisCancel.
   */
  @SuppressWarnings("unused")
  @UsedByNative
  void cancel() {
    handler.post(
        new Runnable() {
          @Override
          public void run() {
            ttsEngine.stop();
            pendingUtterances.clear();
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
    if (wasScreenReaderEnabled != isScreenReaderEnabled()) {
      stopRequester.run();
    }
  }
}
