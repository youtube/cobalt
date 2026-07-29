/*
 * Copyright (C) 2016 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
// Modifications Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

import android.os.Handler;
import android.os.HandlerThread;
import android.os.Message;
import android.view.Choreographer;
import android.view.Choreographer.FrameCallback;
import dev.cobalt.util.DisplayUtil;
import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

/** Makes a best effort to adjust frame release timestamps for a smoother visual result. */
@JNINamespace("starboard")
public final class VideoFrameReleaseTimeHelper {

  private static final long CHOREOGRAPHER_SAMPLE_DELAY_MILLIS = 500;
  private static final long MAX_ALLOWED_DRIFT_NS = 20000000;

  private static final long VSYNC_OFFSET_PERCENTAGE = 80;
  private static final int MIN_FRAMES_FOR_ADJUSTMENT = 6;
  private static final long NANOS_PER_SECOND = 1000000000L;

  private final VSyncSampler mVsyncSampler;
  private final boolean mUseDefaultDisplayVsync;
  private final long mVsyncDurationNs;
  private final long mVsyncOffsetNs;

  private long mLastFramePresentationTimeUs;
  private long mAdjustedLastFrameTimeNs;
  private long mPendingAdjustedFrameTimeNs;
  private double mLastPlaybackRate;

  private boolean mHaveSync;
  private long mSyncUnadjustedReleaseTimeNs;
  private long mSyncFramePresentationTimeNs;
  private long mFrameCount;

  /**
   * Constructs an instance that smooths frame release timestamps but does not align them with the
   * default display's vsync signal.
   */
  @CalledByNative
  public VideoFrameReleaseTimeHelper() {
    this(DisplayUtil.getDefaultDisplayRefreshRate());
  }

  private VideoFrameReleaseTimeHelper(double defaultDisplayRefreshRate) {
    mUseDefaultDisplayVsync = defaultDisplayRefreshRate != DisplayUtil.DISPLAY_REFRESH_RATE_UNKNOWN;
    if (mUseDefaultDisplayVsync) {
      mVsyncSampler = VSyncSampler.getInstance();
      mVsyncDurationNs = (long) (NANOS_PER_SECOND / defaultDisplayRefreshRate);
      mVsyncOffsetNs = (mVsyncDurationNs * VSYNC_OFFSET_PERCENTAGE) / 100;
    } else {
      mVsyncSampler = null;
      mVsyncDurationNs = -1; // Value unused.
      mVsyncOffsetNs = -1; // Value unused.
    }
  }

  /** Enables the helper. */
  @CalledByNative
  public void enable() {
    mHaveSync = false;
    mLastPlaybackRate = -1;
    if (mUseDefaultDisplayVsync) {
      mVsyncSampler.addObserver();
    }
  }

  /** Disables the helper. */
  @CalledByNative
  public void disable() {
    if (mUseDefaultDisplayVsync) {
      mVsyncSampler.removeObserver();
    }
  }

