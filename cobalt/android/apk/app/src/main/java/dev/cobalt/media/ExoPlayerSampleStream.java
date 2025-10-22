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

import androidx.annotation.Nullable;
import androidx.media3.common.C;
import androidx.media3.common.DataReader;
import androidx.media3.common.Format;
import androidx.media3.common.util.UnstableApi;
import androidx.media3.decoder.DecoderInputBuffer;
import androidx.media3.exoplayer.FormatHolder;
import androidx.media3.exoplayer.source.SampleQueue;
import androidx.media3.exoplayer.source.SampleStream;
import androidx.media3.exoplayer.upstream.Allocator;
import dev.cobalt.util.Log;
import java.io.IOException;
import java.nio.ByteBuffer;

/** Queues encoded media to be retrieved by the player renderers */
@UnstableApi
public class ExoPlayerSampleStream implements SampleStream {
    private SampleQueue sampleQueue;
    private boolean endOfStream = false;
    private boolean wroteFirstSample = false;
    private long lastWrittenTimeUs = Long.MIN_VALUE;
    private final Object queueLock;

    @Nullable
    private IOException fatalError;

    private static final class BufferDataReader implements DataReader {
        private ByteBuffer sample;
        public BufferDataReader(ByteBuffer sample) {
            this.sample = sample;
        }

        @Override
        public int read(byte[] buffer, int offset, int length) {
            if (sample.remaining() == 0) {
                return C.RESULT_END_OF_INPUT;
            }
            int bytesToRead = Math.min(length, sample.remaining());

            sample.get(buffer, offset, bytesToRead);
            return bytesToRead;
        }
    };

    private boolean isVideo() {
        return sampleQueue.getUpstreamFormat().channelCount == Format.NO_VALUE;
    }

    ExoPlayerSampleStream(Allocator allocator, Format format, Object lock) {
        sampleQueue = SampleQueue.createWithoutDrm(allocator);
        sampleQueue.format(format);
        queueLock = lock;
    }

    void discardBuffer(long positionUs, boolean toKeyframe) {
        synchronized (queueLock) {
            sampleQueue.discardTo(positionUs, toKeyframe, false);
        }
    }

    void writeSamples(ByteBuffer samples, int[] sizes, long[] timestamps, boolean[] keyFrames,
            int sampleCount) {
        synchronized (queueLock) {
            for (int i = 0; i < sampleCount; ++i) {
                int sampleSize = sizes[i];
                long timestampUs = timestamps[i];
                boolean isKeyFrame = keyFrames[i];

                int sampleFlags = 0;
                if (isKeyFrame) {
                    sampleFlags |= C.BUFFER_FLAG_KEY_FRAME;
                }

                int currentPosition = samples.position();
                // Limit the buffer to the end of the current sample so that the sampleQueue doesn't
                // read beyond it.
                samples.limit(currentPosition + sampleSize);
                BufferDataReader dataReader = new BufferDataReader(samples);

                try {
                    int result = 0;
                    int remaining = sampleSize;
                    while (remaining > 0) {
                        result = sampleQueue.sampleData(dataReader, sampleSize, true);
                        remaining -= result;
                    }
                    sampleQueue.sampleMetadata(timestampUs, sampleFlags, sampleSize, 0, null);

                    samples.limit(samples.capacity());
                } catch (Exception e) {
                    Log.e(TAG, String.format("Error queueing sample %s", e.toString()));
                }
            }
        }
    }

    void writeEndOfStream() {
        sampleQueue.sampleMetadata(
                sampleQueue.getLargestQueuedTimestampUs(), C.BUFFER_FLAG_END_OF_STREAM, 0, 0, null);
    }


    @Override
    public boolean isReady() {
        boolean queueIsEmpty = sampleQueue.getFirstTimestampUs() == Long.MIN_VALUE;
        String type =
                sampleQueue.getUpstreamFormat().sampleRate != Format.NO_VALUE ? "audio" : "video";
        return sampleQueue.isReady(endOfStream);
    }

    @Override
    public void maybeThrowError() throws IOException {}

    @Override
    public int readData(FormatHolder formatHolder, DecoderInputBuffer buffer, int readFlags) {
        if (!sampleQueue.isReady(endOfStream)) {
            return C.RESULT_NOTHING_READ;
        }

        int read = -1;
        try {
            read = sampleQueue.read(formatHolder, buffer, readFlags, endOfStream);
        } catch (Exception e) {
            Log.i(TAG, String.format("Caught exception from read() %s", e.toString()));
        }
        return read;
    }

    @Override
    public int skipData(long positionUs) {
        synchronized (queueLock) {
            int skipCount = sampleQueue.getSkipCount(positionUs, endOfStream);
            sampleQueue.skip(skipCount);
            return skipCount;
        }
    }

    public long getBufferedPositionUs() {
        return sampleQueue.getLargestQueuedTimestampUs();
    }

    public void seek(long timestampUs, boolean seekToKeyFrame) {
        synchronized (queueLock) {
            sampleQueue.discardTo(timestampUs, true, true);
            endOfStream = false;

            lastWrittenTimeUs = Long.MIN_VALUE;
        }
    }

    public void destroy() {
        synchronized (queueLock) {
            sampleQueue.reset();
            sampleQueue.release();
        }
    }
}
