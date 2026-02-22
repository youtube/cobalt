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
import java.nio.ByteBuffer;

/** Implements the ExoPlayer MediaPeriod interface to write samples to the ExoPlayerSampleStream. */
public class ExoPlayerMediaPeriod implements MediaPeriod {
    private final Format mFormat;
    private final Allocator mAllocator;
    private ExoPlayerSampleStream mStream;

    ExoPlayerMediaPeriod(Format format, Allocator allocator) {
        this.mFormat = format;
        this.mAllocator = allocator;
    }

    /**
     * Prepares the media period.
     * @param callback The callback to be notified when preparation is complete.
     * @param positionUs The position in microseconds to prepare from.
     */
    @Override
    public void prepare(Callback callback, long positionUs) {
        callback.onPrepared(this);
    }

    /**
     * Throws an error if preparation has failed.
     * @throws IOException If an error occurred during preparation.
     */
    @Override
    public void maybeThrowPrepareError() throws IOException {}

    /**
     * Returns the track groups for this media period.
     * @return The track groups.
     */
    @Override
    public TrackGroupArray getTrackGroups() {
        return new TrackGroupArray(new TrackGroup(mFormat));
    }

    /**
     * Selects tracks for playback.
     * @param selections The track selections.
     * @param mayRetainStreamFlags Flags indicating whether the corresponding stream may be
     *         retained.
     * @param streams The streams to be updated with the selected tracks.
     * @param streamResetFlags Flags indicating whether the corresponding stream has been reset.
     * @param positionUs The position in microseconds to start playback from.
     * @return The actual position in microseconds where playback will start.
     */
    @Override
    public long selectTracks(ExoTrackSelection[] selections, boolean[] mayRetainStreamFlags,
            SampleStream[] streams, boolean[] streamResetFlags, long positionUs) {
        if (mStream != null) {
            throw new IllegalStateException(
                    "Tried to initialize the SampleStream after it has already been created");
        }

        for (int i = 0; i < selections.length; ++i) {
            if (selections[i] != null) {
                mStream = new ExoPlayerSampleStream(mAllocator, selections[i].getSelectedFormat());
                streams[i] = mStream;
                streamResetFlags[i] = true;
            }
        }
        return positionUs;
    }

    /**
     * Discards buffered data up to a specified position.
     * @param positionUs The position in microseconds to discard to.
     * @param toKeyframe Whether to discard to the keyframe before or at the specified position.
     */
    @Override
    public void discardBuffer(long positionUs, boolean toKeyframe) {
        if (mStream != null) {
            mStream.discardBuffer(positionUs, toKeyframe);
        }
    }

    /**
     * Reads a discontinuity, which can occur after seeking. This is ignored as it's handled in
     * native code.
     * @return The position of the discontinuity in microseconds, or C.TIME_UNSET if no
     *     discontinuity was read.
     */
    @Override
    public long readDiscontinuity() {
        return C.TIME_UNSET;
    }

    /**
     * Seeks to a specified position.
     * @param positionUs The position in microseconds to seek to.
     * @return The actual position in microseconds that was sought to.
     */
    @Override
    public long seekToUs(long positionUs) {
        mStream.seek(positionUs, mFormat);
        return positionUs;
    }

    /**
     * Returns the adjusted seek position, in cases where the requested seek position is not
     * available.
     * @param positionUs The requested seek position in microseconds.
     * @param seekParameters The seek parameters.
     * @return The adjusted seek position in microseconds.
     */
    @Override
    public long getAdjustedSeekPositionUs(long positionUs, SeekParameters seekParameters) {
        return positionUs;
    }

    /**
     * Returns the buffered position in microseconds.
     * @return The buffered position in microseconds.
     */
    @Override
    public long getBufferedPositionUs() {
        return mStream != null ? mStream.getBufferedPositionUs() : 0L;
    }

    /**
     * Returns the next load position in microseconds.
     * @return The next load position in microseconds.
     */
    @Override
    public long getNextLoadPositionUs() {
        return getBufferedPositionUs();
    }

    /**
     * Returns whether loading should continue.
     * @param loadingInfo The loading information.
     * @return Whether loading should continue.
     */
    @Override
    public boolean continueLoading(@NonNull LoadingInfo loadingInfo) {
        return mStream == null || (!mStream.endOfStreamWritten() && mStream.canAcceptMoreData());
    }

    /**
     * Returns whether the media period is currently loading.
     * @return Whether the media period is currently loading.
     */
    @Override
    public boolean isLoading() {
        return mStream == null || !mStream.endOfStreamWritten();
    }

    @Override
    public void reevaluateBuffer(long positionUs) {}

    public void destroySampleStream() {
        if (mStream != null) {
            mStream.destroy();
        }
    }

    /**
     * Writes a sample to the sample stream.
     * @param samples The sample data.
     * @param size The size of the sample data.
     * @param timestamp The timestamp of the sample in microseconds.
     * @param isKeyFrame Whether the sample is a keyframe.
     */
    public void writeSample(ByteBuffer samples, int size, long timestamp, boolean isKeyFrame) {
        mStream.writeSample(samples, size, timestamp, isKeyFrame);
    }

    public void writeEndOfStream() {
        mStream.writeEndOfStream();
    }

    public boolean canAcceptMoreData() {
        if (mStream == null) {
            return false;
        }
        return mStream.canAcceptMoreData();
    }
}
