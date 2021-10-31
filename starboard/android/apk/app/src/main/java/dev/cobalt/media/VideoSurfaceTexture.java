// Copyright 2019 The Cobalt Authors. All Rights Reserved.
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

import android.graphics.SurfaceTexture;
import dev.cobalt.util.UsedByNative;

// A wrapper of SurfaceTexture class.
// VideoSurfaceTexture allows native code to receive OnFrameAvailable event.
@UsedByNative
public class VideoSurfaceTexture extends SurfaceTexture {
  @UsedByNative
  VideoSurfaceTexture(int texName) {
    super(texName);
  }

  @UsedByNative
  void setOnFrameAvailableListener(final long nativeVideoDecoder) {
    super.setOnFrameAvailableListener(
        new SurfaceTexture.OnFrameAvailableListener() {
          @Override
          public void onFrameAvailable(SurfaceTexture surfaceTexture) {
            nativeOnFrameAvailable(nativeVideoDecoder);
          }
        });
  }

  @UsedByNative
  void removeOnFrameAvailableListener() {
    super.setOnFrameAvailableListener(null);
  }

  private native void nativeOnFrameAvailable(long nativeVideoDecoder);
}
