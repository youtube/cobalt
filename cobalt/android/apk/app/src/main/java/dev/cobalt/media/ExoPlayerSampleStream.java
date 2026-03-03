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

import androidx.annotation.NonNull;
import androidx.media3.common.C;
import androidx.media3.common.DataReader;
import androidx.media3.common.Format;
import androidx.media3.decoder.DecoderInputBuffer;
import androidx.media3.exoplayer.FormatHolder;
import androidx.media3.exoplayer.source.SampleQueue;
import androidx.media3.exoplayer.source.SampleStream;
import androidx.media3.exoplayer.upstream.Allocator;
import dev.cobalt.util.Log;
import java.io.IOException;
import java.nio.ByteBuffer;

/**
 * Buffers and provides media samples to ExoPlayer's renderers.
 *
 * This stream maintains an internal {@link SampleQueue} and provides a thread-safe bridge
 * for the native layer to write encoded samples.
 */
public class ExoPlayerSampleStream implements SampleStream {
    // The player maintains a copy of each sample in the SampleQueue to read asynchronously.
    private final SampleQueue mSampleQueue;
    private final Object mLock = new Object();
    private volatile boolean mEndOfStream = false;

    /**
     * An implementation of {@link DataReader} that reads data from a {@link ByteBuffer}.
     * This allows ExoPlayer's {@link SampleQueue} to read media samples directly from
     * memory-backed buffers (including DirectByteBuffers from native code) without
     * requiring an intermediate byte array at the bridge level.
     */
    private static class ByteBufferDataReader implements DataReader {
        private final ByteBuffer mBuffer;

        public ByteBufferDataReader(ByteBuffer buffer) {
            mBuffer = buffer;
        }

        @Override
        public int read(@NonNull byte[] buffer, int offset, int length) {
            if (!mBuffer.hasRemaining()) {
                return C.RESULT_END_OF_INPUT;
            }
            int readLength = Math.min(length, mBuffer.remaining());
            mBuffer.get(buffer, offset, readLength);
            return readLength;
        }
    }

    private static final long MAX_BUFFER_DURATION_US = 5 * 1000 * 1000; // 5 seconds.
    // CanAcceptMoreData() returns false when the total queued media duration is more than
    // MAX_BUFFER_DURATION_US - MEMORY_PRESSURE_THRESHOLD_US. This allows time for the queue to
    // empty before it can accept more samples, without overfilling the queue.
    private static final long MEMORY_PRESSURE_THRESHOLD_US = 250 * 1000; //  250 milliseconds.

    ExoPlayerSampleStream(Allocator allocator, Format format) {
        mSampleQueue = SampleQueue.createWithoutDrm(allocator);
        mSampleQueue.format(format);
    }

    void discardBuffer(long positionUs, boolean toKeyframe) {
        synchronized (mLock) {
            mSampleQueue.discardTo(positionUs, toKeyframe, false);
        }
    }

    /**
     * Queues a sample to the {@link SampleQueue}.
     */
    public void writeSample(ExoPlayerMediaSample sample) {
        synchronized (mLock) {
            try {
                ByteBufferDataReader dataReader = new ByteBufferDataReader(sample.getSamples());
                int bytesWritten = 0;
                while (bytesWritten < sample.getSize()) {
                    int result =
                            mSampleQueue.sampleData(dataReader, sample.getSize() - bytesWritten, true);
                    if (result == C.RESULT_END_OF_INPUT) {
                        throw new IOException("Unexpected end of input from ByteBuffer");
                    }
                    bytesWritten += result;
                }
            } catch (IOException e) {
                Log.e(TAG, "Failed to write sample data to SampleQueue", e);
                return;
            }
            mSampleQueue.sampleMetadata(sample.getTimestampUsec(), sample.getFlags(),
                    sample.getSize(), 0, null);
        }
    }

    public void writeEndOfStream() {
        synchronized (mLock) {
            mSampleQueue.sampleMetadata(mSampleQueue.getLargestQueuedTimestampUs(),
                    C.BUFFER_FLAG_END_OF_STREAM, 0, 0, null);
            mEndOfStream = true;
        }
    }

    @Override
    public boolean isReady() {
        synchronized (mLock) {
            return mSampleQueue.isReady(mEndOfStream);
        }
    }

    @Override
    public void maybeThrowError() throws IOException {}

    @Override
    public int readData(
            @NonNull FormatHolder formatHolder, @NonNull DecoderInputBuffer buffer, int readFlags) {
        synchronized (mLock) {
            if (!mSampleQueue.isReady(mEndOfStream)) {
                return C.RESULT_NOTHING_READ;
            }

            try {
                return mSampleQueue.read(formatHolder, buffer, readFlags, mEndOfStream);
            } catch (DecoderInputBuffer.InsufficientCapacityException e) {
                Log.i(TAG,
                        String.format(
                                "Caught DecoderInputBuffer.InsufficientCapacityException from read() %s",
                                e));
            }

            return C.RESULT_NOTHING_READ;
        }
    }

    @Override
    public int skipData(long positionUs) {
        synchronized (mLock) {
            int skipCount = mSampleQueue.getSkipCount(positionUs, mEndOfStream);
            mSampleQueue.skip(skipCount);
            return skipCount;
        }
    }

    /**
     * Returns the timestamp of the most advanced queued buffer.
     * @return The buffered position in microseconds.
     */
    public long getBufferedPositionUs() {
        synchronized (mLock) {
            return mEndOfStream ? C.TIME_END_OF_SOURCE : mSampleQueue.getLargestQueuedTimestampUs();
        }
    }

    /**
     * Attempts to seek within the currently queued samples.
     *
     * If the seek position is not found in the queue, the queue is reset.
     */
    public void seek(long timestampUs, Format format) {
        synchronized (mLock) {
            if (!mSampleQueue.seekTo(timestampUs, true)) {
                mSampleQueue.reset();
                mSampleQueue.format(format);
            }
            mEndOfStream = false;
        }
    }

    public void destroy() {
        synchronized (mLock) {
            mSampleQueue.reset();
            mSampleQueue.release();
        }
    }

    /**
     * Returns true if the queue has enough capacity to accept more samples.
     *
     * Capacity is determined by the duration of media currently buffered in the queue. If the
     * duration exceeds a threshold, this returns false to signal the native layer to pause
     * sample production.
     */
    public boolean canAcceptMoreData() {
        synchronized (mLock) {
            if (mSampleQueue.getWriteIndex() == mSampleQueue.getReadIndex()) {
                return true;
            }
            long bufferedDurationUs =
                    mSampleQueue.getLargestQueuedTimestampUs() - mSampleQueue.getFirstTimestampUs();
            return bufferedDurationUs < MAX_BUFFER_DURATION_US - MEMORY_PRESSURE_THRESHOLD_US;
        }
    }

    public boolean endOfStreamWritten() {
        return mEndOfStream;
    }
}
