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

import android.content.Context;
import android.os.Looper;
import android.view.Surface;
import androidx.annotation.Nullable;
import androidx.media3.common.AudioAttributes;
import androidx.media3.common.Format;
import androidx.media3.common.MimeTypes;
import androidx.media3.common.PlaybackException;
import androidx.media3.common.Player;
import androidx.media3.common.Player.State;
import androidx.media3.common.util.UnstableApi;
import androidx.media3.exoplayer.DefaultRenderersFactory;
import androidx.media3.exoplayer.ExoPlaybackException;
import androidx.media3.exoplayer.ExoPlayer;
import androidx.media3.exoplayer.RendererCapabilities;
import androidx.media3.exoplayer.RenderersFactory;
import androidx.media3.exoplayer.audio.MediaCodecAudioRenderer;
import androidx.media3.exoplayer.source.MergingMediaSource;
import dev.cobalt.media.CobaltMediaSource;
import dev.cobalt.util.Log;
import dev.cobalt.util.UsedByNative;
import java.util.ArrayList;

@UsedByNative
@UnstableApi
public class ExoPlayerBridge {
  private ExoPlayer player;
  private final Context context;
  // private CobaltMediaSource mediaSource;
  private CobaltMediaSource audioMediaSource;
  private CobaltMediaSource videoMediaSource;
  private MergingMediaSource mergingMediaSource;
  private long mNativeExoPlayerBridge;
  public ExoPlayerBridge(Context context) {
    this.context = context;
  }
  private native void nativeOnPlaybackStateChanged(long NativeExoPlayerBridge, int playbackState);
  private native void nativeOnInitialized(long nativeExoPlayerBridge);
  private native void nativeOnError(long nativeExoPlayerBridge);
  private native void nativeSetPlayingStatus(long nativeExoPlayerBridge, boolean isPlaying);

  private class ExoPlayerListener implements Player.Listener {

    public void onPlaybackStateChanged(@State int playbackState) {
      switch (playbackState) {
        case Player.STATE_IDLE:
          Log.i(TAG, "ExoPlayer state changed to idle");
          break;
        case Player.STATE_BUFFERING:
          Log.i(TAG, "ExoPlayer state changed to buffering");
          nativeOnPlaybackStateChanged(mNativeExoPlayerBridge, playbackState);
          break;
        case Player.STATE_READY:
          Log.i(TAG, "ExoPlayer state changed to ready");
          nativeOnPlaybackStateChanged(mNativeExoPlayerBridge, playbackState);
          break;
        case Player.STATE_ENDED:
          Log.i(TAG, "ExoPlayer state changed to ended");
          nativeOnPlaybackStateChanged(mNativeExoPlayerBridge, playbackState);
          break;
      }
    }

    public void onIsPlayingChanged(boolean isPlaying) {
      if (isPlaying) {
        // Active playback.
        Log.i(TAG, "Exoplayer is playing.");
        nativeSetPlayingStatus(mNativeExoPlayerBridge, true);
      } else {
        // Not playing because playback is paused, ended, suppressed, or the player
        // is buffering, stopped or failed. Check player.getPlayWhenReady,
        // player.getPlaybackState, player.getPlaybackSuppressionReason and
        // player.getPlaybackError for details.
        Log.i(TAG, "Exoplayer is not playing.");
        nativeSetPlayingStatus(mNativeExoPlayerBridge, true);
      }
    }

    public void onPlayerError(PlaybackException error) {
      @Nullable Throwable cause = error.getCause();
      Log.e(TAG, String.format("ExoPlayer playback error %s, code: %s, cause: %s", error.getMessage(), error.getErrorCodeName(), error.getCause().getMessage()));
      nativeOnError(mNativeExoPlayerBridge);
    }

    public void onPlayWhenReadyChanged(boolean playWhenReady, @Player.PlayWhenReadyChangeReason int reason) {
      Log.i(TAG, String.format("onPlayWhenReadyChanged() with playWhenReady %b and change reason %d", playWhenReady, reason));
    }

