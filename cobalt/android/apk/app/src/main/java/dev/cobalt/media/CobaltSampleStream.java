// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

import androidx.media3.common.C;
import androidx.media3.common.Format;
import androidx.media3.common.util.ParsableByteArray;
import androidx.media3.common.util.UnstableApi;
import androidx.media3.decoder.DecoderInputBuffer;
import androidx.media3.exoplayer.FormatHolder;
import androidx.media3.exoplayer.source.SampleQueue;
import androidx.media3.exoplayer.source.SampleStream;
import androidx.media3.exoplayer.upstream.Allocator;
import androidx.media3.exoplayer.upstream.DefaultAllocator;
import dev.cobalt.util.Log;
import java.io.IOException;
import java.util.ArrayList;

@UnstableApi
public class CobaltSampleStream implements SampleStream {
  private SampleQueue sampleQueue;
  public boolean prerolled = false;
  public boolean firstData = true;
  private boolean endOfStream = false;
  private Allocator allocator;

  CobaltSampleStream(Allocator allocator, Format format) {
    this.allocator = allocator;
    // audioAllocator = new DefaultAllocator(true, 4000000 /* 4 MB */);
    // videoAllocator = new DefaultAllocator(true, 10000000 /* 10 MB */);
    sampleQueue = SampleQueue.createWithoutDrm(allocator);
    sampleQueue.format(format);
  }

  void discardBuffer(long positionUs, boolean toKeyframe) {
    Log.i(TAG, String.format("Discarding Samplequeue to %d", positionUs));
    sampleQueue.discardTo(positionUs, toKeyframe, true);
  }

  void writeSample(byte[] data, int sizeInBytes, long timestampUs, boolean isKeyFrame, boolean isEndOfStream) {
    int sampleFlags = isKeyFrame ? C.BUFFER_FLAG_KEY_FRAME : 0;
    if (firstData) {
      sampleFlags = sampleFlags | C.BUFFER_FLAG_FIRST_SAMPLE;
      firstData = false;
      Log.i(TAG, String.format("Setting sampleQueue start time to %d", timestampUs));
      sampleQueue.setStartTimeUs(timestampUs);
    }
    if (isEndOfStream) {
      sampleFlags = sampleFlags | C.BUFFER_FLAG_END_OF_STREAM;
      endOfStream = true;
      Log.i(TAG, "Reached end of stream in SampleQueue");
    }
    // Log.i(TAG, String.format("Preparing to append to queue with timestamp %d", timestampUs));
    ParsableByteArray array = new ParsableByteArray(data);
    // Log.i(TAG, String.format("Size in bytes is %d, data length is %d", sizeInBytes, data.length));
    try {
      sampleQueue.sampleData(array, sizeInBytes);
    } catch (Exception e) {
      Log.i(TAG, String.format("Caught exception from sampleQueue.sampleData() %s", e.toString()));
    }
    try {
      sampleQueue.sampleMetadata(timestampUs, sampleFlags, sizeInBytes, 0, null);
      Log.i(TAG, String.format("Wrote %s sample with timestamp %d", (sampleQueue.getUpstreamFormat().sampleRate != Format.NO_VALUE ? "audio" :"video"), timestampUs));
    } catch (Exception e) {
      Log.i(TAG, String.format("Caught exception from sampleQueue.sampleMetadata() %s", e.toString()));
    }
    // Log.i(TAG, String.format("Appended sample for format: %s (%s) of size %d. Current read index: %d, current queued timestamp: %d, is key frame: %s",
    //     sampleQueue.getUpstreamFormat().codecs, sampleQueue.getUpstreamFormat().sampleMimeType,
    //     sizeInBytes, sampleQueue.getReadIndex(), sampleQueue.getLargestQueuedTimestampUs(), isKeyFrame ? "yes" : "no"));
    if (!prerolled) {
      if (sampleQueue.getUpstreamFormat().sampleRate != Format.NO_VALUE) {
        // Audio stream
        prerolled = (prerolled || sampleQueue.getLargestQueuedTimestampUs() > 500000) || endOfStream;
        if (prerolled) {
          Log.i(TAG, "Prerolled audio stream");
        }
      } else {
        // Video stream
        prerolled = (prerolled || sampleQueue.getLargestQueuedTimestampUs() > 5000000) || endOfStream;
        if (prerolled) {
          Log.i(TAG, "Prerolled video stream");
        }
      }
    }
  }

