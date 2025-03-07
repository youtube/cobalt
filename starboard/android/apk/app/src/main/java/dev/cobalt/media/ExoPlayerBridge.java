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
import androidx.media3.exoplayer.ExoPlayer;
import dev.cobalt.media.CobaltMediaSource;
import dev.cobalt.util.Log;
import dev.cobalt.util.UsedByNative;
import java.util.ArrayList;

@UsedByNative
@UnstableApi
public class ExoPlayerBridge {
  private static ExoPlayer player;
  private final Context context;

  public ExoPlayerBridge(Context context) {
    this.context = context;
  }
  @UsedByNative
  public void createExoPlayer(Looper looper) {
    player = new ExoPlayer.Builder(context).setLooper(looper).build();
    Log.v(TAG, "Created ExoPlayer");
  }

  @UsedByNative
  public void destroyExoPlayer() {
    player.release();
    Log.v(TAG, "Destroyed ExoPlayer");
  }
};