    public void onPlaybackSuppressionReasonChanged(@Player.PlaybackSuppressionReason int playbackSuppressionReason
    ) {
      Log.i(TAG, String.format("onPlaybackSuppressionReasonChanged() with playbackSuppressionReason %d", playbackSuppressionReason));
    }
  }

  @SuppressWarnings("unused")
  @UsedByNative
  public void createExoPlayer(long nativeExoPlayerBridge, Surface surface, byte[] audioConfigurationData) {
    Looper.prepare();
    mNativeExoPlayerBridge = nativeExoPlayerBridge;
    player = new ExoPlayer.Builder(context).build();
    player.addListener(new ExoPlayerListener());
    player.setAudioAttributes(AudioAttributes.DEFAULT, /* handleAudioFocus= */ true);

    audioMediaSource = new CobaltMediaSource(true, this, audioConfigurationData);
    videoMediaSource = new CobaltMediaSource(false, this, null);
    mergingMediaSource = new MergingMediaSource(true, audioMediaSource, videoMediaSource);
    player.setVideoSurface(surface);
    player.setMediaSource(mergingMediaSource);
    Log.i(TAG, "Created ExoPlayer");
    player.prepare();
    player.setPlayWhenReady(true);
  }

  @SuppressWarnings("unused")
  @UsedByNative
  public void destroyExoPlayer() {
    player.release();
    Log.i(TAG, "Destroyed ExoPlayer");
    Looper.myLooper().quit();
  }

  @SuppressWarnings("unused")
  @UsedByNative
  private boolean seek(long seekToTimeUs) {
    if (player == null) {
      Log.e(TAG, "Cannot set seek with NULL ExoPlayer.");
      return false;
    }
    // player.seekTo(seekToTimeUs * 1000);
    // Log.i(TAG, String.format("ExoPlayer seeked to %d microseconds.", seekToTimeUs));
    return true;
  }

  @SuppressWarnings("unused")
  @UsedByNative
  private boolean setVolume(float volume) {
    if (player == null) {
      Log.e(TAG, "Cannot set volume with NULL ExoPlayer.");
      return false;
    }
    Log.i(TAG, String.format("Setting ExoPlayer volume to %f", volume));
    player.setVolume(volume);
    return true;
  }

  @SuppressWarnings("unused")
  @UsedByNative
  private boolean play() {
    if (player == null) {
      Log.e(TAG, "Unable to play with NULL ExoPlayer.");
      return false;
    }
    player.setPlayWhenReady(true);
    Log.i(TAG, "ExoPlayer is ready to play.");
    return true;
  }

  @SuppressWarnings("unused")
  @UsedByNative
  private boolean pause() {
    if (player == null) {
      Log.e(TAG, "Unable to pause with NULL ExoPlayer.");
      return false;
    }
    player.pause();
    Log.i(TAG, "ExoPlayer is paused.");
    return true;
  }

  @SuppressWarnings("unused")
  @UsedByNative
  private boolean stop() {
    if (player == null) {
      Log.e(TAG, "Unable to stop with NULL ExoPlayer.");
      return true;
    }
    player.stop();
    Log.i(TAG, "ExoPlayer is stopped.");
    return false;
  }

  @SuppressWarnings("unused")
  @UsedByNative
  public void writeSample(byte[] data, int sizeInBytes, long timestampUs, boolean isAudio, boolean isKeyFrame, boolean isEndOfStream) {
    if (isAudio) {
      audioMediaSource.writeSample(data, sizeInBytes, timestampUs, isKeyFrame, isEndOfStream);
    } else {
      videoMediaSource.writeSample(data, sizeInBytes, timestampUs, isKeyFrame, isEndOfStream);
    }
  }

  @UsedByNative
  @SuppressWarnings("unused")
  private long getCurrentPositionUs() {
    return player.getCurrentPosition() * 1000; // getCurrentPosition returns milliseconds.
  }

  public void onStreamCreated() {
    if (audioMediaSource.isInitialized() && videoMediaSource.isInitialized()) {
      nativeOnInitialized(mNativeExoPlayerBridge);
    }
  }
};
