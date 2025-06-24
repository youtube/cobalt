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
import android.os.Handler;
import android.os.HandlerThread;
import android.os.Looper;
import android.os.Process;
import android.view.Surface;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.media3.common.AudioAttributes;
import androidx.media3.common.Format;
import androidx.media3.common.MimeTypes;
import androidx.media3.common.PlaybackException;
import androidx.media3.common.PlaybackParameters;
import androidx.media3.common.Player;
import androidx.media3.common.Player.State;
import androidx.media3.common.util.UnstableApi;
import androidx.media3.exoplayer.analytics.AnalyticsListener;
import androidx.media3.exoplayer.DefaultLoadControl;
import androidx.media3.exoplayer.DefaultRenderersFactory;
import androidx.media3.exoplayer.trackselection.DefaultTrackSelector;
import androidx.media3.exoplayer.ExoPlaybackException;
import androidx.media3.exoplayer.ExoPlayer;
import androidx.media3.exoplayer.LoadControl;
import androidx.media3.exoplayer.RendererCapabilities;
import androidx.media3.exoplayer.RenderersFactory;
import androidx.media3.exoplayer.audio.MediaCodecAudioRenderer;
import androidx.media3.exoplayer.source.MediaSource;
import androidx.media3.exoplayer.source.MergingMediaSource;
import androidx.media3.exoplayer.util.EventLogger;
import dev.cobalt.media.CobaltMediaSource;
import dev.cobalt.media.ExoPlayerFormatCreator;
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
  private MediaSource playbackMediaSource;
  private long mNativeExoPlayerBridge;
  private boolean isAudioOnly;
  private boolean isVideoOnly;
  private LoadControl loadControl;
  private boolean audioPrerolled = false;
  private boolean videoPrerolled = false;
  private boolean firedEOS = false;
  public ExoPlayerBridge(Context context) {
    this.context = context;
  }
  private native void nativeOnPlaybackStateChanged(long NativeExoPlayerBridge, int playbackState);
  private native void nativeOnInitialized(long nativeExoPlayerBridge);
  private native void nativeOnError(long nativeExoPlayerBridge);
  private native void nativeSetPlayingStatus(long nativeExoPlayerBridge, boolean isPlaying);
  private HandlerThread exoplayerThread;
  private Handler exoplayerHandler;
  private long lastPlaybackPos = 0;
  private volatile boolean stopped = false;
  private boolean notifiedPrerolled = false;
  private volatile boolean destroying = false;
  private int droppedFramesOnPlayerThread = 0;

  private class ExoPlayerListener implements Player.Listener {

    public void onPlayerRelease() {
      Log.i(TAG, "ExoPlayer has completed release, quitting handler thread.");
    }
    public void onPlaybackStateChanged(@State int playbackState) {
      Log.i(TAG, String.format("Called onPlaybackStateChanged() with %d", playbackState));
      switch (playbackState) {
        case Player.STATE_IDLE:
          Log.i(TAG, "ExoPlayer state changed to idle");
          break;
        case Player.STATE_BUFFERING:
          Log.i(TAG, "ExoPlayer state changed to buffering");
          if (!stopped) {
            nativeOnPlaybackStateChanged(mNativeExoPlayerBridge, playbackState);
          }
          break;
        case Player.STATE_READY:
          Log.i(TAG, "ExoPlayer state changed to ready");
          if (player.isTunnelingEnabled()) {
            Log.i(TAG, "TUNNEL MODE ENABLED");
          } else {
            Log.i(TAG, "TUNNEL MODE DISABLED");
          }
          if (!stopped) {
            // nativeOnPlaybackStateChanged(mNativeExoPlayerBridge, playbackState);
          }
          break;
        case Player.STATE_ENDED:
          Log.i(TAG, "ExoPlayer state changed to ended");
          if (!stopped && !firedEOS) {
            firedEOS = true;
            nativeOnPlaybackStateChanged(mNativeExoPlayerBridge, playbackState);
          }
          break;
      }
    }

    public void onIsPlayingChanged(boolean isPlaying) {
      if (isPlaying) {
        // Active playback.
        Log.i(TAG, "Exoplayer is playing.");
        if (!stopped) {
          nativeSetPlayingStatus(mNativeExoPlayerBridge, true);
        }
      } else {
        // Not playing because playback is paused, ended, suppressed, or the player
        // is buffering, stopped or failed. Check player.getPlayWhenReady,
        // player.getPlaybackState, player.getPlaybackSuppressionReason and
        // player.getPlaybackError for details.
        Log.i(TAG, "Exoplayer is not playing.");
        nativeSetPlayingStatus(mNativeExoPlayerBridge, false);
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

  private final class ExoPlayerEventLogger extends EventLogger {

    @Override
    public void onDroppedVideoFrames(
        AnalyticsListener.EventTime eventTime,
        int droppedFrames,
        long elapsedMs
    ) {
      Log.i(TAG, String.format("REPORTING %d DROPPED VIDEO FRAMES", droppedFrames));
      droppedFramesOnPlayerThread += droppedFrames;
    }

  }

  @SuppressWarnings("unused")
  @UsedByNative
  public void createExoPlayer(long nativeExoPlayerBridge, Surface surface, byte[] audioConfigurationData, boolean isAudioOnly, boolean isVideoOnly, int sampleRate, int channelCount, int width, int height, int fps, int videoBitrate, String audioMime, String videoMime) {
    if (surface == null) {
      Log.i(TAG, "AUSTIN: SURFACE IS NULL");
    }
    if (!surface.isValid()) {
      Log.i(TAG, "AUSTIN: SURFACE IS INVALID");
    }
    this.exoplayerThread = new HandlerThread("ExoPlayerThread", Process.THREAD_PRIORITY_AUDIO);
    exoplayerThread.start();
    this.exoplayerHandler = new Handler(exoplayerThread.getLooper());
    // Looper.prepare();
    this.isAudioOnly = isAudioOnly;
    mNativeExoPlayerBridge = nativeExoPlayerBridge;
    // loadControl = new DefaultLoadControl.Builder().setBufferDurationsMs(0, 4000, 0, 0).build();

    boolean preferTunnelMode = false;
    if (preferTunnelMode) {
      Log.i(TAG, "Tunnel mode is preferred for this playback.");
    }
    DefaultTrackSelector trackSelector = new DefaultTrackSelector(context);
    trackSelector.setParameters(
        new DefaultTrackSelector.ParametersBuilder(context).setTunnelingEnabled(preferTunnelMode).build());

    player = new ExoPlayer.Builder(context).setLoadControl(createLoadControl()).setLooper(exoplayerThread.getLooper()).setTrackSelector(trackSelector).setReleaseTimeoutMs(1000).build();
    // player = new ExoPlayer.Builder(context).build();
    exoplayerHandler.post(() -> {
      player.addListener(new ExoPlayerListener());
      // player.setAudioAttributes(AudioAttributes.DEFAULT, /* handleAudioFocus= */ true);

      if (!isAudioOnly) {
        Log.i(TAG, "Setting video surface");
        player.setVideoSurface(surface);
      }

      if (isAudioOnly) {
        Log.i(TAG, "Creating audio only exoplayer");
        audioMediaSource = new CobaltMediaSource(true, this, audioConfigurationData, ExoPlayerFormatCreator.createAudioFormat(audioMime, audioConfigurationData, sampleRate, channelCount));
        playbackMediaSource = audioMediaSource;
        // player.setMediaSource(audioMediaSource);
      } else if (isVideoOnly) {
        Log.i(TAG, "Creating video only exoplayer");
        videoMediaSource = new CobaltMediaSource(false, this, null, ExoPlayerFormatCreator.createVideoFormat(videoMime, width, height, (float) fps, videoBitrate));
        playbackMediaSource = videoMediaSource;
        this.isVideoOnly = true;
      } else {
        audioMediaSource = new CobaltMediaSource(true, this, audioConfigurationData, ExoPlayerFormatCreator.createAudioFormat(audioMime, audioConfigurationData, sampleRate, channelCount));
        videoMediaSource = new CobaltMediaSource(false, this, null, ExoPlayerFormatCreator.createVideoFormat(videoMime, width, height, (float) fps, videoBitrate));
        mergingMediaSource = new MergingMediaSource(true, audioMediaSource, videoMediaSource);
        playbackMediaSource = mergingMediaSource;
        // player.setMediaSource(mergingMediaSource);
      }
      player.setMediaSource(playbackMediaSource);
      player.addAnalyticsListener(new ExoPlayerEventLogger());
      Log.i(TAG, "Created ExoPlayer");
      // player.setPlayWhenReady(true);
      player.prepare();
      player.play();
    });
  }

  private synchronized void updatePlaybackPos() {
    if (!isAbleToProcessCommands()) {
      Log.e(TAG, "Unable update timestamps with NULL ExoPlayer.");
      return;
    }
    lastPlaybackPos = player.getCurrentPosition() * 1000;
    // Log.i(TAG, String.format("Updated playback pos to %d", lastPlaybackPos));
    if (!stopped) {
      exoplayerHandler.postDelayed(this::updatePlaybackPos, 75);
    } else {
      Log.i(TAG, "Stopped, not posting delayed updateplaybackpos");
    }
  }

  @SuppressWarnings("unused")
  @UsedByNative
  public void destroyExoPlayer() throws InterruptedException {
    if (!isAbleToProcessCommands()) {
      Log.e(TAG, "Unable to destroy player with NULL ExoPlayer.");
      return;
    }
    Log.i(TAG, "Removing callbacks");
    exoplayerHandler.removeCallbacksAndMessages(null);
    exoplayerHandler.removeCallbacks(this::updatePlaybackPos);
    destroying = true;
    exoplayerHandler.post(() -> {
      // if (!stopped) {
        stopped = true;
        // Log.i(TAG, "Signalling player stop");
        // player.stop();
      // }
      Log.i(TAG, "Releasing player");
      if (player != null) {
        player.release();
        player = null;
      } else {
        Log.i(TAG, "Player is already null");
      }
      // Log.i(TAG, "quitting thread safely");
      // if (exoplayerThread.getLooper() != null) {
      //   exoplayerThread.getLooper().quitSafely();
      // } else {
      //   Log.i(TAG, "Looper is already null");
      // }
      destroying = false;
      stopped = false;
      notifiedPrerolled = false;
      exoplayerHandler.getLooper().quitSafely();
      Log.i(TAG, "Destroyed ExoPlayer");
    });
    exoplayerThread.join(10000);
    if (exoplayerThread.isAlive()) {
      Log.i(TAG, "ExoPlayer thread did not join after 10 seconds.");
    }
    exoplayerHandler = null;
    exoplayerThread = null;
    Log.i(TAG, "Destroyed ExoPlayer thread");
  }

  @SuppressWarnings("unused")
  @UsedByNative
  private boolean seek(long seekToTimeUs) {
    if (!isAbleToProcessCommands()) {
      Log.e(TAG, "Cannot set seek with NULL ExoPlayer.");
      return false;
    }
    exoplayerHandler.post(() -> {
      player.seekTo(seekToTimeUs / 1000);
      Log.i(TAG, String.format("ExoPlayer seeked to %d microseconds.", seekToTimeUs));
      droppedFramesOnPlayerThread = 0;
    });
    return true;
  }

  @SuppressWarnings("unused")
  @UsedByNative
  private boolean setVolume(float volume) {
    if (!isAbleToProcessCommands()) {
      Log.e(TAG, "Cannot set volume with NULL ExoPlayer.");
      return false;
    }
    exoplayerHandler.post(() -> {
      Log.i(TAG, String.format("Setting ExoPlayer volume to %f", volume));
      player.setVolume(volume);
    });
    return true;
  }

  @UsedByNative
  private void setPlaybackRate(float playbackRate) {
    if (!isAbleToProcessCommands()) {
      Log.e(TAG, "Cannot set playback rate with NULL ExoPlayer.");
      return;
    }
    if (playbackRate > 0.0f) {
      exoplayerHandler.post(() -> {
        Log.i(TAG, String.format("Setting ExoPlayer playback rate to %f", playbackRate));
        player.setPlaybackParameters(new PlaybackParameters(playbackRate));
      });
    }
  }

  @SuppressWarnings("unused")
  @UsedByNative
  private boolean play() {
    Log.i(TAG, "Called ExoPlayerBridge.play()");
    if (!isAbleToProcessCommands()) {
      Log.e(TAG, "Unable to play with NULL ExoPlayer.");
      return false;
    }
    exoplayerHandler.post(() -> {
      if (!player.isPlaying()) {
        player.play();
      }
      Log.i(TAG, "Called play() on Exoplayer.");
    });
    return true;
  }

  @SuppressWarnings("unused")
  @UsedByNative
  private boolean pause() {
    if (!isAbleToProcessCommands()) {
      Log.e(TAG, "Unable to pause with NULL ExoPlayer.");
      return false;
    }
    exoplayerHandler.post(() -> {
      player.pause();
      Log.i(TAG, "ExoPlayer is paused.");
    });
    return true;
  }

  @SuppressWarnings("unused")
  @UsedByNative
  private boolean stop() {
    Log.i(TAG, "Called player stop");
    if (!isAbleToProcessCommands()) {
      Log.e(TAG, "Unable to stop with NULL ExoPlayer.");
      return true;
    }
    if (!stopped) {
      exoplayerHandler.removeCallbacks(this::updatePlaybackPos);
      exoplayerHandler.post(() -> {
        player.stop();
        stopped = true;
        Log.i(TAG, "ExoPlayer is stopped.");
      });
    }
    return true;
  }

  @SuppressWarnings("unused")
  @UsedByNative
  public void writeSample(byte[] data, int sizeInBytes, long timestampUs, boolean isAudio, boolean isKeyFrame, boolean isEndOfStream) {
    if (!isAbleToProcessCommands()) {
      Log.e(TAG, "Unable to write samples with a NULL ExoPlayer.");
      return;
    }
    if (!stopped) {
      // Log.i(TAG, String.format("Writing %s sample with timestamp %d", (isAudio ? "audio" : "video"), timestampUs));
      if (isAudio) {
        audioPrerolled |= audioMediaSource.writeSample(data, sizeInBytes, timestampUs, isKeyFrame,
            isEndOfStream);
      } else {
        videoPrerolled |= videoMediaSource.writeSample(data, sizeInBytes, timestampUs, isKeyFrame,
            isEndOfStream);
      }
      // exoplayerHandler.postAtFrontOfQueue(() -> {
      //   PlaybackParameters params = player.getPlaybackParameters();
      //   Log.i(TAG, String.format("Checking if player is playing: %b, speed: %f", player.isPlaying(),
      //       params.speed));
      // });
      // if (notifiedPrerolled) {
      //   Log.i(TAG, String.format("Playback pos: %d", getCurrentPositionUs()));
      // }
    }
  }

  @UsedByNative
  @SuppressWarnings("unused")
  private synchronized long getCurrentPositionUs() {
    return lastPlaybackPos; // getCurrentPosition returns milliseconds.
  }

  @UsedByNative
  private synchronized int getDroppedFrames() {
    return droppedFramesOnPlayerThread;
  }

  public void onStreamCreated() {
    if (!isAbleToProcessCommands()) {
      Log.i(TAG, "Destroying player, cannot create new stream");
    }
    if ((isAudioOnly && audioMediaSource.isInitialized()) || (isVideoOnly && videoMediaSource.isInitialized()) || (audioMediaSource.isInitialized() && videoMediaSource.isInitialized())) {
      nativeOnInitialized(mNativeExoPlayerBridge);
    }
  }

  private LoadControl createLoadControl() {
    return new DefaultLoadControl.Builder().setBufferDurationsMs(500, 50000, 500, 500)
        // .setBackBuffer(0, false) // No back buffer
        .build();
  }

  public void onPrerolled(boolean isAudio) {
    if (!isAbleToProcessCommands()) {
      Log.i(TAG, "Destroying player, cannot preroll");
    }
    if (isAudio) {
      audioPrerolled = true;
    } else {
      videoPrerolled = true;
    }
    if (((isAudioOnly && audioPrerolled) || (isVideoOnly && videoPrerolled) || (audioPrerolled && videoPrerolled)) && !notifiedPrerolled) {
      notifiedPrerolled = true;
      Log.i(TAG, String.format("prerolled. audio pre: %b, video pre: %b, notified: %b, audio only: %b, video only: %b", audioPrerolled, videoPrerolled, notifiedPrerolled, isAudioOnly, isVideoOnly));
      Log.i(TAG, "Media has prerolled, should be ready to play");
      nativeOnPlaybackStateChanged(mNativeExoPlayerBridge, Player.STATE_READY);
      exoplayerHandler.post(this::updatePlaybackPos);
    } else {
      // Log.i(TAG, String.format("Didn't preroll. audio pre: %b, video pre: %b, notified: %b", audioPrerolled, videoPrerolled, notifiedPrerolled));
    }
  }

  private boolean isAbleToProcessCommands() {
    return !destroying && (player != null) && (exoplayerThread != null) && (exoplayerHandler != null);
  }

  public void notifyEOS() {
    // Called from the MediaSource after it's released an EOS frame.
    if (!firedEOS) {
      nativeOnPlaybackStateChanged(mNativeExoPlayerBridge, Player.STATE_ENDED);
      firedEOS = true;
    }
  }
};
