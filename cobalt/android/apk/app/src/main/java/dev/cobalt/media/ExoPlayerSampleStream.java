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
import androidx.annotation.Nullable;
import androidx.media3.common.C;
import androidx.media3.common.Format;
import androidx.media3.common.util.ParsableByteArray;
import androidx.media3.common.util.UnstableApi;
import androidx.media3.decoder.DecoderInputBuffer;
import androidx.media3.exoplayer.FormatHolder;
import androidx.media3.exoplayer.drm.DrmSessionEventListener;
import androidx.media3.exoplayer.drm.DrmSessionManager;
import androidx.media3.exoplayer.source.SampleQueue;
import androidx.media3.exoplayer.source.SampleStream;
import androidx.media3.exoplayer.upstream.Allocator;
import androidx.media3.extractor.TrackOutput;
import androidx.media3.extractor.TrackOutput.CryptoData;
import dev.cobalt.util.Log;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;

/** Queues encoded media to be retrieved by the player renderers */
@UnstableApi
public class ExoPlayerSampleStream implements SampleStream {
    // The player maintains a copy of each sample in the SampleQueue to read asynchronously.
    private final SampleQueue mSampleQueue;
    private volatile boolean mEndOfStream = false;
    private final ParsableByteArray mSampleData = new ParsableByteArray();

    // The memory here is managed by Java rather than the native allocator, which may increase
    // memory pressure.
    // TODO: Have the SampleQueue read directly from native memory, rather than manage its own
    // memory.
    private static final long MAX_BUFFER_DURATION_US = 5 * 1000 * 1000; // 5 seconds.
    // CanAcceptMoreData() returns false when the total queued media duration is more than
    // MAX_BUFFER_DURATION_US - MEMORY_PRESSURE_THRESHOLD_US. This allows time for the queue to
    // empty before it can accept more samples, without overfilling the queue.
    private static final long MEMORY_PRESSURE_THRESHOLD_US = 250 * 1000; //  250 milliseconds.

    ExoPlayerSampleStream(Allocator allocator, Format format) {
        mSampleQueue = SampleQueue.createWithoutDrm(allocator);
        mSampleQueue.format(format);
    }

    ExoPlayerSampleStream(Allocator allocator, Format format, DrmSessionManager drmSessionManager,
            DrmSessionEventListener.EventDispatcher eventDispatcher) {
        mSampleQueue = SampleQueue.createWithDrm(allocator, drmSessionManager, eventDispatcher);
        mSampleQueue.format(format);
    }

    void discardBuffer(long positionUs, boolean toKeyframe) {
        mSampleQueue.discardTo(positionUs, toKeyframe, false);
    }

    /**
     * Queues samples to decode, along with related flags and relevant metadata.
     * @param samples The sample data.
     * @param size The size of the sample data.
     * @param timestamp The timestamp of the sample in microseconds.
     * @param isKeyFrame Whether the sample is a keyframe.
     */
    public void writeSample(@NonNull byte[] samples, int size, long timestamp, boolean isKeyFrame) {
        writeSample(samples, size, timestamp, isKeyFrame, null, null, 0, null, null);
    }

    /**
     * Queues encrypted samples to decode, along with related flags and relevant metadata.
     * @param samples The sample data.
     * @param size The size of the sample data.
     * @param timestamp The timestamp of the sample in microseconds.
     * @param isKeyFrame Whether the sample is a keyframe.
     * @param cryptoData The information required to decrypt this sample.
     * @param initializationVector The initialization vector for this sample.
     * @param ivSize The size of the initialization vector.
     */
    public void writeSample(@NonNull byte[] samples, int size, long timestamp, boolean isKeyFrame,
            @Nullable CryptoData cryptoData, @Nullable byte[] initializationVector, int ivSize,
            @Nullable int[] subsampleEncryptedBytes, @Nullable int[] subsampleClearBytes) {
        int flags = 0;
        if (isKeyFrame) {
            flags |= C.BUFFER_FLAG_KEY_FRAME;
        }

        int totalSize = size;
        if (cryptoData != null) {
            flags |= C.BUFFER_FLAG_ENCRYPTED;
            ParsableByteArray encryptionSignalByte = new ParsableByteArray(1);
            // If subsamples are present, set the 0x80 bit.
            boolean hasSubsamples = subsampleEncryptedBytes != null && subsampleClearBytes != null && subsampleEncryptedBytes.length > 0;
            encryptionSignalByte.getData()[0] = (byte) (ivSize | (hasSubsamples ? 0x80 : 0));
            encryptionSignalByte.setPosition(0);
            mSampleQueue.sampleData(encryptionSignalByte, 1, TrackOutput.SAMPLE_DATA_PART_ENCRYPTION);
            totalSize++;

            ParsableByteArray ivData = new ParsableByteArray(initializationVector, ivSize);
            mSampleQueue.sampleData(ivData, ivSize, TrackOutput.SAMPLE_DATA_PART_ENCRYPTION);
            totalSize += ivSize;

            if (hasSubsamples) {
                int subsampleCount = subsampleEncryptedBytes.length;
                ParsableByteArray subsampleCountData = new ParsableByteArray(2);
                subsampleCountData.getData()[0] = (byte) (subsampleCount >> 8);
                subsampleCountData.getData()[1] = (byte) (subsampleCount & 0xFF);
                subsampleCountData.setPosition(0);
                mSampleQueue.sampleData(subsampleCountData, 2, TrackOutput.SAMPLE_DATA_PART_ENCRYPTION);
                totalSize += 2;

                int subsampleDataLength = 6 * subsampleCount;
                ByteBuffer subsampleBuffer = ByteBuffer.allocate(subsampleDataLength);
                for (int i = 0; i < subsampleCount; i++) {
                    subsampleBuffer.putShort((short) subsampleClearBytes[i]);
                    subsampleBuffer.putInt(subsampleEncryptedBytes[i]);
                }
                ParsableByteArray subsampleData = new ParsableByteArray(subsampleBuffer.array());
                mSampleQueue.sampleData(subsampleData, subsampleDataLength, TrackOutput.SAMPLE_DATA_PART_ENCRYPTION);
                totalSize += subsampleDataLength;
            }
        }

        mSampleData.reset(samples, size);
        mSampleQueue.sampleData(mSampleData, size);
        mSampleQueue.sampleMetadata(timestamp, flags, totalSize, 0, cryptoData);
    }

