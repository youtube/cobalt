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
import androidx.media3.common.MediaItem;
import androidx.media3.common.MediaMetadata;
import androidx.media3.common.TrackGroup;
import androidx.media3.common.util.UnstableApi;
import androidx.media3.datasource.TransferListener;
import androidx.media3.exoplayer.SeekParameters;
import androidx.media3.exoplayer.source.BaseMediaSource;
import androidx.media3.exoplayer.source.MediaPeriod;
import androidx.media3.exoplayer.source.SampleStream;
import androidx.media3.exoplayer.source.SinglePeriodTimeline;
import androidx.media3.exoplayer.source.TrackGroupArray;
import androidx.media3.exoplayer.trackselection.ExoTrackSelection;
import androidx.media3.exoplayer.upstream.Allocator;

import java.io.IOException;

import dev.cobalt.media.ExoPlayerRendererType;
import dev.cobalt.util.Log;

@UnstableApi
public final class CobaltMediaSource extends BaseMediaSource {
    private final Format format;
    private ExoMediaPeriod mediaPeriod;
    // Reference to the host ExoPlayerBridge
    private final ExoPlayerBridge playerBridge;
    private final MediaItem mediaItem;
    private final int rendererType;

    CobaltMediaSource(ExoPlayerBridge playerBridge, Format format, int rendererType) {
        this.playerBridge = playerBridge;
        this.format = format;
        this.rendererType = rendererType;

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
        mediaPeriod = new ExoMediaPeriod(format, allocator, playerBridge, rendererType);
        return mediaPeriod;
    }

    @Override
    public void releasePeriod(MediaPeriod mediaPeriod) {}

    public boolean writeSample(byte[] data, int sizeInBytes, long timestampUs, boolean isKeyFrame,
            boolean isEndOfStream) {
        return mediaPeriod.writeSample(data, sizeInBytes, timestampUs, isKeyFrame, isEndOfStream);
    }

    public boolean isInitialized() {
        return mediaPeriod.initialized;
    }

    private static final class ExoMediaPeriod implements MediaPeriod {
        private final Format format;
        private final Allocator allocator;
        private CobaltSampleStream stream;
        // Notify the player when initialized to avoid writing samples before the stream exists.
        public boolean initialized = false;
        private ExoPlayerBridge playerBridge;

        private boolean reachedEos = false;

        private boolean pendingDiscontinuity = false;
        private long discontinuityPositionUs;
        private final int rendererType;

        ExoMediaPeriod(Format format, Allocator allocator, ExoPlayerBridge playerBridge,
                int rendererType) {
            this.format = format;
            this.allocator = allocator;
            this.playerBridge = playerBridge;
            this.rendererType = rendererType;
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
                    stream = new CobaltSampleStream(allocator, selections[i].getSelectedFormat());
                    streams[i] = stream;
                    streamResetFlags[i] = true;
                }
            }
            initialized = true;
            playerBridge.onStreamCreated();
            return positionUs;
        }

        @Override
        public void discardBuffer(long positionUs, boolean toKeyframe) {
            stream.discardBuffer(positionUs, toKeyframe);
        }

        @Override
        public long readDiscontinuity() {
            if (pendingDiscontinuity) {
                long positionToReport = discontinuityPositionUs;
                pendingDiscontinuity = false;
                discontinuityPositionUs = C.TIME_UNSET;
                Log.i(TAG,
                        String.format("readDiscontinuity() reporting discontinuity at: %d",
                                positionToReport));
                return positionToReport;
            }
            return C.TIME_UNSET;
        }

        @Override
        public long seekToUs(long positionUs) {
            Log.i(TAG, String.format("ExoPlayer seeking to timestamp %d", positionUs));
            stream.seek(positionUs);
            reachedEos = false;
            pendingDiscontinuity = true;
            discontinuityPositionUs = positionUs;
            return positionUs;
        }

        @Override
        public long getAdjustedSeekPositionUs(long positionUs, SeekParameters seekParameters) {
            return positionUs;
        }

        @Override
        public long getBufferedPositionUs() {
            if (reachedEos) {
                return C.TIME_END_OF_SOURCE;
            }
            long pos = C.TIME_UNSET;
            if (stream != null) {
                pos = stream.getBufferedPositionUs();
            }
            pos = pos == C.TIME_UNSET ? 0 : pos;
            return pos;
        }

        @Override
        public long getNextLoadPositionUs() {
            long pos = getBufferedPositionUs();
            return pos;
        }

        @Override
        public boolean continueLoading(long loadingInfo) {
            return !reachedEos;
        }

        @Override
        public boolean isLoading() {
            return !reachedEos;
        }

        @Override
        public void reevaluateBuffer(long positionUs) {}

        public boolean writeSample(byte[] data, int sizeInBytes, long timestampUs,
                boolean isKeyFrame, boolean isEndOfStream) {
            try {
                if (isEndOfStream) {
                    reachedEos = true;
                }
            } catch (Exception e) {
                //
            }
            stream.writeSample(data, sizeInBytes, timestampUs, isKeyFrame, isEndOfStream);
            if (stream.prerolled) {
                playerBridge.onPrerolled(rendererType);
            }
            return true;
        }
    }
}
