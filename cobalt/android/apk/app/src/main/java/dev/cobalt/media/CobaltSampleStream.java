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
import androidx.media3.common.Format;
import androidx.media3.common.MimeTypes;
import androidx.media3.common.util.ParsableByteArray;
import androidx.media3.common.util.UnstableApi;
import androidx.media3.decoder.DecoderInputBuffer;
import androidx.media3.exoplayer.FormatHolder;
import androidx.media3.exoplayer.source.SampleQueue;
import androidx.media3.exoplayer.source.SampleStream;
import androidx.media3.exoplayer.upstream.Allocator;

import java.io.IOException;

import dev.cobalt.util.Log;

@UnstableApi
public class CobaltSampleStream implements SampleStream {
    private SampleQueue sampleQueue;
    private boolean endOfStream = false;
    private boolean wroteFirstSample = false;
    private long lastWrittenTimeUs = Long.MIN_VALUE;

    @Nullable
    private IOException fatalError;

    CobaltSampleStream(Allocator allocator, Format format) {
        sampleQueue = SampleQueue.createWithoutDrm(allocator);
        sampleQueue.format(format);
    }

    void discardBuffer(long positionUs, boolean toKeyframe) {
        Log.i(TAG, String.format("Called DISCARDBUFFER to %d, tokeyframe: %b", positionUs, toKeyframe));
        sampleQueue.discardTo(positionUs, toKeyframe, false);
    }

    synchronized void writeSample(byte[] data, int sizeInBytes, long timestampUs, boolean isKeyFrame,
            boolean isEndOfStream) {
        int sampleFlags = 0;
        if (isKeyFrame) {
            sampleFlags |= C.BUFFER_FLAG_KEY_FRAME;
        }
        if (isEndOfStream) {
            sampleFlags |= C.BUFFER_FLAG_END_OF_STREAM;
            Log.i(TAG, "WRITING END OF STREAM");
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
            Log.i(TAG,
                    String.format("Wrote %s sample with timestamp: %d, keyframe: %b",
                            sampleQueue.getUpstreamFormat().sampleRate != Format.NO_VALUE ? "audio"
                                                                                          : "video",
                            timestampUs, isKeyFrame));
        } catch (Exception e) {
            Log.i(TAG,
                    String.format(
                            "Caught exception from sampleQueue.sampleMetadata() %s", e.toString()));
        }

        if (lastWrittenTimeUs != Long.MIN_VALUE) {
            if (timestampUs <= lastWrittenTimeUs && !isEndOfStream) {
                Log.i(TAG, String.format("ERROR: Latest written timestamp %d is less than or equal to last written timestamp %d, with a delta of %d", timestampUs, lastWrittenTimeUs, lastWrittenTimeUs - timestampUs));
            }
        }
        timestampUs = lastWrittenTimeUs;
        wroteFirstSample = true;
        wroteFirstSample = true;
    }

    @Override
    public boolean isReady() {
        boolean queueIsEmpty = sampleQueue.getFirstTimestampUs() == Long.MIN_VALUE;
        String type = sampleQueue.getUpstreamFormat().sampleRate != Format.NO_VALUE ? "audio" : "video";
        Log.i(TAG, String.format("Checking if %s queue is ready: %b, first timestamp: %d, largest timestamp: %d", type, sampleQueue.isReady(endOfStream), sampleQueue.getFirstTimestampUs(), sampleQueue.getLargestQueuedTimestampUs()));
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

        Log.i(TAG,
                String.format("Read %s sample with timestamp: %d",
                        sampleQueue.getUpstreamFormat().sampleRate != Format.NO_VALUE ? "audio"
                                                                                      : "video",
                        buffer.timeUs));
        return read;
    }

    @Override
    public int skipData(long positionUs) {
        Log.i(TAG, String.format("Called SKIPDATA to %d", positionUs));
        int skipCount = sampleQueue.getSkipCount(positionUs, endOfStream);
        sampleQueue.skip(skipCount);
        return skipCount;
    }

    public long getBufferedPositionUs() {
        return sampleQueue.getLargestQueuedTimestampUs();
    }

    synchronized public void seek(long timestampUs, boolean seekToKeyFrame) {
        String type = sampleQueue.getUpstreamFormat().sampleRate != Format.NO_VALUE ? "audio" : "video";
        Log.i(TAG, String.format("Before discarding to %d, first timestamp is %d with key frame: %b on type: %s", timestampUs, sampleQueue.getFirstTimestampUs(), seekToKeyFrame, type));
            sampleQueue.discardTo(timestampUs, true, true);
            endOfStream = false;
            // The SbPlayer may call seek multiple times before playback begins. After the first
            // sample is written, we avoid resetting the sample queue start time.
            if (!wroteFirstSample && timestampUs != 0) {
                sampleQueue.setStartTimeUs(timestampUs);
            }
            lastWrittenTimeUs = sampleQueue.getFirstTimestampUs();
            Log.i(TAG, String.format("Set queue start time to %d, first timestamp is now %d with seek to key frame: %b on type: %s", timestampUs, sampleQueue.getFirstTimestampUs(), seekToKeyFrame, type));
        }
    }
