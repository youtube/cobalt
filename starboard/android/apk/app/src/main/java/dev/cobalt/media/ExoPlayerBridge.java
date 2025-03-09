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
import androidx.media3.common.AudioAttributes;
import androidx.media3.common.Format;
import androidx.media3.common.MimeTypes;
import androidx.media3.common.Player;
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
  public ExoPlayerBridge(Context context) {
    this.context = context;
  }

  @SuppressWarnings("unused")
  @UsedByNative
  public void createExoPlayer(Surface surface) {
    Looper.prepare();
    // RenderersFactory factory = new DefaultRenderersFactory(context);
    player = new ExoPlayer.Builder(context).build();
    player.setAudioAttributes(AudioAttributes.DEFAULT, /* handleAudioFocus= */ true);
    Log.i(TAG, String.format("Player renderer count 1 is %d", player.getRendererCount()));
    Format audioFormat = new Format.Builder()
        .setSampleMimeType(MimeTypes.AUDIO_OPUS)
        .setSampleRate(48000)
        .setChannelCount(2)
        //.setPcmEncoding(C.ENCODING_PCM_FLOAT)
        .setLanguage("en-GB")
        .build();
    Format videoFormat = new Format.Builder()
        .setSampleMimeType(MimeTypes.VIDEO_VP9)
        .setAverageBitrate(100_000)
        .setWidth(1920)
        .setHeight(1080)
        .setFrameRate(30f)
        .setLanguage("en-GB")
        .build();
    for (int i = 0; i < player.getRendererCount(); ++i) {
      Log.i(TAG, String.format("Name of renderer %d is %s", i, player.getRenderer(i).getName()));
      try {
        int formatSupport = player.getRenderer(i).getCapabilities().supportsFormat(audioFormat);
        Log.i(TAG, String.format("Support for AAC: %d",
            RendererCapabilities.getFormatSupport(formatSupport)));
        formatSupport = player.getRenderer(i).getCapabilities().supportsFormat(videoFormat);
        Log.i(TAG, String.format("Support for VP9: %d",
            RendererCapabilities.getFormatSupport(formatSupport)));
      } catch (ExoPlaybackException e) {
        Log.i(TAG, String.format("Exception %s", e.toString()));
        continue;
      }
    }
    audioMediaSource = new CobaltMediaSource(true);
    videoMediaSource = new CobaltMediaSource(false);
    mergingMediaSource = new MergingMediaSource(true, audioMediaSource, videoMediaSource);
    player.setVideoSurface(surface);
    player.setMediaSource(mergingMediaSource);
    Log.i(TAG, "Created ExoPlayer");
    player.setPlayWhenReady(false);
    player.prepare();
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
    player.seekTo(seekToTimeUs * 1000);
    Log.i(TAG, String.format("ExoPlayer seeked to %d microseconds.", seekToTimeUs));
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
    player.play();
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
  public void writeSample(byte[] data, int sizeInBytes, long timestampUs, boolean isAudio) {
    if (isAudio) {
      audioMediaSource.writeSample(data, sizeInBytes, timestampUs);
    } else {
      videoMediaSource.writeSample(data, sizeInBytes, timestampUs);
    }
  }
};
