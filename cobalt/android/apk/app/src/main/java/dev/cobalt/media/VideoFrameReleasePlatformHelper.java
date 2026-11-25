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

package dev.cobalt.media;

import static dev.cobalt.media.Log.TAG;

import android.content.Context;
import android.hardware.display.DisplayManager;
import android.os.Build;
import android.os.Handler;
import android.os.Looper;
import android.os.SystemClock;
import android.view.Choreographer;
import android.view.Display;
import android.view.Surface;
import androidx.annotation.Nullable;
import androidx.annotation.RequiresApi;
import dev.cobalt.util.DisplayUtil;
import dev.cobalt.util.Log;
import org.chromium.base.ContextUtils;
import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

/**
 * Standalone platform helper for {@code VideoRenderAlgorithmAndroidV2}.
 *
 * <p>Runs {@link DisplayManager.DisplayListener} and {@link Choreographer} VSYNC sampling on the
 * main looper, votes {@link Surface#setFrameRate}, and pushes VSYNC parameters into C++ atomics so
 * native frame rendering makes zero per-frame JNI calls.
 */
@JNINamespace("starboard")
public final class VideoFrameReleasePlatformHelper implements DisplayManager.DisplayListener {
  private static final long VSYNC_SAMPLE_PERIOD_MS = 500L;
  private static final long VSYNC_OFFSET_PERCENTAGE = 80L;
  private static final long NANOS_PER_SECOND = 1_000_000_000L;
  private static final long TIME_UNSET = -1L;

  @NativeMethods
  interface Natives {
    void onVsyncUpdate(
        long nativeVideoFrameReleasePlatformHelper,
        long sampledVsyncTimeNs,
        long vsyncDurationNs,
        long vsyncOffsetNs);
  }

  private final Object mNativeLock = new Object();
  private long mNativeHelper;
  private volatile boolean mReleased;

  private final Handler mMainHandler = new Handler(Looper.getMainLooper());
  private final Object mScheduleToken = new Object();
  private final boolean mTrackDisplayChanges;
  private final boolean mPreferVsyncCallbackV33;

  @Nullable private final DisplayManager mDisplayManager;
  @Nullable private Choreographer mChoreographer;
  @Nullable private Object mVsyncCallbackV33;
  private final Choreographer.FrameCallback mFrameCallback = this::onLegacyFrame;

  private boolean mChoreographerInitAttempted;
  private boolean mDisplayListenerRegistered;
  private boolean mSamplingActive;
  private long mSampledVsyncTimeNs;
  private long mVsyncDurationNs = TIME_UNSET;

  private final Object mSurfaceLock = new Object();
  @Nullable private Surface mCurrentSurface;
  private float mAppliedFrameRate;
  private boolean mAppliedSeamlessOnly;

  @CalledByNative
  public static VideoFrameReleasePlatformHelper create(
      long nativeHelper,
      boolean trackDisplayChanges,
      boolean preferVsyncCallbackV33,
      boolean startActive) {
    return new VideoFrameReleasePlatformHelper(
        nativeHelper, trackDisplayChanges, preferVsyncCallbackV33, startActive);
  }

  private VideoFrameReleasePlatformHelper(
      long nativeHelper,
      boolean trackDisplayChanges,
      boolean preferVsyncCallbackV33,
      boolean startActive) {
    mNativeHelper = nativeHelper;
    mTrackDisplayChanges = trackDisplayChanges;
    mPreferVsyncCallbackV33 = preferVsyncCallbackV33 && Build.VERSION.SDK_INT >= 33;

    Context appContext = ContextUtils.getApplicationContext();
    mDisplayManager =
        appContext != null
            ? (DisplayManager) appContext.getSystemService(Context.DISPLAY_SERVICE)
            : null;

    // Seed initial display refresh period synchronously so early frames have a valid period
    // before the main-looper Choreographer callback fires.
    long initialDurationNs = queryDefaultDisplayVsyncDurationNs();
    if (initialDurationNs > 0) {
      mVsyncDurationNs = initialDurationNs;
      pushToNative(0L, initialDurationNs);
    }

    runOrPostOnMain(() -> {
      if (mReleased) {
        return;
      }
      if (mTrackDisplayChanges && mDisplayManager != null && !mDisplayListenerRegistered) {
        try {
          mDisplayManager.registerDisplayListener(this, mMainHandler);
          mDisplayListenerRegistered = true;
        } catch (RuntimeException e) {
          Log.w(TAG, "Failed to register DisplayListener", e);
        }
      }
      if (startActive) {
        setSamplingActiveOnMain(true);
      }
    });
  }