  /**
   * Adjusts a frame release timestamp.
   *
   * @param framePresentationTimeUs The frame's presentation time, in microseconds.
   * @param unadjustedReleaseTimeNs The frame's unadjusted release time, in nanoseconds and in the
   *     same time base as {@link System#nanoTime()}.
   * @return The adjusted frame release timestamp, in nanoseconds and in the same time base as
   *     {@link System#nanoTime()}.
   */
  @CalledByNative
  public long adjustReleaseTime(
      long framePresentationTimeUs, long unadjustedReleaseTimeNs, double playbackRate) {
    if (playbackRate == 0) {
      return unadjustedReleaseTimeNs;
    }
    if (playbackRate != mLastPlaybackRate) {
      // Resync if playback rate has changed.
      mHaveSync = false;
      mLastPlaybackRate = playbackRate;
    }

    long framePresentationTimeNs = framePresentationTimeUs * 1000;

    // Until we know better, the adjustment will be a no-op.
    long adjustedFrameTimeNs = framePresentationTimeNs;
    long adjustedReleaseTimeNs = unadjustedReleaseTimeNs;

    if (mHaveSync) {
      // See if we've advanced to the next frame.
      if (framePresentationTimeUs != mLastFramePresentationTimeUs) {
        mFrameCount++;
        mAdjustedLastFrameTimeNs = mPendingAdjustedFrameTimeNs;
      }
      if (mFrameCount >= MIN_FRAMES_FOR_ADJUSTMENT) {
        // We're synced and have waited the required number of frames to apply an adjustment.
        // Calculate the average frame time across all the frames we've seen since the last sync.
        // This will typically give us a frame rate at a finer granularity than the frame times
        // themselves (which often only have millisecond granularity).
        long averageFrameDurationNs =
            (framePresentationTimeNs - mSyncFramePresentationTimeNs) / mFrameCount;
        // Project the adjusted frame time forward using the average.
        long candidateAdjustedFrameTimeNs = mAdjustedLastFrameTimeNs + averageFrameDurationNs;
        if (isDriftTooLarge(candidateAdjustedFrameTimeNs, unadjustedReleaseTimeNs, playbackRate)) {
          mHaveSync = false;
        } else {
          adjustedFrameTimeNs = candidateAdjustedFrameTimeNs;
          adjustedReleaseTimeNs =
              mSyncUnadjustedReleaseTimeNs
                  + adjustFrameTimeForPlaybackRate(
                      adjustedFrameTimeNs - mSyncFramePresentationTimeNs, playbackRate);
        }
      } else {
        // We're synced but haven't waited the required number of frames to apply an adjustment.
        // Check drift anyway.
        if (isDriftTooLarge(framePresentationTimeNs, unadjustedReleaseTimeNs, playbackRate)) {
          mHaveSync = false;
        }
      }
    }

    // If we need to sync, do so now.
    if (!mHaveSync) {
      mSyncFramePresentationTimeNs = framePresentationTimeNs;
      mSyncUnadjustedReleaseTimeNs = unadjustedReleaseTimeNs;
      mFrameCount = 0;
      mHaveSync = true;
      onSynced();
    }

    mLastFramePresentationTimeUs = framePresentationTimeUs;
    mPendingAdjustedFrameTimeNs = adjustedFrameTimeNs;

    if (mVsyncSampler == null || mVsyncSampler.mSampledVsyncTimeNs == 0) {
      return adjustedReleaseTimeNs;
    }

    // Find the timestamp of the closest vsync. This is the vsync that we're targeting.
    long snappedTimeNs =
        closestVsync(adjustedReleaseTimeNs, mVsyncSampler.mSampledVsyncTimeNs, mVsyncDurationNs);
    // Apply an offset so that we release before the target vsync, but after the previous one.
    return snappedTimeNs - mVsyncOffsetNs;
  }

  protected void onSynced() {
    // Do nothing.
  }

  private boolean isDriftTooLarge(long frameTimeNs, long releaseTimeNs, double playbackRate) {
    long elapsedFrameTimeNs = frameTimeNs - mSyncFramePresentationTimeNs;
    long elapsedReleaseTimeNs = releaseTimeNs - mSyncUnadjustedReleaseTimeNs;
    return Math.abs(
            elapsedReleaseTimeNs - adjustFrameTimeForPlaybackRate(elapsedFrameTimeNs, playbackRate))
        > MAX_ALLOWED_DRIFT_NS;
  }

  private long adjustFrameTimeForPlaybackRate(long frameTimeNs, double playbackRate) {
    if (playbackRate == 1.0 || playbackRate == 0.0) {
      return frameTimeNs;
    }
    // YT playback rate can be set to multiples of 0.25, ranging from 0.25 to 2.0 (e.g., 0.25, 0.5,
    // 0.75, 1.0, 1.25, 1.5, 1.75, and 2.0). To prevent issues with floating-point conversions, the
    // playback rate is converted to a long integer by multiplying it by 4.
    final long kPlaybackRateAdjustmentMultiplier = 4;
    long adjustedPlaybackRate = Math.round(playbackRate * kPlaybackRateAdjustmentMultiplier);
    return frameTimeNs / adjustedPlaybackRate * kPlaybackRateAdjustmentMultiplier;
  }

