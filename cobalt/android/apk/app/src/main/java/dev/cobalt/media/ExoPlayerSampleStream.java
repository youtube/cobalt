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
import androidx.media3.common.MimeTypes;
import androidx.media3.decoder.DecoderInputBuffer;
import androidx.media3.exoplayer.FormatHolder;
import androidx.media3.exoplayer.source.SampleStream;
import java.io.IOException;
import java.nio.ByteBuffer;

/**
 * A custom {@link SampleStream} that interfaces with the native Cobalt C++ layer to provide media
 * samples to ExoPlayer. It pulls data from the native queues and handles memory allocation for
 * zero-copy JNI transfer.
 */
public class ExoPlayerSampleStream implements SampleStream {
  // Custom signal from native indicating the buffer requires allocation before reading data.
  private static final int RESULT_NEEDS_ALLOCATION = -6;

  private final Format mFormat;
  private final int mType;
  private final ExoPlayerBridge mBridge;
  private final ExoPlayerMediaSource mMediaSource;
  private boolean mFormatSent = false;
  private boolean mIsFirstSample = true;
  private final long[] mMetadata = new long[3];

  ExoPlayerSampleStream(Format format, ExoPlayerBridge bridge, ExoPlayerMediaSource mediaSource) {
    mFormat = format;
    mBridge = bridge;
    mMediaSource = mediaSource;
    if (MimeTypes.isVideo(format.sampleMimeType)) {
      mType = ExoPlayerBridge.TYPE_VIDEO;
    } else {
      mType = ExoPlayerBridge.TYPE_AUDIO;
    }
  }

  /** Returns whether data is available to be read. */
  @Override
  public boolean isReady() {
    return mBridge.isReady(mType);
  }

  /**
   * Throws an error that's preventing data from being read. Does nothing if no such error exists.
   */
  @Override
  public void maybeThrowError() throws IOException {}

  /**
   * Reads from the stream.
   *
   * @param formatHolder A {@link FormatHolder} to populate in the case of reading a format.
   * @param buffer A {@link DecoderInputBuffer} to populate in the case of reading a sample or the
   *     end of the stream.
   * @param readFlags Flags controlling the behavior of this read operation.
   * @return The result of the read operation.
   */
  @Override
  public int readData(
      @NonNull FormatHolder formatHolder, @NonNull DecoderInputBuffer buffer, int readFlags) {
    if (!mFormatSent || (readFlags & SampleStream.FLAG_REQUIRE_FORMAT) != 0) {
      formatHolder.format = mFormat;
      mFormatSent = true;
      return C.RESULT_FORMAT_READ;
    }

    int result = mBridge.readSample(mType, buffer.data, mMetadata);

    if (result == RESULT_NEEDS_ALLOCATION) {
      int size = (int) mMetadata[1];
      // Cobalt's JNI layer strictly requires zero-copy DirectByteBuffers.
      // We don't call buffer.ensureSpaceForWrite(size) here, as it may allocate
      // standard Java heap buffers which are rejected by the C++ bridge.
      buffer.data = ByteBuffer.allocateDirect(size);
      result = mBridge.readSample(mType, buffer.data, mMetadata);
    }

    if (result == C.RESULT_BUFFER_READ) {
      buffer.timeUs = mMetadata[0];
      int size = (int) mMetadata[1];
      int flags = (int) mMetadata[2];

      if (mIsFirstSample) {
        mIsFirstSample = false;
        mMediaSource.updateTimelineStartTime(buffer.timeUs);
      }

      if (buffer.data != null) {
        buffer.data.position(size);
      }

      buffer.setFlags(flags);
    }

    return result;
  }

  /**
   * Attempts to skip to the keyframe before the specified position.
   *
   * @param positionUs The specified time.
   * @return The number of messages that were skipped.
   */
  @Override
  public int skipData(long positionUs) {
    return mBridge.skipData(mType, positionUs);
  }

  public long getBufferedPositionUs() {
    return mBridge.getBufferedPositionUs(mType);
  }
}
