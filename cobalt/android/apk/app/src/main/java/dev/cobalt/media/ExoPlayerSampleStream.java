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

import androidx.annotation.NonNull;
import androidx.media3.common.C;
import androidx.media3.common.Format;
import androidx.media3.common.util.ParsableByteArray;
import androidx.media3.common.util.UnstableApi;
import androidx.media3.decoder.DecoderInputBuffer;
import androidx.media3.exoplayer.FormatHolder;
import androidx.media3.exoplayer.source.SampleQueue;
import androidx.media3.exoplayer.source.SampleStream;
import androidx.media3.exoplayer.upstream.Allocator;
import dev.cobalt.util.Log;
import java.io.IOException;

/** Queues encoded media to be retrieved by the player renderers */
@UnstableApi
public class ExoPlayerSampleStream implements SampleStream {
    // The player maintains a copy of each sample in the SampleQueue to read asynchronously.
    private final SampleQueue sampleQueue;
    private volatile boolean endOfStream = false;
    private final ParsableByteArray sampleData = new ParsableByteArray();
    // The memory here is managed by Java rather than the native allocator, which may increase
    // memory pressure.
    // TODO: Have the SampleQueue read directly from native memory, rather than manage its own
    // memory.
    private static final long MAX_BUFFER_DURATION_US = 5 * 1000 * 1000; // 5 seconds.
    private static final long MEMORY_PRESSURE_THRESHOLD_US = 250 * 1000; //  250 milliseconds.

    ExoPlayerSampleStream(Allocator allocator, Format format) {
        sampleQueue = SampleQueue.createWithoutDrm(allocator);
        sampleQueue.format(format);
    }

    void discardBuffer(long positionUs, boolean toKeyframe) {
        sampleQueue.discardTo(positionUs, toKeyframe, false);
    }

    public void writeSample(byte[] samples, int size, long timestamp, boolean isKeyFrame) {
        sampleData.reset(samples, size);
        int flags = 0;
        if (isKeyFrame) {
            flags |= C.BUFFER_FLAG_KEY_FRAME;
        }

        // TODO: Optimize by avoiding an extra sample copy here.
        sampleQueue.sampleData(sampleData, size);
        sampleQueue.sampleMetadata(timestamp, flags, size, 0, null);
    }

    public void writeEndOfStream() {
        sampleQueue.sampleMetadata(
                sampleQueue.getLargestQueuedTimestampUs(), C.BUFFER_FLAG_END_OF_STREAM, 0, 0, null);
        endOfStream = true;
    }

    @Override
    public boolean isReady() {
        return sampleQueue.isReady(endOfStream);
    }

    @Override
    public void maybeThrowError() throws IOException {}

    @Override
    public int readData(
            @NonNull FormatHolder formatHolder, @NonNull DecoderInputBuffer buffer, int readFlags) {
        if (!sampleQueue.isReady(endOfStream)) {
            return C.RESULT_NOTHING_READ;
        }

        int read = C.RESULT_NOTHING_READ;
        try {
            read = sampleQueue.read(formatHolder, buffer, readFlags, endOfStream);
        } catch (DecoderInputBuffer.InsufficientCapacityException e) {
            Log.i(TAG,
                    String.format(
                            "Caught DecoderInputBuffer.InsufficientCapacityException from read() %s",
                            e));
        }
        return read;
    }

    @Override
    public int skipData(long positionUs) {
        int skipCount = sampleQueue.getSkipCount(positionUs, endOfStream);
        sampleQueue.skip(skipCount);
        return skipCount;
    }

    public long getBufferedPositionUs() {
        return endOfStream ? C.TIME_END_OF_SOURCE : sampleQueue.getLargestQueuedTimestampUs();
    }

    public void seek(long timestampUs, Format format) {
        if (!sampleQueue.seekTo(timestampUs, true)) {
            sampleQueue.reset();
            sampleQueue.format(format);
        }
        endOfStream = false;
    }

    public void destroy() {
        sampleQueue.release();
    }

    public boolean canAcceptMoreData() {
        long bufferedDurationUs =
                sampleQueue.getLargestQueuedTimestampUs() - sampleQueue.getFirstTimestampUs();
        return bufferedDurationUs < MAX_BUFFER_DURATION_US - MEMORY_PRESSURE_THRESHOLD_US;
    }

    public boolean endOfStreamWritten() {
        return endOfStream;
    }
}
