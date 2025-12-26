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


import androidx.annotation.Nullable;
import androidx.media3.common.C;
import androidx.media3.common.Format;
import androidx.media3.common.MediaItem;
import androidx.media3.common.MediaMetadata;
import androidx.media3.common.util.UnstableApi;
import androidx.media3.datasource.TransferListener;
import androidx.media3.exoplayer.source.BaseMediaSource;
import androidx.media3.exoplayer.source.MediaPeriod;
import androidx.media3.exoplayer.source.SinglePeriodTimeline;
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
        if (mediaPeriod == null) {
            mediaPeriod = new ExoPlayerMediaPeriod(format, allocator);
            return mediaPeriod;
        }
        throw new IllegalStateException(
                "Called MediaSource.createPeriod when the MediaPeriod already exists");
    }

    @Override
    public void releasePeriod(MediaPeriod mediaPeriod) {
        if (this.mediaPeriod != null) {
            if (mediaPeriod != this.mediaPeriod) {
                throw new IllegalStateException(
                        "Called MediaSource.releasePeriod on an unknown MediaPeriod");
            }
            // Ignore the passed-in MediaPeriod and call the ExoPlayerMediaPeriod directly. As
            // there's only a single MediaPeriod, this will match the passed MediaPeriod.
            this.mediaPeriod.destroySampleStream();
            this.mediaPeriod = null;
            return;
        }
        throw new IllegalStateException(
                "Called MediaSource.releasePeriod() after period was already released");
    }

    public void writeSample(byte[] samples, int size, long timestamp, boolean isKeyFrame) {
        mediaPeriod.writeSample(samples, size, timestamp, isKeyFrame);
    }

    public void writeEndOfStream() {
        mediaPeriod.writeEndOfStream();
    }

    public boolean canAcceptMoreData() {
        return !(mediaPeriod == null) || mediaPeriod.canAcceptMoreData();
    }
}
