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

import androidx.annotation.GuardedBy;
import androidx.annotation.Nullable;
import androidx.media3.common.C;
import androidx.media3.common.Format;
import androidx.media3.common.MediaItem;
import androidx.media3.common.MediaMetadata;
import androidx.media3.common.util.UnstableApi;
import androidx.media3.datasource.TransferListener;
import androidx.media3.exoplayer.source.BaseMediaSource;
import androidx.media3.exoplayer.source.ForwardingTimeline;
import androidx.media3.exoplayer.source.MediaPeriod;
import androidx.media3.exoplayer.source.SinglePeriodTimeline;
import androidx.media3.exoplayer.upstream.Allocator;
import java.io.IOException;

/**
 * A custom {@link BaseMediaSource} that provides native Starboard media streams to ExoPlayer.
 *
 * <p>Purpose: Manages timeline generation and creates {@link ExoPlayerMediaPeriod} instances for a
 * single audio or video stream received from the Starboard native layer.
 *
 * <p>Lifetime and Ownership: Instantiated and owned by {@link ExoPlayerBridge} for each active
 * audio and video track. Released when the parent {@link ExoPlayerBridge} is released.
 *
 * <p>Threading Model: Thread-affine to ExoPlayer's internal playback thread. Timeline updates and
 * period creation are guarded by an internal lock ({@code mLock}) to allow safe timeline time
 * adjustments.
 */
@UnstableApi
public final class ExoPlayerMediaSource extends BaseMediaSource {
  private final Format mFormat;
  private final Object mLock = new Object();
  private final ExoPlayerBridge mBridge;

  @GuardedBy("mLock")
  private ExoPlayerMediaPeriod mMediaPeriod;

  private final MediaItem mMediaItem;

  ExoPlayerMediaSource(Format format, ExoPlayerBridge bridge) {
    mFormat = format;
    mBridge = bridge;
    mMediaItem = new MediaItem.Builder().setMediaMetadata(MediaMetadata.EMPTY).build();
  }

  public Format getFormat() {
    return mFormat;
  }

  @Override
  protected void prepareSourceInternal(@Nullable TransferListener mediaTransferListener) {
    updateTimelineStartTime(0L);
  }

  public void updateTimelineStartTime(long startTimeUs) {
    refreshSourceInfo(
        new OffsetTimeline(
            new SinglePeriodTimeline(
                /* durationUs= */ C.TIME_UNSET,
                /* isSeekable= */ true,
                /* isDynamic= */ false,
                /* useLiveConfiguration= */ false,
                /* manifest= */ null,
                getMediaItem()),
            startTimeUs));
  }

  @Override
  protected void releaseSourceInternal() {}

  @Override
  public MediaItem getMediaItem() {
    return mMediaItem;
  }

  @Override
  public void maybeThrowSourceInfoRefreshError() throws IOException {}

  @Override
  public MediaPeriod createPeriod(MediaPeriodId id, Allocator allocator, long startPositionUs) {
    synchronized (mLock) {
      if (mMediaPeriod == null) {
        mMediaPeriod = new ExoPlayerMediaPeriod(this, mBridge);
        return mMediaPeriod;
      }
    }
    throw new IllegalStateException(
        "Called MediaSource.createPeriod when the MediaPeriod already exists");
  }

  @Override
  public void releasePeriod(MediaPeriod mediaPeriod) {
    synchronized (mLock) {
      if (mMediaPeriod != null) {
        if (mediaPeriod != mMediaPeriod) {
          throw new IllegalStateException(
              "Called MediaSource.releasePeriod on an unknown MediaPeriod");
        }
        mMediaPeriod = null;
        return;
      }
    }
    throw new IllegalStateException(
        "Called MediaSource.releasePeriod() after period was already released");
  }

  /**
   * A custom {@link ForwardingTimeline} that applies a constant time offset to the underlying
   * timeline's window. This is used to synchronize ExoPlayer's internal clock with the absolute
   * timestamps of live streams without requiring a seek operation.
   */
  private static class OffsetTimeline extends ForwardingTimeline {
    private final long mOffsetUs;

    public OffsetTimeline(androidx.media3.common.Timeline timeline, long offsetUs) {
      super(timeline);
      mOffsetUs = offsetUs;
    }

    @Override
    public Window getWindow(int windowIndex, Window window, long defaultPositionProjectionUs) {
      super.getWindow(windowIndex, window, defaultPositionProjectionUs);
      window.positionInFirstPeriodUs -= mOffsetUs;
      return window;
    }
  }
}
