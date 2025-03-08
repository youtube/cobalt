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
import androidx.media3.common.util.UnstableApi;
import androidx.media3.exoplayer.DefaultRenderersFactory;
import androidx.media3.exoplayer.ExoPlayer;
import androidx.media3.exoplayer.RenderersFactory;
import androidx.media3.exoplayer.audio.MediaCodecAudioRenderer;
import dev.cobalt.media.CobaltMediaSource;
import dev.cobalt.util.Log;
import dev.cobalt.util.UsedByNative;
import java.util.ArrayList;

@UsedByNative
@UnstableApi
public class ExoPlayerBridge {
  private ExoPlayer player;
  private final Context context;
  private CobaltMediaSource mediaSource;
  public ExoPlayerBridge(Context context) {
    this.context = context;
  }

  @SuppressWarnings("unused")
  @UsedByNative
  public void createExoPlayer() {
    Looper.prepare();
    // RenderersFactory factory = new DefaultRenderersFactory(context);
    player = new ExoPlayer.Builder(context).build();
    Log.i(TAG, String.format("Player renderer count 1 is %d", player.getRendererCount()));
    for (int i = 0; i < player.getRendererCount(); ++i) {
      Log.i(TAG, String.format("Name of renderer %d is %s", i, player.getRenderer(i).getName()));
    }
    mediaSource = new CobaltMediaSource();
    player.setMediaSource(mediaSource);
    Log.i(TAG, "Created ExoPlayer");
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
};