  @CalledByNative
  public void setPlaybackActive(boolean active) {
    if (mReleased) {
      return;
    }
    runOrPostOnMain(() -> setSamplingActiveOnMain(active));
  }

  @CalledByNative
  public void updateSurfaceFrameRate(
      @Nullable Object surfaceCandidate, float frameRate, boolean seamlessOnly) {
    if (Build.VERSION.SDK_INT < 30 || mReleased) {
      return;
    }
    Surface newSurface = (surfaceCandidate instanceof Surface) ? (Surface) surfaceCandidate : null;
    synchronized (mSurfaceLock) {
      if (mCurrentSurface != newSurface) {
        clearSurfaceFrameRateLocked();
        mCurrentSurface = newSurface;
      }
      if (mCurrentSurface == null || !mCurrentSurface.isValid()) {
        return;
      }
      float sanitizedRate = (frameRate > 0.0f && Float.isFinite(frameRate)) ? frameRate : 0.0f;
      if (mAppliedFrameRate == sanitizedRate && mAppliedSeamlessOnly == seamlessOnly) {
        return;
      }
      mAppliedFrameRate = sanitizedRate;
      mAppliedSeamlessOnly = seamlessOnly;
      applySurfaceFrameRate(mCurrentSurface, sanitizedRate, seamlessOnly);
    }
  }

  @CalledByNative
  public void clearSurfaceFrameRate() {
    if (Build.VERSION.SDK_INT < 30) {
      return;
    }
    synchronized (mSurfaceLock) {
      clearSurfaceFrameRateLocked();
      mCurrentSurface = null;
      mAppliedSeamlessOnly = false;
    }
  }

  @CalledByNative
  public void release() {
    synchronized (mNativeLock) {
      mReleased = true;
      mNativeHelper = 0L;
    }
    clearSurfaceFrameRate();
    runOrPostOnMain(() -> {
      setSamplingActiveOnMain(false);
      if (mDisplayListenerRegistered && mDisplayManager != null) {
        try {
          mDisplayManager.unregisterDisplayListener(this);
        } catch (RuntimeException e) {
          Log.w(TAG, "Failed to unregister DisplayListener", e);
        }
        mDisplayListenerRegistered = false;
      }
    });
  }

  private void runOrPostOnMain(Runnable runnable) {
    if (Looper.myLooper() == Looper.getMainLooper()) {
      runnable.run();
    } else {
      mMainHandler.post(runnable);
    }
  }

  private void setSamplingActiveOnMain(boolean active) {
    if (mReleased) {
      active = false;
    }
    if (!active) {
      mMainHandler.removeCallbacksAndMessages(mScheduleToken);
      removeChoreographerCallbacksOnMain();
      if (mSamplingActive) {
        mSamplingActive = false;
        mSampledVsyncTimeNs = 0L;
        pushToNative(TIME_UNSET, mVsyncDurationNs);
      }
      return;
    }
    if (mSamplingActive) {
      return;
    }
    if (!ensureChoreographerOnMain()) {
      return;
    }
    mSamplingActive = true;
    mMainHandler.removeCallbacksAndMessages(mScheduleToken);
    removeChoreographerCallbacksOnMain();
    // Refresh display period on resume in case a mode change happened while paused.
    if (mTrackDisplayChanges) {
      long refreshedDurationNs = queryDefaultDisplayVsyncDurationNs();
      if (refreshedDurationNs > 0) {
        mVsyncDurationNs = refreshedDurationNs;
      }
    }
    postImmediateVsyncSampleOnMain();
  }

  private boolean ensureChoreographerOnMain() {
    if (mChoreographer != null) {
      return true;
    }
    if (mChoreographerInitAttempted) {
      return false;
    }
    mChoreographerInitAttempted = true;
    try {
      mChoreographer = Choreographer.getInstance();
      if (mPreferVsyncCallbackV33 && Build.VERSION.SDK_INT >= 33) {
        mVsyncCallbackV33 = Api33.createVsyncCallback(this);
      }
      return true;
    } catch (RuntimeException e) {
      // Catch platform Choreographer initialization errors (see b/213926330).
      Log.w(TAG, "Disabling VSYNC sampling due to Choreographer error", e);
      mChoreographer = null;
      return false;
    }
  }

