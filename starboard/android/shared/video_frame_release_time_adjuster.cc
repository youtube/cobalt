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

#include "starboard/android/shared/video_frame_release_time_adjuster.h"

namespace starboard {
namespace android {
namespace shared {

VideoFrameReleaseTimeHelper::VideoFrameReleaseTimeHelper() {
  this(DISPLAY_REFRESH_RATE_UNKNOWN);
}

VideoFrameReleaseTimeHelper::VideoFrameReleaseTimeHelper(Context context) {
  this(getDefaultDisplayRefreshRate(context));
}

VideoFrameReleaseTimeHelper::VideoFrameReleaseTimeHelper(
    double defaultDisplayRefreshRate) {
  useDefaultDisplayVsync =
      defaultDisplayRefreshRate != DISPLAY_REFRESH_RATE_UNKNOWN;
  if (useDefaultDisplayVsync) {
    vsyncSampler = VSyncSampler.getInstance();
    vsyncDurationNs = (long)(NANOS_PER_SECOND / defaultDisplayRefreshRate);
    vsyncOffsetNs = (vsyncDurationNs * VSYNC_OFFSET_PERCENTAGE) / 100;
  } else {
    vsyncSampler = null;
    vsyncDurationNs = -1;  // Value unused.
    vsyncOffsetNs = -1;    // Value unused.
  }
}

void VideoFrameReleaseTimeHelper::enable() {
  haveSync = false;
  if (useDefaultDisplayVsync) {
    vsyncSampler.addObserver();
  }
}

void VideoFrameReleaseTimeHelper::disable() {
  if (useDefaultDisplayVsync) {
    vsyncSampler.removeObserver();
  }
}

long VideoFrameReleaseTimeHelper::adjustReleaseTime(
    long framePresentationTimeUs,
    long unadjustedReleaseTimeNs) {
  long framePresentationTimeNs = framePresentationTimeUs * 1000;

  // Until we know better, the adjustment will be a no-op.
  long adjustedFrameTimeNs = framePresentationTimeNs;
  long adjustedReleaseTimeNs = unadjustedReleaseTimeNs;

  if (haveSync) {
    // See if we've advanced to the next frame.
    if (framePresentationTimeUs != lastFramePresentationTimeUs) {
      frameCount++;
      adjustedLastFrameTimeNs = pendingAdjustedFrameTimeNs;
    }
    if (frameCount >= MIN_FRAMES_FOR_ADJUSTMENT) {
      // We're synced and have waited the required number of frames to apply an
      // adjustment. Calculate the average frame time across all the frames
      // we've seen since the last sync. This will typically give us a frame
      // rate at a finer granularity than the frame times themselves (which
      // often only have millisecond granularity).
      long averageFrameDurationNs =
          (framePresentationTimeNs - syncFramePresentationTimeNs) / frameCount;
      // Project the adjusted frame time forward using the average.
      long candidateAdjustedFrameTimeNs =
          adjustedLastFrameTimeNs + averageFrameDurationNs;

      if (isDriftTooLarge(candidateAdjustedFrameTimeNs,
                          unadjustedReleaseTimeNs)) {
        haveSync = false;
      } else {
        adjustedFrameTimeNs = candidateAdjustedFrameTimeNs;
        adjustedReleaseTimeNs = syncUnadjustedReleaseTimeNs +
                                adjustedFrameTimeNs -
                                syncFramePresentationTimeNs;
      }
    } else {
      // We're synced but haven't waited the required number of frames to apply
      // an adjustment. Check drift anyway.
      if (isDriftTooLarge(framePresentationTimeNs, unadjustedReleaseTimeNs)) {
        haveSync = false;
      }
    }
  }

  // If we need to sync, do so now.
  if (!haveSync) {
    syncFramePresentationTimeNs = framePresentationTimeNs;
    syncUnadjustedReleaseTimeNs = unadjustedReleaseTimeNs;
    frameCount = 0;
    haveSync = true;
    onSynced();
  }

  lastFramePresentationTimeUs = framePresentationTimeUs;
  pendingAdjustedFrameTimeNs = adjustedFrameTimeNs;

  if (vsyncSampler == null || vsyncSampler.sampledVsyncTimeNs == 0) {
    return adjustedReleaseTimeNs;
  }

  // Find the timestamp of the closest vsync. This is the vsync that we're
  // targeting.
  long snappedTimeNs = closestVsync(
      adjustedReleaseTimeNs, vsyncSampler.sampledVsyncTimeNs, vsyncDurationNs);
  // Apply an offset so that we release before the target vsync, but after the
  // previous one.
  return snappedTimeNs - vsyncOffsetNs;
}

void VideoFrameReleaseTimeHelper::onSynced() {
  // Do nothing.
}

boolean VideoFrameReleaseTimeHelper::isDriftTooLarge(long frameTimeNs,
                                                     long releaseTimeNs) {
  long elapsedFrameTimeNs = frameTimeNs - syncFramePresentationTimeNs;
  long elapsedReleaseTimeNs = releaseTimeNs - syncUnadjustedReleaseTimeNs;
  return Math.abs(elapsedReleaseTimeNs - elapsedFrameTimeNs) >
         MAX_ALLOWED_DRIFT_NS;
}

static long VideoFrameReleaseTimeHelper::closestVsync(long releaseTime,
                                                      long sampledVsyncTime,
                                                      long vsyncDuration) {
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
  return snappedAfterDiff < snappedBeforeDiff ? snappedAfterNs
                                              : snappedBeforeNs;
}

static double VideoFrameReleaseTimeHelper::getDefaultDisplayRefreshRate(
    Context context) {
  WindowManager manager =
      (WindowManager)context.getSystemService(Context.WINDOW_SERVICE);
  return manager.getDefaultDisplay() != null
             ? manager.getDefaultDisplay().getRefreshRate()
             : DISPLAY_REFRESH_RATE_UNKNOWN;
}

}  // namespace shared
}  // namespace android
}  // namespace starboard
