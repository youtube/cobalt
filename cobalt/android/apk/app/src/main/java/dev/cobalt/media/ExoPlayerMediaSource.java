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
    private final Format mFormat;
    private ExoPlayerMediaPeriod mMediaPeriod;
    private final MediaItem mMediaItem;

    ExoPlayerMediaSource(Format format) {
        this.mFormat = format;
        this.mMediaItem = new MediaItem.Builder().setMediaMetadata(MediaMetadata.EMPTY).build();
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
        return this.mMediaItem;
    }

    /**
     * Throws an error if the source info could not be refreshed.
     * @throws IOException If an error occurred.
     */
    @Override
    public void maybeThrowSourceInfoRefreshError() throws IOException {}

    /**
     * Creates a media period for this media source.
     * @param id The media period id.
     * @param allocator The allocator to be used for the media period.
     * @param startPositionUs The start position in microseconds.
     * @return The media period.
     */
    @Override
    public MediaPeriod createPeriod(MediaPeriodId id, Allocator allocator, long startPositionUs) {
        if (mMediaPeriod == null) {
            mMediaPeriod = new ExoPlayerMediaPeriod(mFormat, allocator);
            return mMediaPeriod;
        }
        throw new IllegalStateException(
                "Called MediaSource.createPeriod when the MediaPeriod already exists");
    }

    /**
     * Releases a media period.
     * @param mediaPeriod The media period to be released.
     */
    @Override
    public void releasePeriod(MediaPeriod mediaPeriod) {
        if (this.mMediaPeriod != null) {
            if (mediaPeriod != this.mMediaPeriod) {
                throw new IllegalStateException(
                        "Called MediaSource.releasePeriod on an unknown MediaPeriod");
            }
            // Ignore the passed-in MediaPeriod and call the ExoPlayerMediaPeriod directly. As
            // there's only a single MediaPeriod, this will match the passed MediaPeriod.
            this.mMediaPeriod.destroySampleStream();
            this.mMediaPeriod = null;
            return;
        }
        throw new IllegalStateException(
                "Called MediaSource.releasePeriod() after period was already released");
    }

    /**
     * Writes a sample to the media period.
     * @param samples The sample data.
     * @param size The size of the sample data.
     * @param timestamp The timestamp of the sample in microseconds.
     * @param isKeyFrame Whether the sample is a keyframe.
     */
    public void writeSample(byte[] samples, int size, long timestamp, boolean isKeyFrame) {
        mMediaPeriod.writeSample(samples, size, timestamp, isKeyFrame);
    }

    public void writeEndOfStream() {
        mMediaPeriod.writeEndOfStream();
    }

    public boolean canAcceptMoreData() {
        return mMediaPeriod != null && mMediaPeriod.canAcceptMoreData();
    }
}