  private void postImmediateVsyncSampleOnMain() {
    if (mChoreographer == null || !mSamplingActive || mReleased) {
      return;
    }
    try {
      if (mVsyncCallbackV33 != null && Build.VERSION.SDK_INT >= 33) {
        Api33.postVsyncCallback(mChoreographer, mVsyncCallbackV33);
      } else {
        mChoreographer.postFrameCallback(mFrameCallback);
      }
    } catch (RuntimeException e) {
      // Fall back cleanly if VsyncCallback fails at runtime on a vendor ROM.
      mVsyncCallbackV33 = null;
      try {
        mChoreographer.postFrameCallback(mFrameCallback);
      } catch (RuntimeException fallbackError) {
        Log.w(TAG, "Failed to post Choreographer callback", fallbackError);
      }
    }
  }

  private void removeChoreographerCallbacksOnMain() {
    if (mChoreographer == null) {
      return;
    }
    try {
      if (mVsyncCallbackV33 != null && Build.VERSION.SDK_INT >= 33) {
        Api33.removeVsyncCallback(mChoreographer, mVsyncCallbackV33);
      }
      mChoreographer.removeFrameCallback(mFrameCallback);
    } catch (RuntimeException e) {
      Log.w(TAG, "Failed to remove Choreographer callback", e);
    }
  }

  private void scheduleNextDelayedSampleOnMain() {
    mMainHandler.removeCallbacksAndMessages(mScheduleToken);
    if (!mSamplingActive || mReleased) {
      return;
    }
    mMainHandler.postAtTime(
        this::postImmediateVsyncSampleOnMain,
        mScheduleToken,
        SystemClock.uptimeMillis() + VSYNC_SAMPLE_PERIOD_MS);
  }

  private void onLegacyFrame(long vsyncTimeNs) {
    if (!mSamplingActive || mReleased) {
      return;
    }
    mSampledVsyncTimeNs = vsyncTimeNs;
    if (mVsyncDurationNs <= 0 || mTrackDisplayChanges) {
      long displayDurationNs = queryDefaultDisplayVsyncDurationNs();
      if (displayDurationNs > 0) {
        mVsyncDurationNs = displayDurationNs;
      }
    }
    if (mVsyncDurationNs > 0) {
      pushToNative(mSampledVsyncTimeNs, mVsyncDurationNs);
    }
    scheduleNextDelayedSampleOnMain();
  }

  void onVsyncFrameDataV33(long frameTimeNanos, long timelineDurationNs) {
    if (!mSamplingActive || mReleased) {
      return;
    }
    mSampledVsyncTimeNs = frameTimeNanos;
    if (timelineDurationNs > 0) {
      mVsyncDurationNs = timelineDurationNs;
    } else {
      long fallbackDurationNs = queryDefaultDisplayVsyncDurationNs();
      if (fallbackDurationNs > 0) {
        mVsyncDurationNs = fallbackDurationNs;
      }
    }
    if (mVsyncDurationNs > 0) {
      pushToNative(mSampledVsyncTimeNs, mVsyncDurationNs);
    }
    scheduleNextDelayedSampleOnMain();
  }

  @Override
  public void onDisplayAdded(int displayId) {}

  @Override
  public void onDisplayRemoved(int displayId) {}

  @Override
  public void onDisplayChanged(int displayId) {
    if (displayId != Display.DEFAULT_DISPLAY || mReleased || !mTrackDisplayChanges) {
      return;
    }
    long updatedDurationNs = queryDefaultDisplayVsyncDurationNs();
    if (updatedDurationNs > 0 && updatedDurationNs != mVsyncDurationNs) {
      mVsyncDurationNs = updatedDurationNs;
      mSampledVsyncTimeNs = 0L;
      // Clear the pre-switch VSYNC phase anchor until postImmediateVsyncSampleOnMain()
      // delivers a sample from the new display mode.
      pushToNative(TIME_UNSET, mVsyncDurationNs);
    }
    if (mSamplingActive && ensureChoreographerOnMain()) {
      mMainHandler.removeCallbacksAndMessages(mScheduleToken);
      removeChoreographerCallbacksOnMain();
      postImmediateVsyncSampleOnMain();
    }
  }

