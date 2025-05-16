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
  private boolean prerolled = false;
  private boolean firstData = true;

  CobaltSampleStream(Allocator allocator, Format format) {
    // this.allocator = allocator;
    // audioAllocator = new DefaultAllocator(true, 4000000 /* 4 MB */);
    // videoAllocator = new DefaultAllocator(true, 10000000 /* 10 MB */);
    sampleQueue = SampleQueue.createWithoutDrm(allocator);
    sampleQueue.format(format);
  }

  void writeSample(byte[] data, int sizeInBytes, long timestampUs, boolean isKeyFrame, boolean isEndOfStream) {
    int sampleFlags = isKeyFrame ? C.BUFFER_FLAG_KEY_FRAME : 0;
    if (firstData) {
      sampleFlags = sampleFlags | C.BUFFER_FLAG_FIRST_SAMPLE;
      firstData = false;
    }
    if (isEndOfStream) {
      sampleFlags = sampleFlags | C.BUFFER_FLAG_END_OF_STREAM;
    }
    // Log.i(TAG, String.format("Preparing to append to queue with timestamp %d", timestampUs));
    ParsableByteArray array = new ParsableByteArray(data);
    // Log.i(TAG, String.format("Size in bytes is %d, data length is %d", sizeInBytes, data.length));
    sampleQueue.sampleData(array, sizeInBytes);
    sampleQueue.sampleMetadata(timestampUs, sampleFlags, sizeInBytes,0, null);
    // Log.i(TAG, String.format("Appended sample for format: %s (%s) of size %d. Current read index: %d, current queued timestamp: %d, is key frame: %s",
    //     sampleQueue.getUpstreamFormat().codecs, sampleQueue.getUpstreamFormat().sampleMimeType,
    //     sizeInBytes, sampleQueue.getReadIndex(), sampleQueue.getLargestQueuedTimestampUs(), isKeyFrame ? "yes" : "no"));
    if (!prerolled) {
      if (sampleQueue.getUpstreamFormat().sampleRate != Format.NO_VALUE) {
        // Audio stream
        prerolled = (prerolled || sampleQueue.getLargestQueuedTimestampUs() > 500000);
        if (prerolled) {
          Log.i(TAG, "Prerolled audio stream");
        }
      } else {
        // Video stream
        prerolled = (prerolled || sampleQueue.getLargestQueuedTimestampUs() > 7000000);
        if (prerolled) {
          Log.i(TAG, "Prerolled video stream");
        }
      }
    }
  }

  @Override
  public boolean isReady() {
    boolean ready = sampleQueue.isReady(false);
    return ready && prerolled;
    // return sampleQueue.isReady(false);
  }

  @Override
  public void maybeThrowError() throws IOException {

  }

  @Override
  public int readData(FormatHolder formatHolder, DecoderInputBuffer buffer, int readFlags) {
    if (!sampleQueue.isReady(false)) {
      // Log.e(TAG, "SampleQueue is not ready to read from");
      return C.RESULT_NOTHING_READ;
    }
    // Log.i(TAG, String.format("READING DATA FROM SAMPLEQUEUE. READFLAGS: %d", readFlags));
    // if (buffer.format != null) {
    //   Log.i(TAG, String.format("ExoPlayer is reading data for format %s", buffer.format.sampleMimeType));
    // } else if (formatHolder.format != null) {
    //   Log.i(TAG, String.format("ExoPlayer is reading formatholder for format %s", formatHolder.format.sampleMimeType));
    // }
    int read = sampleQueue.read(formatHolder, buffer, readFlags, false);
    // Log.i(TAG, String.format("READ RETURNS %d", read));
    return read;
  }

  @Override
  public int skipData(long positionUs) {
    return 0;
  }

  public long getBufferedPositionUs() {
    return sampleQueue.getLargestQueuedTimestampUs();
  }

  public void clearStream() {
    sampleQueue.discardToEnd();
    firstData = true;
    prerolled = false;
  }
}
