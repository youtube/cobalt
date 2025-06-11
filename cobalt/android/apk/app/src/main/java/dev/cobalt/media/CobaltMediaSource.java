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

import android.media.AudioFormat;
import android.net.Uri;
import android.telecom.Call;
import androidx.annotation.Nullable;
import androidx.media3.common.C;
import androidx.media3.common.Format;
import androidx.media3.common.MediaItem;
import androidx.media3.common.MediaMetadata;
import androidx.media3.common.MimeTypes;
import androidx.media3.common.Timeline;
import androidx.media3.common.TrackGroup;
import androidx.media3.common.util.NullableType;
import androidx.media3.common.util.UnstableApi;
import androidx.media3.datasource.TransferListener;
import androidx.media3.decoder.DecoderInputBuffer;
import androidx.media3.exoplayer.FormatHolder;
import androidx.media3.exoplayer.LoadingInfo;
import androidx.media3.exoplayer.SeekParameters;
import androidx.media3.exoplayer.source.BaseMediaSource;
import androidx.media3.exoplayer.source.MediaPeriod;
import androidx.media3.exoplayer.source.SampleStream;
import androidx.media3.exoplayer.source.SampleQueue;
import androidx.media3.exoplayer.source.SinglePeriodTimeline;
import androidx.media3.exoplayer.source.TrackGroupArray;
import androidx.media3.exoplayer.trackselection.ExoTrackSelection;
import androidx.media3.exoplayer.upstream.Allocator;
import dev.cobalt.util.Log;
import dev.cobalt.media.ExoPlayerBridge;
import dev.cobalt.media.ExoPlayerFormatCreator;
import java.io.IOException;
import dev.cobalt.util.UsedByNative;
import java.time.chrono.MinguoEra;
import java.util.Arrays;
import java.util.List;
import java.util.concurrent.BlockingQueue;
import java.util.concurrent.LinkedBlockingQueue;

@UnstableApi
public final class CobaltMediaSource extends BaseMediaSource {
  // TODO: Dynamically select format.
  private Format audioFormat;
  private Format videoFormat;
  private Format format;
  private CustomMediaPeriod mediaPeriod;
  private boolean isAudio;
  // Reference to the host ExoPlayerBridge
  private ExoPlayerBridge playerBridge;
  private MediaItem mediaItem;
  CobaltMediaSource(boolean isAudio, ExoPlayerBridge playerBridge, byte[] audioConfigurationData, Format format) {
    this.playerBridge = playerBridge;
    this.format = format;

    this.mediaItem = new MediaItem.Builder()
        // .setUri(uri)
        .setMediaMetadata(MediaMetadata.EMPTY)
        // .setMimeType(videoMimeType)
        // .setAudioMimeType(audioMimeType)
        .build();
    this.isAudio = isAudio;
  }

  @Override
  protected void prepareSourceInternal(@Nullable TransferListener mediaTransferListener) {
    // Start loading generic information about the media (think: manifests/metadata).
    // Once ready, call this method (and populate any data you care about):
    refreshSourceInfo(
        new SinglePeriodTimeline(
            /* durationUs= */ 92233720391l, // ~25.6 hours
            /* isSeekable= */ true,
            /* isDynamic= */ false,
            /* useLiveConfiguration= */ false,
            /* manifest= */ null,
            getMediaItem()));
    // refreshSourceInfo(Timeline.EMPTY);
  }

  @Override
  protected void releaseSourceInternal() {
    // Release any pending resources you hold from the prepareSourceInternal step above.
  }

  @Override
  public MediaItem getMediaItem() {
    // If needed, return a value that can be obtained via Player.getCurrentMediaItem();
    // Log.i(TAG, "Called getMediaItem()");
    return this.mediaItem;
  }

  @Override
  public void maybeThrowSourceInfoRefreshError() throws IOException {
    // Throw any pending exceptions that prevents the class from creating a MediaPeriod (if any).
  }

  @Override
  public MediaPeriod createPeriod(MediaPeriodId id, Allocator allocator, long startPositionUs) {
    // Log.i(TAG, "Called createPeriod");
    // Log.i(TAG, String.format("Allocation size is %d", allocator.getIndividualAllocationLength()));
    // Format format = isAudio ? audioFormat : videoFormat;
    mediaPeriod = new CustomMediaPeriod(format, allocator, playerBridge, isAudio);
    return mediaPeriod;
  }

