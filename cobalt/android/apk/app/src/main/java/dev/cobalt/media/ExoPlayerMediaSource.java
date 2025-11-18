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
import androidx.annotation.Nullable;
import androidx.media3.common.C;
import androidx.media3.common.Format;
import androidx.media3.common.MediaItem;
import androidx.media3.common.MediaMetadata;
import androidx.media3.common.TrackGroup;
import androidx.media3.common.util.UnstableApi;
import androidx.media3.datasource.TransferListener;
import androidx.media3.exoplayer.LoadingInfo;
import androidx.media3.exoplayer.SeekParameters;
import androidx.media3.exoplayer.source.BaseMediaSource;
import androidx.media3.exoplayer.source.MediaPeriod;
import androidx.media3.exoplayer.source.SampleStream;
import androidx.media3.exoplayer.source.SinglePeriodTimeline;
import androidx.media3.exoplayer.source.TrackGroupArray;
import androidx.media3.exoplayer.trackselection.ExoTrackSelection;
import androidx.media3.exoplayer.upstream.Allocator;
import java.io.IOException;

/** Writes encoded media from the native app to the SampleStream */
@UnstableApi
public final class ExoPlayerMediaSource extends BaseMediaSource {
    private final Format format;
    private ExoPlayerMediaPeriod mediaPeriod;

    private final MediaItem mediaItem;

    ExoPlayerMediaSource(Format format) {
        this.format = format;
        this.mediaItem = new MediaItem.Builder().setMediaMetadata(MediaMetadata.EMPTY).build();
    }

    @Override
    protected void prepareSourceInternal(@Nullable TransferListener mediaTransferListener) {
        refreshSourceInfo(new SinglePeriodTimeline(
                /* durationUs= */ C.TIME_UNSET,
                /* isSeekable= */ true,
                /* isDynamic= */ false,
                /* useLiveConfiguration= */ false,
                /* manifest= */ null, getMediaItem()));
    }

    @Override
    protected void releaseSourceInternal() {}

    @Override
    public MediaItem getMediaItem() {
        return this.mediaItem;
    }

    @Override
    public void maybeThrowSourceInfoRefreshError() throws IOException {}

    @Override
    public MediaPeriod createPeriod(MediaPeriodId id, Allocator allocator, long startPositionUs) {
        mediaPeriod = new ExoPlayerMediaPeriod(format, allocator);
        return mediaPeriod;
    }

    @Override
    public void releasePeriod(MediaPeriod mediaPeriod) {
        // Ignore the passed-in MediaPeriod and call the ExoPlayerMediaPeriod directly. As there's
        // only a single MediaPeriod, this will match the passed MediaPeriod.
        this.mediaPeriod.destroySampleStream();
    }

    public void writeSample(byte[] samples, int size, long timestamp, boolean isKeyFrame) {
        mediaPeriod.writeSample(samples, size, timestamp, isKeyFrame);
    }

    public void writeEndOfStream() {
        mediaPeriod.writeEndOfStream();
    }

    public boolean canAcceptMoreData() {
        return mediaPeriod.canAcceptMoreData();
    }

    private static final class ExoPlayerMediaPeriod implements MediaPeriod {
        private final Format format;
        private final Allocator allocator;
        private ExoPlayerSampleStream stream;
        private long seekTimeUs = C.TIME_UNSET;

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
            for (int i = 0; i < selections.length; ++i) {
                if (selections[i] != null) {
                    stream =
                            new ExoPlayerSampleStream(allocator, selections[i].getSelectedFormat());
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
            if (seekTimeUs != C.TIME_UNSET) {
                long positionToReport = seekTimeUs;
                seekTimeUs = C.TIME_UNSET;
                return positionToReport;
            }
            return seekTimeUs;
        }

        @Override
        public long seekToUs(long positionUs) {
            stream.seek(positionUs, format);
            seekTimeUs = positionUs;
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
            return stream == null || !stream.endOfStreamWritten();
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
}
