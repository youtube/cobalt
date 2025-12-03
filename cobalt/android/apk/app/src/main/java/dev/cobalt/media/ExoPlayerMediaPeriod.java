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

import androidx.annotation.NonNull;
import androidx.media3.common.C;
import androidx.media3.common.Format;
import androidx.media3.common.TrackGroup;
import androidx.media3.exoplayer.LoadingInfo;
import androidx.media3.exoplayer.SeekParameters;
import androidx.media3.exoplayer.source.MediaPeriod;
import androidx.media3.exoplayer.source.SampleStream;
import androidx.media3.exoplayer.source.TrackGroupArray;
import androidx.media3.exoplayer.trackselection.ExoTrackSelection;
import androidx.media3.exoplayer.upstream.Allocator;
import java.io.IOException;

/** Implements the ExoPlayer MediaPeriod interface to write samples to the ExoPlayerSampleStream. */
public class ExoPlayerMediaPeriod implements MediaPeriod {
    private final Format format;
    private final Allocator allocator;
    private ExoPlayerSampleStream stream;

    ExoPlayerMediaPeriod(Format format, Allocator allocator) {
        this.format = format;
        this.allocator = allocator;
    }

    @Override
    public void prepare(Callback callback, long positionUs) {
        callback.onPrepared(this);
    }

    @Override
    public void maybeThrowPrepareError() throws IOException {}

    @Override
    public TrackGroupArray getTrackGroups() {
        return new TrackGroupArray(new TrackGroup(format));
    }

    @Override
    public long selectTracks(ExoTrackSelection[] selections, boolean[] mayRetainStreamFlags,
            SampleStream[] streams, boolean[] streamResetFlags, long positionUs) {
        if (stream != null) {
            throw new IllegalStateException(
                    "Tried to initialize the SampleStream after it has already been created");
        }

        for (int i = 0; i < selections.length; ++i) {
            if (selections[i] != null) {
                stream = new ExoPlayerSampleStream(allocator, selections[i].getSelectedFormat());
                streams[i] = stream;
                streamResetFlags[i] = true;
            }
        }
        return positionUs;
    }

    @Override
    public void discardBuffer(long positionUs, boolean toKeyframe) {
        stream.discardBuffer(positionUs, toKeyframe);
    }

    @Override
    public long readDiscontinuity() {
        return C.TIME_UNSET;
    }

    @Override
    public long seekToUs(long positionUs) {
        stream.seek(positionUs, format);
        return positionUs;
    }

    @Override
    public long getAdjustedSeekPositionUs(long positionUs, SeekParameters seekParameters) {
        return positionUs;
    }

    @Override
    public long getBufferedPositionUs() {
        return stream != null ? stream.getBufferedPositionUs() : 0l;
    }

    @Override
    public long getNextLoadPositionUs() {
        return getBufferedPositionUs();
    }

    @Override
    public boolean continueLoading(@NonNull LoadingInfo loadingInfo) {
        return stream == null || (!stream.endOfStreamWritten() && stream.canAcceptMoreData());
    }

    @Override
    public boolean isLoading() {
        return stream == null || !stream.endOfStreamWritten();
    }

    @Override
    public void reevaluateBuffer(long positionUs) {}

    public void destroySampleStream() {
        stream.destroy();
    }

    public void writeSample(byte[] samples, int size, long timestamp, boolean isKeyFrame) {
        stream.writeSample(samples, size, timestamp, isKeyFrame);
    }

    public void writeEndOfStream() {
        stream.writeEndOfStream();
    }

    public boolean canAcceptMoreData() {
        return stream.canAcceptMoreData();
    }
}
