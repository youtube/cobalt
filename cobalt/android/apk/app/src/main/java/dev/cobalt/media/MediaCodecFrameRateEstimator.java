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

import dev.cobalt.util.Log;
import java.util.Arrays;

/** Frame rate estimator for MediaCodec. */
public final class MediaCodecFrameRateEstimator {
  /** Interface for frame rate estimation. */
  public interface FrameRateEstimator {
    int INVALID_FRAME_RATE = -1;

    int getEstimatedFrameRate();

    void reset();

    void onNewInput(long presentationTimeUs);

    void onNewOutput(long presentationTimeUs);
  }

  public static FrameRateEstimator create(boolean useInput) {
    return useInput ? new InputFrameRateEstimator() : new OutputFrameRateEstimator();
  }

  // This estimator is designed to use output timestamps which are in presentation order.
  private static class OutputFrameRateEstimator implements FrameRateEstimator {
    private static final long INVALID_FRAME_TIMESTAMP = -1;
    private static final int MINIMUM_REQUIRED_FRAMES = 4;
    private long mLastFrameTimestampUs = INVALID_FRAME_TIMESTAMP;
    private long mNumberOfFrames = 0;
    private long mTotalDurationUs = 0;

    @Override
    public int getEstimatedFrameRate() {
      if (mTotalDurationUs <= 0 || mNumberOfFrames < MINIMUM_REQUIRED_FRAMES) {
        return INVALID_FRAME_RATE;
      }
      return Math.round((mNumberOfFrames - 1) * 1000000.0f / mTotalDurationUs);
    }

    @Override
    public void reset() {
      mLastFrameTimestampUs = INVALID_FRAME_TIMESTAMP;
      mNumberOfFrames = 0;
      mTotalDurationUs = 0;
    }

    @Override
    public void onNewInput(long presentationTimeUs) {}

    @Override
    public void onNewOutput(long presentationTimeUs) {
      mNumberOfFrames++;

      if (mLastFrameTimestampUs == INVALID_FRAME_TIMESTAMP) {
        mLastFrameTimestampUs = presentationTimeUs;
        return;
      }
      if (presentationTimeUs <= mLastFrameTimestampUs) {
        Log.v(TAG, "Invalid output presentation timestamp.");
        return;
      }

      mTotalDurationUs += presentationTimeUs - mLastFrameTimestampUs;
      mLastFrameTimestampUs = presentationTimeUs;
    }
  }

  // This estimator is designed to use input timestamps which are in decoding order.
  // Once it's verfied to work, we can remove |MediaCodecFrameRateEstimator| and always use this
  // estimator.
  private static class InputFrameRateEstimator implements FrameRateEstimator {
    private static final int WINDOW_SIZE = 32;
    private static final int MINIMUM_REQUIRED_TUNNEL_FRAMES = 8;
    private static final long INVALID_FRAME_TIMESTAMP = -1;
    private final long[] mSortedTimestampsUs = new long[WINDOW_SIZE];
    private long mMinDeltaUs = INVALID_FRAME_TIMESTAMP;
    private int mCount = 0;

    @Override
    public int getEstimatedFrameRate() {
      if (mCount < MINIMUM_REQUIRED_TUNNEL_FRAMES) {
        return INVALID_FRAME_RATE;
      }

      if (mMinDeltaUs == INVALID_FRAME_TIMESTAMP) {
        for (int i = 1; i < mCount; i++) {
          long delta = mSortedTimestampsUs[i] - mSortedTimestampsUs[i - 1];
          if (mMinDeltaUs == INVALID_FRAME_TIMESTAMP || delta < mMinDeltaUs) {
            mMinDeltaUs = delta;
          }
        }
      }

      int validTimestamps = 1;
      for (; validTimestamps < mCount; validTimestamps++) {
        long delta =
            mSortedTimestampsUs[validTimestamps] - mSortedTimestampsUs[validTimestamps - 1];
        if (delta < mMinDeltaUs) {
          mMinDeltaUs = delta;
        } else if (delta > 2 * mMinDeltaUs) {
          // The delta is too large.
          break;
        }
      }

      if (validTimestamps < MINIMUM_REQUIRED_TUNNEL_FRAMES) {
        return INVALID_FRAME_RATE;
      }

      return (int)
          Math.round(
              (validTimestamps - 1)
                  * 1000000.0f
                  / (mSortedTimestampsUs[validTimestamps - 1] - mSortedTimestampsUs[0]));
    }

    @Override
    public void reset() {
      mCount = 0;
      mMinDeltaUs = INVALID_FRAME_TIMESTAMP;
      Arrays.fill(mSortedTimestampsUs, 0);
    }

    @Override
    public void onNewInput(long presentationTimeUs) {
      if (mCount < WINDOW_SIZE) {
        // Find the insertion point and shift elements right to maintain the sorted order.
        int i = mCount;
        while (i > 0 && presentationTimeUs < mSortedTimestampsUs[i - 1]) {
          mSortedTimestampsUs[i] = mSortedTimestampsUs[i - 1];
          i--;
        }
        mSortedTimestampsUs[i] = presentationTimeUs;
        mCount++;
      } else {
        // Remove the least timestamp (at index 0 since it's sorted) and add the new one.
        // Then shift it to the right to maintain the sorted order.
        int i = 0;
        while (i < WINDOW_SIZE - 1 && presentationTimeUs > mSortedTimestampsUs[i + 1]) {
          mSortedTimestampsUs[i] = mSortedTimestampsUs[i + 1];
          i++;
        }
        mSortedTimestampsUs[i] = presentationTimeUs;
      }
    }

    @Override
    public void onNewOutput(long presentationTimeUs) {}
  }

  private MediaCodecFrameRateEstimator() {}
}
