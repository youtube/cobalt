// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

package dev.cobalt.testing;

import android.app.Activity;
import android.app.Service;
import android.content.Context;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.view.SurfaceHolder;
import dev.cobalt.coat.BaseStarboardBridge;
import dev.cobalt.media.VideoSurfaceView;
import dev.cobalt.util.DisplayUtil;
import dev.cobalt.util.Holder;
import dev.cobalt.util.Log;
import org.chromium.base.ContextUtils;
import org.chromium.base.JNIUtils;
import org.chromium.native_test.NativeUnitTest;

/**
 * CobaltTestActivity is a custom activity used to run native unit tests on Android, in place of
 * NativeUnitTestActivity.
 *
 * <p>It ensures that the Chromium base library's application context, JNI class loader, and
 * StarboardBridge JNI instance are properly initialized before the native test harness starts.
 *
 * <p>It also provides the punch-out video surface, so that tests can create SbPlayers in punch-out
 * mode with video.
 *
 * <p>Lifetime/Ownership: Created and managed by the Android system's activity lifecycle. It
 * persists for the duration of the native test suite execution.
 *
 * <p>Threading: Must be created and accessed only on the main application thread (UI thread).
 */
public class CobaltTestActivity extends Activity {
  private static final String TAG = "CobaltTestActivity";

  // How long to wait for the video surface
  private static final long VIDEO_SURFACE_TIMEOUT_MS = 5000;

  /**
   * Lightweight TestStarboardBridge subclass used to initialize JNI bindings and AudioOutputManager
   * for native unit tests without starting a duplicate native Starboard main thread.
   */
  private static class TestStarboardBridge extends BaseStarboardBridge {
    public TestStarboardBridge(Context context, Holder<Activity> activityHolder) {
      super(context, activityHolder, new Holder<Service>());
    }
  }

  private final NativeUnitTest mTest = new NativeUnitTest();
  private final Handler mHandler = new Handler(Looper.getMainLooper());
  private TestStarboardBridge mTestStarboardBridge;
  private boolean mVideoSurfaceCreated;
  private boolean mStarted;
  private boolean mTestsPosted;

  @Override
  public void onCreate(Bundle savedInstanceState) {
    ContextUtils.initApplicationContext(getApplicationContext());
    JNIUtils.setDefaultClassLoader(getClassLoader());
    DisplayUtil.cacheDefaultDisplay(this);

    // NativeUnitTest.preCreate loads the shared library into memory
    mTest.preCreate(this);
    super.onCreate(savedInstanceState);
    mTest.postCreate(this);

    // Instantiate TestStarboardBridge after the native library is loaded so native JNI methods are
    // bound.
    Holder<Activity> activityHolder = new Holder<>();
    activityHolder.set(this);
    mTestStarboardBridge = new TestStarboardBridge(getApplicationContext(), activityHolder);
    // Register the bridge as the instance, so VideoSurfaceView can report the video surface to it.
    BaseStarboardBridge.setInstanceForTesting(mTestStarboardBridge);

    // Run the tests once the surface is created.
    VideoSurfaceView videoSurfaceView = new VideoSurfaceView(this);
    videoSurfaceView
        .getHolder()
        .addCallback(
            new SurfaceHolder.Callback() {
              @Override
              public void surfaceCreated(SurfaceHolder holder) {
                mVideoSurfaceCreated = true;
                maybePostTests();
              }

              @Override
              public void surfaceChanged(
                  SurfaceHolder holder, int format, int width, int height) {}

              @Override
              public void surfaceDestroyed(SurfaceHolder holder) {}
            });
    setContentView(videoSurfaceView);
  }

  @Override
  public void onStart() {
    super.onStart();
    mStarted = true;
    mHandler.postDelayed(
        () -> {
          if (!mTestsPosted) {
            Log.w(TAG, "Video surface not created, running tests without it.");
            postTests();
          }
        },
        VIDEO_SURFACE_TIMEOUT_MS);
    maybePostTests();
  }

  private void maybePostTests() {
    if (mStarted && mVideoSurfaceCreated && !mTestsPosted) {
      postTests();
    }
  }

  private void postTests() {
    mTestsPosted = true;
    mTest.postStart(this, true);
  }

  @Override
  public void onDestroy() {
    mHandler.removeCallbacksAndMessages(null);
    // Drop the static reference to the bridge, which holds this activity.
    BaseStarboardBridge.setInstanceForTesting(null);
    super.onDestroy();
  }
}