  @Override
  public boolean isReady() {
    Log.i(TAG, String.format("Checking if SampleStream is ready: %b", sampleQueue.isReady(endOfStream)));
    return sampleQueue.isReady(endOfStream) && prerolled;
    // boolean ready = sampleQueue.isReady(false);
    // Log.i(TAG, String.format("Checking if SampleStream is ready. SampleQueue: %b, prerolled: %b", ready, prerolled));
    // return ready && prerolled;
    // return sampleQueue.isReady(false);
  }

  @Override
  public void maybeThrowError() throws IOException {
    // Log.i(TAG, "ExoPlayer SampleStream throws an error");
    // try {
    //   sampleQueue.maybeThrowError();
    // } catch (IOException e) {
    //   Log.i(TAG, String.format("Caught exception from SampleQueue: %s", e.toString()));
    // }
  }

  @Override
  public int readData(FormatHolder formatHolder, DecoderInputBuffer buffer, int readFlags) {
    if (sampleQueue.getUpstreamFormat().sampleRate != Format.NO_VALUE) {
      Log.i(TAG, "Called readData() for audio");
    } else {
      Log.i(TAG, "Called readData() for video");
    }
    if (!sampleQueue.isReady(endOfStream)) {
      // Log.e(TAG, "SampleQueue is not ready to read from");
      Log.i(TAG, "Queue is not ready to be read from");
      return C.RESULT_NOTHING_READ;
    }
    Log.i(TAG, String.format("Current read position is %d", sampleQueue.getFirstTimestampUs()));
    // Log.i(TAG, String.format("READING DATA FROM SAMPLEQUEUE. READFLAGS: %d", readFlags));
    // if (buffer.format != null) {
    //   Log.i(TAG, String.format("ExoPlayer is reading data for format %s", buffer.format.sampleMimeType));
    // } else if (formatHolder.format != null) {
    //   Log.i(TAG, String.format("ExoPlayer is reading formatholder for format %s", formatHolder.format.sampleMimeType));
    // }
    int read = -1;
    try {
      read = sampleQueue.read(formatHolder, buffer, readFlags, endOfStream);
      if (buffer.format != null) {
        Log.i(TAG,
            String.format("ExoPlayer is reading data for format %s", buffer.format.sampleMimeType));
      } else if (formatHolder.format != null) {
        Log.i(TAG, String.format("ExoPlayer is reading formatholder for format %s",
            formatHolder.format.sampleMimeType));
      }
      Log.i(TAG, String.format("READ RETURNS %d", read));
    } catch (Exception e) {
      Log.i(TAG, String.format("Caught exception from read() %s", e.toString()));
    }

    Log.i(TAG, String.format("Buffer time for %s sample is %d, is key frame %b", (sampleQueue.getUpstreamFormat().sampleRate != Format.NO_VALUE ? "audio" : "video"), buffer.timeUs, buffer.isKeyFrame()));
    Log.i(TAG, String.format("Allocated bytes is %d", allocator.getTotalBytesAllocated()));
    return read;
  }

  @Override
  public int skipData(long positionUs) {
    Log.i(TAG, String.format("Called skipData() to %d", positionUs));
    int skipCount = sampleQueue.getSkipCount(positionUs, endOfStream);
    sampleQueue.skip(skipCount);
    Log.i(TAG, String.format("Skipped %d samples", skipCount));
    return skipCount;
  }

  public long getBufferedPositionUs() {
    return sampleQueue.getLargestQueuedTimestampUs();
  }

  public void clearStream() {
    Log.i(TAG, "Called clearStream()");
    sampleQueue.reset();
    firstData = true;
    prerolled = false;
    endOfStream = false;
  }
}
