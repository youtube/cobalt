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

import static androidx.media3.extractor.TrackOutput.SAMPLE_DATA_PART_MAIN;
import static dev.cobalt.media.Log.TAG;

import androidx.annotation.Nullable;
import androidx.media3.common.C;
import androidx.media3.common.DataReader;
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
            int bytesToRead = Math.min(length, sample.remaining());
            Log.i(TAG, String.format(
                            "Will read %d bytes, calculated from length: %d and remaining %d",
                            bytesToRead, length, sample.remaining()));
            sample.get(buffer, offset, bytesToRead);
            return bytesToRead;
        }
    };

    ExoPlayerSampleStream(Allocator allocator, Format format, Object lock) {
        sampleQueue = SampleQueue.createWithoutDrm(allocator);
        sampleQueue.format(format);
        queueLock = lock;
    }

    void discardBuffer(long positionUs, boolean toKeyframe) {
        synchronized (queueLock) {
            Log.i(TAG, "CALLING DISCARDBUFFER");
            sampleQueue.discardTo(positionUs, toKeyframe, false);
        }
    }

    void writeSamples(ByteBuffer samples, int[] sizes, long[] timestamps, boolean[] keyFrames, int sampleCount) {
        synchronized (queueLock) {
            DataReader dataReader = new BufferDataReader(samples);
            Log.i(TAG, String.format("GOT SAMPLE. CAPACITY %d, METADATA SIZE: %d, REMAINING: %d",
                samples.capacity(), sizes[0], samples.remaining()));
            // DataReader dataReader = new DataReader() {
            //     @Override
            //     public int read(byte[] buffer, int offset, int length) throws IOException {
            //         int bytesToRead = Math.min(length, samples.remaining());
            //         samples.get(buffer, offset, bytesToRead);
            //         Log.i(TAG, String.format(
            //             "Will read %d bytes, calculated from length: %d and remaining %d",
            //             bytesToRead, length, samples.remaining()));
            //         return bytesToRead;
            //     }
            // };

            for (int i = 0; i < sampleCount; ++i) {
                int sampleSize = sizes[i];
                long timestampUs = timestamps[i];
                boolean isKeyFrame = keyFrames[i];

                // int currentPosition = samples.position();
                // samples.limit(currentPosition + sampleSize);
                //
                // ByteBuffer sample = samples.slice();
                //
                // samples.limit(samples.capacity());
                //
                // samples.position(currentPosition + sampleSize);
                //
                // DataReader dataReader = new BufferDataReader(sample);

                int sampleFlags = 0;
                if (isKeyFrame) {
                    sampleFlags |= C.BUFFER_FLAG_KEY_FRAME;
                }

                try {
                    Log.i(TAG,
                        String.format("WRITING TIMESTAMP %d WITH FLAGS %d AND SAMPLE SIZE %d",
                            timestampUs, sampleFlags, sampleSize));
                    sampleQueue.sampleData(dataReader, sampleSize, false, SAMPLE_DATA_PART_MAIN);
                    sampleQueue.sampleMetadata(timestampUs, sampleFlags, sampleSize, 0, null);
                } catch (Exception e) {
                    Log.e(TAG, String.format("Error queueing sample %s", e.toString()));
                }
            }
        }
    }

    void writeEndOfStream() {
        Log.i(TAG, "CALLING WRITEENDOFSTREAM");
        sampleQueue.sampleMetadata(sampleQueue.getLargestQueuedTimestampUs(), C.BUFFER_FLAG_END_OF_STREAM, 0, 0, null);
    }

    synchronized void writeSample(byte[] data, int sizeInBytes, long timestampUs,
            boolean isKeyFrame, boolean isEndOfStream) {
        Log.i(TAG, "CALLING WRITESAMPLE WRONG!!!!!!!!!!!!!");
        int sampleFlags = 0;
        if (isKeyFrame) {
            sampleFlags |= C.BUFFER_FLAG_KEY_FRAME;
        }
        if (isEndOfStream) {
            sampleFlags |= C.BUFFER_FLAG_END_OF_STREAM;
            endOfStream = true;
        }

        byte[] dataToWrite = data;

        ParsableByteArray array = isEndOfStream ? null : new ParsableByteArray(dataToWrite);

        try {
            sampleQueue.sampleData(array, sizeInBytes);
        } catch (Exception e) {
            Log.i(TAG,
                    String.format(
                            "Caught exception from sampleQueue.sampleData() %s", e.toString()));
        }
        try {
            sampleQueue.sampleMetadata(timestampUs, sampleFlags, sizeInBytes, 0, null);
        } catch (Exception e) {
            Log.i(TAG,
                    String.format(
                            "Caught exception from sampleQueue.sampleMetaData() %s", e.toString()));
        }

        if (lastWrittenTimeUs != Long.MIN_VALUE) {
            if (timestampUs <= lastWrittenTimeUs && !isEndOfStream) {
                Log.i(TAG,
                        String.format(
                                "Latest written timestamp %d is less than or equal to last written timestamp %d, with a delta of %d",
                                timestampUs, lastWrittenTimeUs, lastWrittenTimeUs - timestampUs));
            }
        }
        lastWrittenTimeUs = timestampUs;
        wroteFirstSample = true;
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
            Log.i(TAG, "CALLING SKIPDATA");
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
            Log.i(TAG, "CALLING SEEK");
            sampleQueue.discardTo(timestampUs, true, true);
            endOfStream = false;

            lastWrittenTimeUs = Long.MIN_VALUE;
        }
    }

    public void destroy() {
        synchronized (queueLock) {
            Log.i(TAG, "CALLING DESTROY");
            sampleQueue.reset();
            sampleQueue.release();
        }
    }
}