    public void writeEndOfStream() {
        mSampleQueue.sampleMetadata(mSampleQueue.getLargestQueuedTimestampUs(),
                C.BUFFER_FLAG_END_OF_STREAM, 0, 0, null);
        mEndOfStream = true;
    }

    /**
     * Returns whether the sample queue is ready to provide samples.
     * @return Whether the sample queue is ready.
     */
    @Override
    public boolean isReady() {
        return mSampleQueue.isReady(mEndOfStream);
    }

    /**
     * Throws an error if the sample stream has failed.
     * @throws IOException If an error occurred.
     */
    @Override
    public void maybeThrowError() throws IOException {}

    /**
     * Reads data from the sample queue.
     * @param formatHolder A holder for the format of the data.
     * @param buffer The buffer to write the data to.
     * @param readFlags Flags controlling the read operation.
     * @return The result of the read operation.
     */
    @Override
    public int readData(
            @NonNull FormatHolder formatHolder, @NonNull DecoderInputBuffer buffer, int readFlags) {
        if (!mSampleQueue.isReady(mEndOfStream)) {
            return C.RESULT_NOTHING_READ;
        }

        int read = C.RESULT_NOTHING_READ;
        try {
            read = mSampleQueue.read(formatHolder, buffer, readFlags, mEndOfStream);
        } catch (DecoderInputBuffer.InsufficientCapacityException e) {
            Log.i(TAG,
                    String.format(
                            "Caught DecoderInputBuffer.InsufficientCapacityException from read() %s",
                            e));
        }

        return read;
    }

    /**
     * Skips data in the sample queue.
     * @param positionUs The position in microseconds to skip to.
     * @return The number of samples skipped.
     */
    @Override
    public int skipData(long positionUs) {
        int skipCount = mSampleQueue.getSkipCount(positionUs, mEndOfStream);
        mSampleQueue.skip(skipCount);
        return skipCount;
    }

    /**
     * Returns the timestamp of the most advanced queued buffer.
     * @return The buffered position in microseconds.
     */
    public long getBufferedPositionUs() {
        return mEndOfStream ? C.TIME_END_OF_SOURCE : mSampleQueue.getLargestQueuedTimestampUs();
    }

    /**
     * Attempts to seek within all queued samples to the seek position. If this fails, the queue is
     * reset to prepare for new samples.
     * @param timestampUs The position in microseconds to seek to.
     * @param format The format of the samples.
     */
    public void seek(long timestampUs, Format format) {
        if (!mSampleQueue.seekTo(timestampUs, true)) {
            mSampleQueue.reset();
            mSampleQueue.format(format);
        }
        mEndOfStream = false;
    }

    public void destroy() {
        mSampleQueue.reset();
        mSampleQueue.release();
    }

    /**
     * Returns true if the queue can accept more samples. Returns false if the queue size approaches
     * its maximum to allow time for samples to be dequeued before queueing new ones.
     * @return Whether the queue can accept more data.
     */
    public boolean canAcceptMoreData() {
        long bufferedDurationUs =
                mSampleQueue.getLargestQueuedTimestampUs() - mSampleQueue.getFirstTimestampUs();
        return bufferedDurationUs < MAX_BUFFER_DURATION_US - MEMORY_PRESSURE_THRESHOLD_US;
    }

    public boolean endOfStreamWritten() {
        return mEndOfStream;
    }
}
