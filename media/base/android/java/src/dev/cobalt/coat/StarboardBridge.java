// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

package dev.cobalt.coat;

import static dev.cobalt.util.Log.TAG;

import dev.cobalt.media.AudioOutputManager;
import dev.cobalt.media.VideoSurfaceView;
import dev.cobalt.util.DisplayUtil;
import dev.cobalt.util.Log;
import dev.cobalt.util.UsedByNative;

import android.app.Activity;
import android.view.Display;
import android.view.ViewGroup.LayoutParams;
import android.widget.FrameLayout;
import org.chromium.base.ContextUtils;
import org.chromium.base.ApplicationStatus;

// import org.jni_zero.CalledByNative;
import org.chromium.base.annotations.CalledByNative;

public class StarboardBridge {

  private static StarboardBridge gStarboardBridge;
  private AudioOutputManager audioOutputManager;
  private VideoSurfaceView videoSurfaceView;

  @CalledByNative("StarboardBridge")
  public StarboardBridge() {
    assert(gStarboardBridge == null);
    gStarboardBridge = this;

    nativeInitialize();

    Activity activity = ApplicationStatus.getLastTrackedFocusedActivity();

    this.audioOutputManager = new AudioOutputManager(activity);

    activity.runOnUiThread(
    new Runnable() {
      @Override 
      public void run() {
        videoSurfaceView = new VideoSurfaceView(activity);
        activity.addContentView(
          videoSurfaceView, new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT));
      }
    });
    
    try {
      Log.e(TAG, "COBALT: Creating VideoSurfaceView");
      Thread.sleep(5000);
      Log.e(TAG, "COBALT: Created VideoSurfaceView");
    } catch (Exception e) {

    }
  }

  /** Return supported hdr types. */
  @SuppressWarnings("unused")
  @UsedByNative
  public int[] getSupportedHdrTypes() {
    Display defaultDisplay = DisplayUtil.getDefaultDisplay();
    if (defaultDisplay == null) {
      return null;
    }
  
    Display.HdrCapabilities hdrCapabilities = defaultDisplay.getHdrCapabilities();
    if (hdrCapabilities == null) {
      return null;
    }
  
    return hdrCapabilities.getSupportedHdrTypes();
  }

  @SuppressWarnings("unused")
  @UsedByNative
  AudioOutputManager getAudioOutputManager() {
    return audioOutputManager;
  }

  public void setVideoSurfaceBounds(final int x, final int y, final int width, final int height) {
    if (width == 0 || height == 0) {
      // The SurfaceView should be covered by our UI layer in this case.
      return;
    }

    Activity activity = ApplicationStatus.getLastTrackedFocusedActivity();
    activity.runOnUiThread(
        new Runnable() {
          @Override
          public void run() {
            LayoutParams layoutParams = videoSurfaceView.getLayoutParams();
            // Since videoSurfaceView is added directly to the Activity's content view, which is a
            // FrameLayout, we expect its layout params to become FrameLayout.LayoutParams.
            if (layoutParams instanceof FrameLayout.LayoutParams) {
              ((FrameLayout.LayoutParams) layoutParams).setMargins(x, y, x + width, y + height);
            } else {
              Log.w(
                  TAG,
                  "Unexpected video surface layout params class "
                      + layoutParams.getClass().getName());
            }
            layoutParams.width = width;
            layoutParams.height = height;
            // Even though as a NativeActivity we're not using the Android UI framework, by setting
            // the  layout params it will force a layout to be requested. That will cause the
            // SurfaceView to position its underlying Surface to match the screen coordinates of
            // where the view would be in a UI layout and to set the surface transform matrix to
            // match the view's size.
            videoSurfaceView.setLayoutParams(layoutParams);
          }
        });
  }

  private native void nativeInitialize();
}