  @Override
  public void releasePeriod(MediaPeriod mediaPeriod) {
    // Release any resources related to that MediaPeriod, its media loading, etc.
    // TODO: Clear stream here
  }

  public boolean writeSample(byte[] data, int sizeInBytes, long timestampUs, boolean isKeyFrame, boolean isEndOfStream) {
    return mediaPeriod.writeSample(data, sizeInBytes, timestampUs, isKeyFrame, isEndOfStream);
  }

  public boolean isInitialized() {
    return mediaPeriod.initialized;
  }

  private static final class CustomMediaPeriod implements MediaPeriod {
    private final Format format;
    private final Allocator allocator;
    private CobaltSampleStream stream;
    // Notify the player when initialized to avoid writing samples before the stream exists.
    public boolean initialized = false;
    private ExoPlayerBridge playerBridge;

    private boolean reachedEos = false;

    private boolean firstCall = true;
    private Callback callback = null;
    private boolean isAudio = false;
    private boolean pendingDiscontinuity = false;
    private long discontinuityPositionUs;

    private static class SampleData {
      public final byte[] data;
      public final int sizeInBytes;
      public final long timeUs;
      public final boolean keyFrame;
      public final boolean eos;

      public SampleData(byte[] data, int sizeInBytes, long timeUs, boolean keyFrame, boolean eos) {
        this.data = data;
        this.sizeInBytes = sizeInBytes;
        this.timeUs = timeUs;
        this.keyFrame = keyFrame;
        this.eos = eos;
      }
    }

    private final BlockingQueue<SampleData> pendingSamples = new LinkedBlockingQueue<>();
    // private final SampleQueue sampleQueue;

    CustomMediaPeriod(Format format, Allocator allocator, ExoPlayerBridge playerBridge, boolean isAudio) {
      this.format = format;
      this.allocator = allocator;
      this.playerBridge = playerBridge;
      this.isAudio = isAudio;
    }
    @Override
    public void prepare(Callback callback, long positionUs) {
      // Log.i(TAG, "Called CobaltMediaSource.CustomMediaPeriod.prepare()");
      this.callback = callback;
      // Start loading media from positionUs.
      // Once the list of Formats (see getTrackGroups()) is available, call:
      callback.onPrepared(this);
    }

    @Override
    public void maybeThrowPrepareError() throws IOException {
      // Throw any pending exceptions that prevent you from reaching callback.onPrepared().
    }

    @Override
    public TrackGroupArray getTrackGroups() {
      // Publish all the tracks in the media. Formats in the same group are "adaptive", so that the
      // player can choose between them based on network conditions. If this is not needed, just
      // publish single-Format TrackGroups.
      return new TrackGroupArray(new TrackGroup(format));
    }

    @Override
    public long selectTracks(@NullableType ExoTrackSelection[] selections,
        boolean[] mayRetainStreamFlags, @NullableType SampleStream[] streams,
        boolean[] streamResetFlags, long positionUs) {
      // The Player's TrackSelector selects a subset of getTrackGroups() for playback based on
      // device support and user preferences. This can be customized with DefaultTrackSelector
      // parameters. The "selections" are the requests for new SampleStreams for these formats.
      //  - mayRetainStreamFlags tells you if it's save to keep a previous stream
      //  - streamResetFlags are an output parameter to indicate which streams are newly created
      // Log.i(TAG, String.format("Selections size is %d", selections.length));
      for (int i = 0; i < selections.length; ++i) {
        if (selections[i] != null) {
          // Log.i(TAG, String.format("Stream %d (%s) format is %s", i, selections[i].getSelectedFormat().codecs,
          //     selections[i].getSelectedFormat().sampleMimeType));
          // Log.i(TAG, "Created SampleStream");
          stream = new CobaltSampleStream(allocator, selections[i].getSelectedFormat());
          streams[i] = stream;
          streamResetFlags[i] = true;
        }
      }
      // Log.i(TAG, String.format("positionUs is %d", positionUs));
      initialized = true;
      playerBridge.onStreamCreated();
      return positionUs;
    }

    @Override
    public void discardBuffer(long positionUs, boolean toKeyframe) {
      // Discard data from the streams up to positionUs after it's been played.
      // Log.i(TAG, "Called discardBuffer()");
      stream.discardBuffer(positionUs, toKeyframe);
    }

