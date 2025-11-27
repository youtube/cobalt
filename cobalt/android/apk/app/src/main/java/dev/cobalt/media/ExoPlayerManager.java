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

import static dev.cobalt.util.Log.TAG;

import android.content.Context;
import android.view.Surface;
import dev.cobalt.util.Log;
import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

/** Creates and destroys ExoPlayer instances */
@JNINamespace("starboard")
public class ExoPlayerManager {
    private Context context;

    public ExoPlayerManager(Context context) {
        this.context = context;
    }

    @CalledByNative
    public synchronized ExoPlayerBridge createExoPlayerBridge(long nativeExoPlayerBridge,
            ExoPlayerMediaSource audioSource, ExoPlayerMediaSource videoSource, Surface surface,
            boolean enableTunnelMode) {
        return ExoPlayerBridge.createExoPlayerBridge(nativeExoPlayerBridge, context, audioSource, videoSource,
                surface, enableTunnelMode);
    }

    @CalledByNative
    public synchronized void destroyExoPlayerBridge(ExoPlayerBridge exoPlayerBridge) {
        if (exoPlayerBridge == null) {
            Log.e(TAG, "ExoPlayerManager cannot destroy NULL ExoPlayerBridge.");
            return;
        }
        exoPlayerBridge.destroy();
    }
}
