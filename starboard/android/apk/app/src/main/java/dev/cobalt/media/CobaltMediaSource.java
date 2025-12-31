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
import androidx.annotation.Nullable;
import androidx.media3.common.C;
import androidx.media3.common.Format;
import androidx.media3.common.MediaItem;
import androidx.media3.common.MimeTypes;
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
import java.io.IOException;
import dev.cobalt.util.UsedByNative;
import dev.cobalt.media.SampleQueueManager;
import java.time.chrono.MinguoEra;
import java.util.Arrays;
import java.util.List;

@UnstableApi
public final class CobaltMediaSource extends BaseMediaSource {
  private final SampleQueueManager mManager = new SampleQueueManager();
  // TODO: Dynamically select format.
  private Format audioFormat;
  private Format videoFormat;
  private CustomMediaPeriod mediaPeriod;
  private boolean isAudio;
  // Reference to the host ExoPlayerBridge
  private ExoPlayerBridge playerBridge;
  CobaltMediaSource(boolean isAudio, ExoPlayerBridge playerBridge, byte[] audioConfigurationData) {
    this.playerBridge = playerBridge;
    if (isAudio && audioConfigurationData != null) {
      Log.i(TAG, "Setting Opus config info in ExoPlayer");
      byte[][] csds = {};
      csds = MediaFormatBuilder.starboardParseOpusConfigurationData(48000, audioConfigurationData);
      if (csds == null) {
        Log.e(TAG, "Error parsing Opus config info");
        assert(false);
      }
      List<byte[]> csdsList = Arrays.asList(csds);
      audioFormat = new Format.Builder()
          .setSampleMimeType(MimeTypes.AUDIO_OPUS)
          .setSampleRate(48000)
          .setChannelCount(2)
          .setCodecs("opus")
          .setPcmEncoding(C.ENCODING_PCM_FLOAT)
          .setInitializationData(csdsList)
          .build();
    } else {
      audioFormat = new Format.Builder()
          .setSampleMimeType(MimeTypes.AUDIO_OPUS)
          .setSampleRate(48000)
          .setChannelCount(2)
          .setCodecs("opus")
          .setPcmEncoding(C.ENCODING_PCM_FLOAT)
          .build();
    }
    videoFormat = new Format.Builder()
        .setSampleMimeType(MimeTypes.VIDEO_VP9)
        .setAverageBitrate(100_000)
        .setWidth(1920)
        .setHeight(1080)
        .setFrameRate(30f)
        .build();
    this.isAudio = isAudio;
  }

  @Override
  protected void prepareSourceInternal(@Nullable TransferListener mediaTransferListener) {
    // Start loading generic information about the media (think: manifests/metadata).
    // Once ready, call this method (and populate any data you care about):
    refreshSourceInfo(
        new SinglePeriodTimeline(
            /* durationUs= */ C.TIME_UNSET,
            /* isSeekable= */ true,
            /* isDynamic= */ false,
            /* useLiveConfiguration= */ false,
            /* manifest= */ null,
            getMediaItem()));
  }

  @Override
  protected void releaseSourceInternal() {
    // Release any pending resources you hold from the prepareSourceInternal step above.
  }

  @Override
  public MediaItem getMediaItem() {
    // If needed, return a value that can be obtained via Player.getCurrentMediaItem();
    Log.i(TAG, "Called getMediaItem()");
    return MediaItem.EMPTY;
  }

  @Override
  public void maybeThrowSourceInfoRefreshError() throws IOException {
    // Throw any pending exceptions that prevents the class from creating a MediaPeriod (if any).
  }

  @Override
  public MediaPeriod createPeriod(MediaPeriodId id, Allocator allocator, long startPositionUs) {
    Log.i(TAG, "Called createPeriod");
    Format format = isAudio ? audioFormat : videoFormat;
    mediaPeriod = new CustomMediaPeriod(format, allocator, playerBridge);
    return mediaPeriod;
  }

  @Override
  public void releasePeriod(MediaPeriod mediaPeriod) {
    // Release any resources related to that MediaPeriod, its media loading, etc.
    // TODO: Clear stream here
  }

  public void writeSample(byte[] data, int sizeInBytes, long timestampUs, boolean isKeyFrame, boolean isEndOfStream) {
    mediaPeriod.writeSample(data, sizeInBytes, timestampUs, isKeyFrame, isEndOfStream);
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

    CustomMediaPeriod(Format format, Allocator allocator, ExoPlayerBridge playerBridge) {
      this.format = format;
      this.allocator = allocator;
      this.playerBridge = playerBridge;
    }
    @Override
    public void prepare(Callback callback, long positionUs) {
      Log.i(TAG, "Called CobaltMediaSource.CustomMediaPeriod.prepare()");
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
      return new TrackGroupArray(
          // new TrackGroup(
          //     new Format.Builder()
          //         .setSampleMimeType(MimeTypes.VIDEO_VP9)
          //         .setAverageBitrate(100_000)
          //         .setWidth(1024)
          //         .setHeight(768)
          //         .setFrameRate(30f)
          //         .build(),
          //     new Format.Builder()
          //         .setSampleMimeType(MimeTypes.VIDEO_VP9)
          //         .setAverageBitrate(200_000)
          //         .setWidth(2048)
          //         .setHeight(1440)
          //         .setFrameRate(30f)
          //         .build()),
          // new TrackGroup(
          //     new Format.Builder()
          //         .setSampleMimeType(MimeTypes.AUDIO_OPUS)
          //         .setSampleRate(48000)
          //         .setChannelCount(2)
          //         .setLanguage("en-GB")
          //         .build()),
          // new TrackGroup(
          //     new Format.Builder()
          //         .setSampleMimeType(MimeTypes.TEXT_VTT)
          //         .setLanguage("it")
          //         .build()),
          new TrackGroup(format));
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
    }

    @Override
    public long readDiscontinuity() {
      // Ignorable, just return this value.
      return C.TIME_UNSET;
    }

    @Override
    public long seekToUs(long positionUs) {
      // Handle a request to seek to a new position. This usually involves modifying or resetting
      // the contents of the SampleStreams.
      Log.i(TAG, String.format("Seeking to timestamp %dd (not really)", positionUs));
      stream.clearStream();
      return positionUs;
    }

    @Override
    public long getAdjustedSeekPositionUs(long positionUs, SeekParameters seekParameters) {
      // Option to change seek position requests to another position.
      return positionUs;
    }

    @Override
    public long getBufferedPositionUs() {
      // Return how far the media is buffered.
      if (stream != null) {
        return stream.getBufferedPositionUs();
      }
      return 0;
    }

    @Override
    public long getNextLoadPositionUs() {
      return getBufferedPositionUs();
    }

    @Override
    public boolean continueLoading(LoadingInfo loadingInfo) {
      // Request to continue loading data. Will be called once initially and whenever
      // Callback.onContinueLoadingRequested is triggered from this class (this allows the player's
      // LoadControl to prevent further loading if the buffer is already full)
      return true;
    }

    @Override
    public boolean isLoading() {
      // Whether media is currently loading.
      return true;
    }

    @Override
    public void reevaluateBuffer(long positionUs) {
      // Ignorable, do nothing.
    }

    public void writeSample(byte[] data, int sizeInBytes, long timestampUs, boolean isKeyFrame, boolean isEndOfStream) {
      stream.writeSample(data, sizeInBytes, timestampUs, isKeyFrame, isEndOfStream);
    }
  }
}
