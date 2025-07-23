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

import org.chromium.base.annotations.JNINamespace;

import java.io.IOException;

import dev.cobalt.util.Log;

@JNINamespace("starboard::android::shared::exoplayer")
@UnstableApi
public class CobaltSampleStream implements SampleStream {
    private SampleQueue sampleQueue;
    public boolean prerolled = false;
    public boolean firstData = true;
    private boolean endOfStream = false;
    private Allocator allocator;
    private long samplesQueued = 0;

    CobaltSampleStream(Allocator allocator, Format format) {
        this.allocator = allocator;
        sampleQueue = SampleQueue.createWithoutDrm(allocator);
        sampleQueue.format(format);
    }

    void discardBuffer(long positionUs, boolean toKeyframe) {
        sampleQueue.discardTo(positionUs, toKeyframe, true);
    }

    void writeSample(byte[] data, int sizeInBytes, long timestampUs, boolean isKeyFrame,
            boolean isEndOfStream) {
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
            prerolled = true;
            Log.i(TAG, "Reached end of stream in SampleQueue, setting EOS buffer and returning");
            return;
        }

        ParsableByteArray array = new ParsableByteArray(data);

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
                            "Caught exception from sampleQueue.sampleMetadata() %s", e.toString()));
        }
        samplesQueued = samplesQueued + 1;
        if (!prerolled) {
            if (sampleQueue.getUpstreamFormat().sampleRate != Format.NO_VALUE) {
                // Audio stream
                prerolled = (prerolled || sampleQueue.getLargestQueuedTimestampUs() > 500000)
                        || endOfStream;
                if (prerolled) {
                    Log.i(TAG, "Prerolled audio stream");
                }
            } else {
                // Video stream
                prerolled = (prerolled || sampleQueue.getLargestQueuedTimestampUs() > 500000)
                        || endOfStream;
                if (prerolled) {
                    Log.i(TAG, "Prerolled video stream");
                }
            }
        }
    }

    @Override
    public boolean isReady() {
        return sampleQueue.isReady(endOfStream) || endOfStream;
    }

    @Override
    public void maybeThrowError() throws IOException {}

    @Override
    public int readData(FormatHolder formatHolder, DecoderInputBuffer buffer, int readFlags) {
        if (!sampleQueue.isReady(endOfStream)) {
            return C.RESULT_NOTHING_READ;
        }
        if (endOfStream && samplesQueued == 0) {
            Log.i(TAG, "REPORTING END OF STREAM FROM SAMPLEQUEUE");
            buffer.setFlags(C.BUFFER_FLAG_END_OF_STREAM);
            return C.RESULT_BUFFER_READ;
        }

        int read = -1;
        try {
            read = sampleQueue.read(formatHolder, buffer, readFlags, endOfStream);
        } catch (Exception e) {
            Log.i(TAG, String.format("Caught exception from read() %s", e.toString()));
        }
        if (read == C.RESULT_BUFFER_READ) {
            samplesQueued = samplesQueued - 1;
        }
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
