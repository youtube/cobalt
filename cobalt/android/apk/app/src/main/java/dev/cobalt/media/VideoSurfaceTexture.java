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
import android.view.Surface;
import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

/**
 *  A wrapper of SurfaceTexture class.
 * VideoSurfaceTexture allows native code to receive OnFrameAvailable event.
 */
@JNINamespace("starboard")
public class VideoSurfaceTexture extends SurfaceTexture {
  @CalledByNative
  VideoSurfaceTexture(int texName) {
    super(texName);
  }

  @CalledByNative
  void setOnFrameAvailableListener(final long nativeVideoSurfaceTextureBridge) {
    super.setOnFrameAvailableListener(
        new SurfaceTexture.OnFrameAvailableListener() {
          @Override
          public void onFrameAvailable(SurfaceTexture surfaceTexture) {
            VideoSurfaceTextureJni.get().onFrameAvailable(nativeVideoSurfaceTextureBridge);
          }
        });
  }

  @CalledByNative
  void removeOnFrameAvailableListener() {
    super.setOnFrameAvailableListener(null);
  }

  @CalledByNative
  static Surface createSurface(VideoSurfaceTexture surfaceTexture) {
    return new Surface(surfaceTexture);
  }

  @CalledByNative
  public void updateTexImage() {
    super.updateTexImage();
  }

  @CalledByNative
  public void getTransformMatrix(float[] mtx) {
    super.getTransformMatrix(mtx);
  }

  @NativeMethods
  interface Natives {
    void onFrameAvailable(long nativeVideoSurfaceTextureBridge);
  }
}
