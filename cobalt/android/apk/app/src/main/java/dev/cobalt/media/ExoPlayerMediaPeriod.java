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

import androidx.annotation.NonNull;
import androidx.media3.common.C;
import androidx.media3.common.Format;
import androidx.media3.common.TrackGroup;
import androidx.media3.exoplayer.LoadingInfo;
import androidx.media3.exoplayer.SeekParameters;
import androidx.media3.exoplayer.source.MediaPeriod;
import androidx.media3.exoplayer.source.SampleStream;
import androidx.media3.exoplayer.source.TrackGroupArray;
import androidx.media3.exoplayer.trackselection.ExoTrackSelection;
import androidx.media3.exoplayer.upstream.Allocator;
import java.io.IOException;

/** Implements the ExoPlayer MediaPeriod interface to write samples to the ExoPlayerSampleStream. */
public class ExoPlayerMediaPeriod implements MediaPeriod {
  private final Format mFormat;
  private final Allocator mAllocator;
  private ExoPlayerSampleStream mStream;

  ExoPlayerMediaPeriod(Format format, Allocator allocator) {
    mFormat = format;
    mAllocator = allocator;
  }

  @Override
  public void prepare(Callback callback, long positionUs) {
    callback.onPrepared(this);
  }

  @Override
  public void maybeThrowPrepareError() throws IOException {}

  @Override
  public TrackGroupArray getTrackGroups() {
    return new TrackGroupArray(new TrackGroup(mFormat));
  }

  /**
   * Initializes the {@link ExoPlayerSampleStream} for the selected track.
   *
   * Cobalt's implementation assumes a single track per period (either audio or video).
   */
  @Override
  public long selectTracks(
      ExoTrackSelection[] selections,
      boolean[] mayRetainStreamFlags,
      SampleStream[] streams,
      boolean[] streamResetFlags,
      long positionUs) {
    if (mStream != null) {
      throw new IllegalStateException(
          "Tried to initialize the SampleStream after it has already been created");
    }

    for (int i = 0; i < selections.length; ++i) {
      if (selections[i] != null) {
        mStream = new ExoPlayerSampleStream(mAllocator, selections[i].getSelectedFormat());
        streams[i] = mStream;
        streamResetFlags[i] = true;
      }
    }
    return positionUs;
  }

  @Override
  public void discardBuffer(long positionUs, boolean toKeyframe) {
    if (mStream != null) {
      mStream.discardBuffer(positionUs, toKeyframe);
    }
  }

  @Override
  public long readDiscontinuity() {
    return C.TIME_UNSET;
  }

  @Override
  public long seekToUs(long positionUs) {
    mStream.seek(positionUs, mFormat);
    return positionUs;
  }

  @Override
  public long getAdjustedSeekPositionUs(long positionUs, SeekParameters seekParameters) {
    return positionUs;
  }

  @Override
  public long getBufferedPositionUs() {
    return mStream != null ? mStream.getBufferedPositionUs() : 0L;
  }

  @Override
  public long getNextLoadPositionUs() {
    return getBufferedPositionUs();
  }

  @Override
  public boolean continueLoading(@NonNull LoadingInfo loadingInfo) {
    return mStream == null || (!mStream.endOfStreamWritten() && mStream.canAcceptMoreData());
  }

  @Override
  public boolean isLoading() {
    return mStream == null || !mStream.endOfStreamWritten();
  }

  @Override
  public void reevaluateBuffer(long positionUs) {}

  public void destroySampleStream() {
    if (mStream != null) {
      mStream.destroy();
    }
  }

  /**
   * Writes a sample to the sample stream.
   *
   * @param sample The media sample data and its metadata.
   */
  public void writeSample(ExoPlayerMediaSample sample) {
    mStream.writeSample(sample);
  }

  public void writeEndOfStream() {
    mStream.writeEndOfStream();
  }

  public boolean canAcceptMoreData() {
    if (mStream == null) {
      return false;
    }
    return mStream.canAcceptMoreData();
  }
}
