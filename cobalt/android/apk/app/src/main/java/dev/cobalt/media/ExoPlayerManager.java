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

import android.content.Context;
import dev.cobalt.media.ExoPlayerBridge;
import java.util.ArrayList;
import java.util.List;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

/* Creates and destroys ExoPlayer instances */
@JNINamespace("starboard::android::shared")
public class ExoPlayerManager {

  private List<ExoPlayerBridge> exoPlayerBridgeList;
  private Context context;

  public ExoPlayerManager(Context context) {
    this.context = context;
    exoPlayerBridgeList = new ArrayList<ExoPlayerBridge>();
  }

  @CalledByNative
  ExoPlayerBridge createExoPlayerBridge(long nativeExoPlayerBridge, boolean preferTunnelMode) {
    ExoPlayerBridge exoPlayerBridge = new ExoPlayerBridge(nativeExoPlayerBridge, context, preferTunnelMode);
    // TODO: Check for validity here.
    exoPlayerBridgeList.add(exoPlayerBridge);
    return exoPlayerBridge;
  }

  @CalledByNative
  void destroyExoPlayerBridge(ExoPlayerBridge exoPlayerBridge) {
    exoPlayerBridge.destroy();
    exoPlayerBridgeList.remove(exoPlayerBridge);
  }
}