  private static long closestVsync(long releaseTime, long sampledVsyncTime, long vsyncDuration) {
    long vsyncCount = (releaseTime - sampledVsyncTime) / vsyncDuration;
    long snappedTimeNs = sampledVsyncTime + (vsyncDuration * vsyncCount);
    long snappedBeforeNs;
    long snappedAfterNs;
    if (releaseTime <= snappedTimeNs) {
      snappedBeforeNs = snappedTimeNs - vsyncDuration;
      snappedAfterNs = snappedTimeNs;
    } else {
      snappedBeforeNs = snappedTimeNs;
      snappedAfterNs = snappedTimeNs + vsyncDuration;
    }
    long snappedAfterDiff = snappedAfterNs - releaseTime;
    long snappedBeforeDiff = releaseTime - snappedBeforeNs;
    return snappedAfterDiff < snappedBeforeDiff ? snappedAfterNs : snappedBeforeNs;
  }

  /**
   * Samples display vsync timestamps. A single instance using a single {@link Choreographer} is
   * shared by all {@link VideoFrameReleaseTimeHelper} instances. This is done to avoid a resource
   * leak in the platform on API levels prior to 23.
   */
  private static final class VSyncSampler implements FrameCallback, Handler.Callback {

    public volatile long mSampledVsyncTimeNs;

    private static final int CREATE_CHOREOGRAPHER = 0;
    private static final int MSG_ADD_OBSERVER = 1;
    private static final int MSG_REMOVE_OBSERVER = 2;

    private static final VSyncSampler INSTANCE = new VSyncSampler();

    private final Handler mHandler;
    private final HandlerThread mChoreographerOwnerThread;
    private Choreographer mChoreographer;
    private int mObserverCount;

    public static VSyncSampler getInstance() {
      return INSTANCE;
    }

    private VSyncSampler() {
      mChoreographerOwnerThread = new HandlerThread("ChoreographerOwner:Handler");
      mChoreographerOwnerThread.start();
      mHandler = new Handler(mChoreographerOwnerThread.getLooper(), this);
      mHandler.sendEmptyMessage(CREATE_CHOREOGRAPHER);
    }

    /**
     * Notifies the sampler that a {@link VideoFrameReleaseTimeHelper} is observing {@link
     * #mSampledVsyncTimeNs}, and hence that the value should be periodically updated.
     */
    public void addObserver() {
      mHandler.sendEmptyMessage(MSG_ADD_OBSERVER);
    }

    /**
     * Notifies the sampler that a {@link VideoFrameReleaseTimeHelper} is no longer observing {@link
     * #mSampledVsyncTimeNs}.
     */
    public void removeObserver() {
      mHandler.sendEmptyMessage(MSG_REMOVE_OBSERVER);
    }

    @Override
    public void doFrame(long vsyncTimeNs) {
      mSampledVsyncTimeNs = vsyncTimeNs;
      mChoreographer.postFrameCallbackDelayed(this, CHOREOGRAPHER_SAMPLE_DELAY_MILLIS);
    }

    @Override
    public boolean handleMessage(Message message) {
      switch (message.what) {
        case CREATE_CHOREOGRAPHER:
          {
            createChoreographerInstanceInternal();
            return true;
          }
        case MSG_ADD_OBSERVER:
          {
            addObserverInternal();
            return true;
          }
        case MSG_REMOVE_OBSERVER:
          {
            removeObserverInternal();
            return true;
          }
        default:
          {
            return false;
          }
      }
    }

    private void createChoreographerInstanceInternal() {
      mChoreographer = Choreographer.getInstance();
    }

    private void addObserverInternal() {
      mObserverCount++;
      if (mObserverCount == 1) {
        mChoreographer.postFrameCallback(this);
      }
    }

    private void removeObserverInternal() {
      mObserverCount--;
      if (mObserverCount == 0) {
        mChoreographer.removeFrameCallback(this);
        mSampledVsyncTimeNs = 0;
      }
    }
  }
}
