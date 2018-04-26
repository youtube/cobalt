// Copyright 2018 Google Inc. All Rights Reserved.
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

#ifndef STARBOARD_ANDROID_SHARED_VIDEO_FRAME_RELEASE_TIME_ADJUSTER_H_
#define STARBOARD_ANDROID_SHARED_VIDEO_FRAME_RELEASE_TIME_ADJUSTER_H_

namespace starboard {
namespace android {
namespace shared {

class VideoFrameReleaseTimeAdjuster {
 public:
  /**
   * Constructs an instance that smooths frame release timestamps but does not
   * align them with the default display's vsync signal.
   */
  VideoFrameReleaseTimeHelper();

  /**
   * Constructs an instance that smooths frame release timestamps and aligns
   * them with the default display's vsync signal.
   *
   * @param context A context from which information about the default display
   * can be retrieved.
   */
  VideoFrameReleaseTimeHelper(Context context);

  /** Enables the helper. */
  void enable();

  /** Disables the helper. */
  void disable();

  /**
   * Adjusts a frame release timestamp.
   *
   * @param framePresentationTimeUs The frame's presentation time, in
   * microseconds.
   * @param unadjustedReleaseTimeNs The frame's unadjusted release time, in
   * nanoseconds and in the same time base as {@link System#nanoTime()}.
   * @return The adjusted frame release timestamp, in nanoseconds and in the
   * same time base as
   *     {@link System#nanoTime()}.
   */
  long adjustReleaseTime(long framePresentationTimeUs,
                         long unadjustedReleaseTimeNs);

 protected:
  void onSynced();

 private:
  VideoFrameReleaseTimeHelper(double defaultDisplayRefreshRate);

  boolean isDriftTooLarge(long frameTimeNs, long releaseTimeNs);

  static long closestVsync(long releaseTime,
                           long sampledVsyncTime,
                           long vsyncDuration);

  static double getDefaultDisplayRefreshRate(Context context);

  /**
   * Samples display vsync timestamps. A single instance using a single {@link
   * Choreographer} is shared by all {@link VideoFrameReleaseTimeHelper}
   * instances. This is done to avoid a resource leak in the platform on API
   * levels prior to 23.
   */
  class VSyncSampler implements FrameCallback, Handler.Callback {
   public:
    static VSyncSampler getInstance() { return INSTANCE; }

    VSyncSampler() {
      choreographerOwnerThread =
          new HandlerThread("ChoreographerOwner:Handler");
      choreographerOwnerThread.start();
      handler = new Handler(choreographerOwnerThread.getLooper(), this);
      handler.sendEmptyMessage(CREATE_CHOREOGRAPHER);
    }

    /**
     * Notifies the sampler that a {@link VideoFrameReleaseTimeHelper} is
     * observing {@link #sampledVsyncTimeNs}, and hence that the value should be
     * periodically updated.
     */
    void addObserver() { handler.sendEmptyMessage(MSG_ADD_OBSERVER); }

    /**
     * Notifies the sampler that a {@link VideoFrameReleaseTimeHelper} is no
     * longer observing {@link #sampledVsyncTimeNs}.
     */
    void removeObserver() { handler.sendEmptyMessage(MSG_REMOVE_OBSERVER); }

    void doFrame(long vsyncTimeNs) {
      sampledVsyncTimeNs = vsyncTimeNs;
      choreographer.postFrameCallbackDelayed(this,
                                             CHOREOGRAPHER_SAMPLE_DELAY_MILLIS);
    }

    boolean handleMessage(Message message) {
      switch (message.what) {
        case CREATE_CHOREOGRAPHER: {
          createChoreographerInstanceInternal();
          return true;
        }
        case MSG_ADD_OBSERVER: {
          addObserverInternal();
          return true;
        }
        case MSG_REMOVE_OBSERVER: {
          removeObserverInternal();
          return true;
        }
        default: { return false; }
      }
    }

    volatile long sampledVsyncTimeNs;

   private:
    void createChoreographerInstanceInternal() {
      choreographer = Choreographer.getInstance();
    }

    void addObserverInternal() {
      observerCount++;
      if (observerCount == 1) {
        choreographer.postFrameCallback(this);
      }
    }

    void removeObserverInternal() {
      observerCount--;
      if (observerCount == 0) {
        choreographer.removeFrameCallback(this);
        sampledVsyncTimeNs = 0;
      }
    }

    static final int CREATE_CHOREOGRAPHER = 0;
    static final int MSG_ADD_OBSERVER = 1;
    static final int MSG_REMOVE_OBSERVER = 2;

    static final VSyncSampler INSTANCE = new VSyncSampler();

    final Handler handler;
    final HandlerThread choreographerOwnerThread;
    Choreographer choreographer;
    int observerCount;
  };

  static final double DISPLAY_REFRESH_RATE_UNKNOWN = -1;
  static final long CHOREOGRAPHER_SAMPLE_DELAY_MILLIS = 500;
  static final long MAX_ALLOWED_DRIFT_NS = 20000000;

  static final long VSYNC_OFFSET_PERCENTAGE = 80;
  static final int MIN_FRAMES_FOR_ADJUSTMENT = 6;
  static final long NANOS_PER_SECOND = 1000000000L;

  final VSyncSampler vsyncSampler;
  final boolean useDefaultDisplayVsync;
  final long vsyncDurationNs;
  final long vsyncOffsetNs;

  long lastFramePresentationTimeUs;
  long adjustedLastFrameTimeNs;
  long pendingAdjustedFrameTimeNs;

  boolean haveSync;
  long syncUnadjustedReleaseTimeNs;
  long syncFramePresentationTimeNs;
  long frameCount;
};

}  // namespace shared
}  // namespace android
}  // namespace starboard

#endif  // STARBOARD_ANDROID_SHARED_VIDEO_FRAME_RELEASE_TIME_ADJUSTER_H_