    @Override
    public long readDiscontinuity() {
      // Log.i(TAG, "Called readDiscontinuity()");
      if (pendingDiscontinuity) {
        long positionToReport = discontinuityPositionUs;
        pendingDiscontinuity = false;
        discontinuityPositionUs = C.TIME_UNSET;
        Log.i(TAG, String.format("readDiscontinuity() reporting discontinuity at: %d", positionToReport));
        return positionToReport;
      }
      return C.TIME_UNSET;
    }

    @Override
    public long seekToUs(long positionUs) {
      // Handle a request to seek to a new position. This usually involves modifying or resetting
      // the contents of the SampleStreams.
      Log.i(TAG, String.format("Seeking to timestamp %dd (not really)", positionUs));
      stream.clearStream();
      reachedEos = false;
      pendingDiscontinuity = true;
      discontinuityPositionUs = positionUs;
      return positionUs;
    }

    @Override
    public long getAdjustedSeekPositionUs(long positionUs, SeekParameters seekParameters) {
      // Option to change seek position requests to another position.
      // Log.i(TAG, "Called getAdjustedSeekPositionUs()");
      Log.i(TAG, String.format("Called getAdjustedSeekPositionUs(), returning %d", positionUs));
      return positionUs;
    }

    @Override
    public long getBufferedPositionUs() {
      // Return how far the media is buffered.
      // Log.i(TAG, "Called getBufferedPositionUs()");
      if (reachedEos) {
        // Log.i(TAG, "Returning end of source for getBufferedPositionUs()");
        return C.TIME_END_OF_SOURCE;
      }
      long pos = C.TIME_UNSET;
      if (stream != null) {
        pos = stream.getBufferedPositionUs();
      } else {
        // Log.i(TAG, "Stream is null in getBufferedPositionUs()");
      }
      pos = pos == C.TIME_UNSET ? 0 : pos;
      // Log.i(TAG, String.format("Returning %d for getBufferedPositionUs()", pos));
      return pos;
    }

    @Override
    public long getNextLoadPositionUs() {
      // Log.i(TAG, "Called getBufferedPositionUs()");
      long pos = getBufferedPositionUs();
      // Log.i(TAG, String.format("Returning %d for getNextLoadPositionUs()", pos));
      return pos;
    }

    @Override
    public boolean continueLoading(LoadingInfo loadingInfo) {
      // Request to continue loading data. Will be called once initially and whenever
      // Callback.onContinueLoadingRequested is triggered from this class (this allows the player's
      // LoadControl to prevent further loading if the buffer is already full)
      // Log.i(TAG, "Called continueLoading()");
      // Log.i(TAG, String.format("LoadingInfo: lastRebufferRealtimeMs - %d, playbackPositionUs - %d, playbackSpeed - %.2f", loadingInfo.lastRebufferRealtimeMs, loadingInfo.playbackPositionUs, loadingInfo.playbackSpeed));
      // if (firstCall) {
      //   Log.i(TAG, "Return true");
      //   firstCall = false;
      //   return true;
      // }
      // Log.i(TAG, "Return false");
      // return false;
      return true;
    }

    @Override
    public boolean isLoading() {
      // Whether media is currently loading.
      // Log.i(TAG, "Called isLoading()");
      // return stream.firstData;
      return true;
    }

    @Override
    public void reevaluateBuffer(long positionUs) {
      // Ignorable, do nothing.
      // Log.i(TAG, "Called reevaluateBuffer()");
    }

    public boolean writeSample(byte[] data, int sizeInBytes, long timestampUs, boolean isKeyFrame, boolean isEndOfStream) {
      // Log.i(TAG, "Called writeSample()");
      try {
        if (isEndOfStream) {
          // Log.i(TAG, "Setting reachedEos to true");
          reachedEos = true;
        }
        // pendingSamples.put(new SampleData(data, sizeInBytes, timestampUs, isKeyFrame, isEndOfStream));
      } catch (Exception e) {
        //
      }
      stream.writeSample(data, sizeInBytes, timestampUs, isKeyFrame, isEndOfStream);
      // this.callback.onContinueLoadingRequested(this);
      if (stream.prerolled) {
        playerBridge.onPrerolled(isAudio);
      }
      return true;
    }
  }
}