  private long queryDefaultDisplayVsyncDurationNs() {
    Display display =
        mDisplayManager != null ? mDisplayManager.getDisplay(Display.DEFAULT_DISPLAY) : null;
    double refreshRate =
        display != null ? display.getRefreshRate() : DisplayUtil.getDefaultDisplayRefreshRate();
    if (refreshRate >= 10.0 && refreshRate <= 1000.0 && Double.isFinite(refreshRate)) {
      return Math.round((double) NANOS_PER_SECOND / refreshRate);
    }
    return TIME_UNSET;
  }

  private void pushToNative(long sampledVsyncTimeNs, long vsyncDurationNs) {
    long vsyncOffsetNs =
        vsyncDurationNs > 0 ? (vsyncDurationNs * VSYNC_OFFSET_PERCENTAGE) / 100L : TIME_UNSET;
    synchronized (mNativeLock) {
      if (mNativeHelper != 0L && !mReleased) {
        VideoFrameReleasePlatformHelperJni.get()
            .onVsyncUpdate(mNativeHelper, sampledVsyncTimeNs, vsyncDurationNs, vsyncOffsetNs);
      }
    }
  }

  private void clearSurfaceFrameRateLocked() {
    if (Build.VERSION.SDK_INT < 30
        || mCurrentSurface == null
        || mAppliedFrameRate == 0.0f
        || !mCurrentSurface.isValid()) {
      mAppliedFrameRate = 0.0f;
      return;
    }
    boolean seamlessOnly = mAppliedSeamlessOnly;
    mAppliedFrameRate = 0.0f;
    applySurfaceFrameRate(mCurrentSurface, 0.0f, seamlessOnly);
  }

  private static void applySurfaceFrameRate(
      Surface surface, float frameRate, boolean seamlessOnly) {
    if (Build.VERSION.SDK_INT >= 31) {
      Api31.setSurfaceFrameRate(surface, frameRate, seamlessOnly);
    } else if (Build.VERSION.SDK_INT >= 30) {
      Api30.setSurfaceFrameRate(surface, frameRate);
    }
  }

  @RequiresApi(30)
  private static final class Api30 {
    static void setSurfaceFrameRate(Surface surface, float frameRate) {
      int compatibility =
          frameRate == 0.0f
              ? Surface.FRAME_RATE_COMPATIBILITY_DEFAULT
              : Surface.FRAME_RATE_COMPATIBILITY_FIXED_SOURCE;
      try {
        surface.setFrameRate(frameRate, compatibility);
      } catch (RuntimeException e) {
        Log.w(TAG, "Failed to call Surface.setFrameRate (API 30)", e);
      }
    }
  }

  @RequiresApi(31)
  private static final class Api31 {
    static void setSurfaceFrameRate(Surface surface, float frameRate, boolean seamlessOnly) {
      int compatibility =
          frameRate == 0.0f
              ? Surface.FRAME_RATE_COMPATIBILITY_DEFAULT
              : Surface.FRAME_RATE_COMPATIBILITY_FIXED_SOURCE;
      int changeStrategy =
          seamlessOnly
              ? Surface.CHANGE_FRAME_RATE_ONLY_IF_SEAMLESS
              : Surface.CHANGE_FRAME_RATE_ALWAYS;
      try {
        surface.setFrameRate(frameRate, compatibility, changeStrategy);
      } catch (RuntimeException e) {
        Log.w(TAG, "Failed to call Surface.setFrameRate (API 31)", e);
      }
    }
  }

  @RequiresApi(33)
  private static final class Api33 {
    static Object createVsyncCallback(VideoFrameReleasePlatformHelper owner) {
      return (Choreographer.VsyncCallback)
          data -> {
            long frameTimeNs = data.getFrameTimeNanos();
            long timelineDurationNs = TIME_UNSET;
            Choreographer.FrameTimeline[] timelines = data.getFrameTimelines();
            if (timelines != null && timelines.length >= 2) {
              long deltaNs =
                  timelines[1].getExpectedPresentationTimeNanos()
                      - timelines[0].getExpectedPresentationTimeNanos();
              if (deltaNs >= 1_000_000L && deltaNs <= 100_000_000L) {
                timelineDurationNs = deltaNs;
              }
            }
            owner.onVsyncFrameDataV33(frameTimeNs, timelineDurationNs);
          };
    }

    static void postVsyncCallback(Choreographer choreographer, Object callback) {
      choreographer.postVsyncCallback((Choreographer.VsyncCallback) callback);
    }

    static void removeVsyncCallback(Choreographer choreographer, Object callback) {
      choreographer.removeVsyncCallback((Choreographer.VsyncCallback) callback);
    }
  }
}
