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

import android.Manifest;
import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.speech.RecognitionListener;
import android.speech.RecognizerIntent;
import android.speech.SpeechRecognizer;
import android.support.v4.app.ActivityCompat;
import android.support.v4.content.ContextCompat;
import dev.cobalt.util.Holder;
import dev.cobalt.util.UsedByNative;
import java.util.ArrayList;

/**
 * This class uses Android's SpeechRecognizer to perform speech recognition. Using Android's
 * platform recognizer offers several benefits, like good good quality and good local fallback when
 * no data connection is available.
 */
public class VoiceRecognizer {
  private final Context context;
  private final Holder<Activity> activityHolder;
  private final Handler mainHandler = new Handler(Looper.getMainLooper());
  private SpeechRecognizer speechRecognizer;

  // Native pointer to C++ SbSpeechRecognizerImpl.
  private long nativeSpeechRecognizerImpl;

  // Remember if we are using continuous recognition.
  private boolean continuous;
  private boolean interimResults;
  private int maxAlternatives;

  private volatile boolean requestAudioPermissionStarted;

  // Internal class to handle events from Android's SpeechRecognizer and route
  // them to native.
  class Listener implements RecognitionListener {
    @Override
    public void onBeginningOfSpeech() {
      nativeOnSpeechDetected(nativeSpeechRecognizerImpl, true);
    }

    @Override
    public void onBufferReceived(byte[] buffer) {}

    @Override
    public void onEndOfSpeech() {
      nativeOnSpeechDetected(nativeSpeechRecognizerImpl, false);
    }

    @Override
    public void onError(int error) {
      nativeOnError(nativeSpeechRecognizerImpl, error);
      reset();
    }

    @Override
    public void onEvent(int eventType, Bundle params) {}

    @Override
    public void onPartialResults(Bundle bundle) {
      handleResults(bundle, false);
    }

    @Override
    public void onReadyForSpeech(Bundle params) {}

    @Override
    public void onResults(Bundle bundle) {
      handleResults(bundle, true);
      reset();
    }

    @Override
    public void onRmsChanged(float rmsdB) {}

    private void handleResults(Bundle bundle, boolean isFinal) {
      ArrayList<String> list = bundle.getStringArrayList(SpeechRecognizer.RESULTS_RECOGNITION);
      String[] results = list.toArray(new String[list.size()]);
      float[] scores = bundle.getFloatArray(SpeechRecognizer.CONFIDENCE_SCORES);
      nativeOnResults(nativeSpeechRecognizerImpl, results, scores, isFinal);
    }
  };

  public VoiceRecognizer(Context context, Holder<Activity> activityHolder) {
    this.context = context;
    this.activityHolder = activityHolder;
  }

  @SuppressWarnings("unused")
  @UsedByNative
  public void startRecognition(
      boolean continuous,
      boolean interimResults,
      int maxAlternatives,
      long nativeSpeechRecognizerImpl) {
    this.continuous = continuous;
    this.interimResults = interimResults;
    this.maxAlternatives = maxAlternatives;
    this.nativeSpeechRecognizerImpl = nativeSpeechRecognizerImpl;

    if (requestRecordAudioPermission()) {
      startRecognitionInternal();
    }
  }

  @SuppressWarnings("unused")
  @UsedByNative
  public void stopRecognition() {
    Runnable runnable =
        new Runnable() {
          @Override
          public void run() {
            if (Looper.myLooper() != Looper.getMainLooper()) {
              throw new RuntimeException("Must be called in main thread.");
            }
            if (speechRecognizer == null) {
              return;
            }
            reset();
          }
        };
    mainHandler.post(runnable);
  }

  /**
   * Request for the RECORD_AUDIO permission. Returns true if the permission is granted and returns
   * false if the permission is not granted at this moment and starting to request RECORD_AUDIO
   * permission.
   */
  public synchronized boolean requestRecordAudioPermission() {
    Activity activity = activityHolder.get();
    if (activity == null) {
      mainHandler.post(new Runnable() {
        @Override
        public void run() {
          nativeOnError(
              nativeSpeechRecognizerImpl, SpeechRecognizer.ERROR_INSUFFICIENT_PERMISSIONS);
        }
      });
      return false;
    }

    if (ContextCompat.checkSelfPermission(activity, Manifest.permission.RECORD_AUDIO)
        != PackageManager.PERMISSION_GRANTED) {
      if (!requestAudioPermissionStarted) {
        ActivityCompat.requestPermissions(
            activity, new String[] {Manifest.permission.RECORD_AUDIO}, R.id.rc_record_audio);
        requestAudioPermissionStarted = true;
      }
      return false;
    } else {
      return true;
    }
  }

  /** Handles the RECORD_AUDIO request result. */
  public synchronized void onRequestPermissionsResult(
      int requestCode, String[] permissions, int[] grantResults) {
    if (requestCode == R.id.rc_record_audio) {
      // If request is cancelled, the result arrays are empty.
      if (grantResults.length > 0 && grantResults[0] == PackageManager.PERMISSION_GRANTED) {
        // Permission was granted.
        startRecognitionInternal();
      } else {
        // Permission denied.
        nativeOnError(nativeSpeechRecognizerImpl, SpeechRecognizer.ERROR_INSUFFICIENT_PERMISSIONS);
      }
      requestAudioPermissionStarted = false;
    }
  }

  private void startRecognitionInternal() {
    Runnable runnable =
        new Runnable() {
          @Override
          public void run() {
            if (Looper.myLooper() != Looper.getMainLooper()) {
              throw new RuntimeException("Must be called in main thread.");
            }
            speechRecognizer =
                SpeechRecognizer.createSpeechRecognizer(context);
            speechRecognizer.setRecognitionListener(new Listener());
            Intent intent = new Intent(RecognizerIntent.ACTION_RECOGNIZE_SPEECH);
            intent.putExtra(
                RecognizerIntent.EXTRA_LANGUAGE_MODEL, RecognizerIntent.LANGUAGE_MODEL_FREE_FORM);
            intent.putExtra("android.speech.extra.DICTATION_MODE", continuous);
            intent.putExtra(RecognizerIntent.EXTRA_PARTIAL_RESULTS, interimResults);
            intent.putExtra(RecognizerIntent.EXTRA_MAX_RESULTS, maxAlternatives);
            speechRecognizer.startListening(intent);
          }
        };
    mainHandler.post(runnable);
  }

  private void reset() {
    speechRecognizer.destroy();
    speechRecognizer = null;

    nativeSpeechRecognizerImpl = 0;
    continuous = false;
    interimResults = false;
    maxAlternatives = 1;
  }

  private native void nativeOnSpeechDetected(long nativeSpeechRecognizerImpl, boolean detected);

  private native void nativeOnError(long nativeSpeechRecognizerImpl, int error);

  private native void nativeOnResults(
      long nativeSpeechRecognizerImpl, String[] results, float[] confidences, boolean isFinal);
}
