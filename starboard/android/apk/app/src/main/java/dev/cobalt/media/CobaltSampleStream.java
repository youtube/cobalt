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
  // private Allocator audioAllocator;
  // private Allocator videoAllocator;
  private final Allocator allocator;
  private int AUDIO_INDEX = 0;
  private int VIDEO_INDEX = 0;

  CobaltSampleStream(Allocator allocator, Format format) {
    this.allocator = allocator;
    // audioAllocator = new DefaultAllocator(true, 4000000 /* 4 MB */);
    // videoAllocator = new DefaultAllocator(true, 10000000 /* 10 MB */);
    sampleQueue = SampleQueue.createWithoutDrm(allocator);
    sampleQueue.format(format);
  }

  void writeSample(byte[] data, int sizeInBytes, long timestampUs) {
    ParsableByteArray array = new ParsableByteArray(data);
    sampleQueue.sampleData(array, sizeInBytes);
    sampleQueue.sampleMetadata(timestampUs, 0, sizeInBytes,0, null);
    Log.i(TAG, String.format("Appended sample for format: %s", sampleQueue.getUpstreamFormat().codecs));
  }

  @Override
  public boolean isReady() {
    return false;
  }

  @Override
  public void maybeThrowError() throws IOException {

  }

  @Override
  public int readData(FormatHolder formatHolder, DecoderInputBuffer buffer, int readFlags) {
    if (!sampleQueue.isReady(false)) {
      Log.e(TAG, "SampleQueue is not ready");
      return C.RESULT_NOTHING_READ;
    }
    return sampleQueue.read(formatHolder, buffer, readFlags, false);
  }

  @Override
  public int skipData(long positionUs) {
    return 0;
  }
}
