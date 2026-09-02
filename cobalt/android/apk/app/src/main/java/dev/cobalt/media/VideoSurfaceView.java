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

package dev.cobalt.media;

import static dev.cobalt.media.Log.TAG;

import android.content.Context;
import android.graphics.Color;
import android.util.AttributeSet;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import androidx.annotation.VisibleForTesting;
import dev.cobalt.coat.BaseStarboardBridge;
import dev.cobalt.util.Log;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

/**
 * A Surface view to be used by the video decoder. It informs the Starboard application when the
 * surface is available so that the decoder can get a reference to it.
 */
@JNINamespace("starboard")
public class VideoSurfaceView extends SurfaceView {
  @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
  @NativeMethods
  public interface Natives {
    void onVideoSurfaceChanged(Surface surface);
  }

  private static Natives sNatives;

  @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
  public static void setNativesForTesting(Natives natives) {
    sNatives = natives;
  }

  private static Natives getNatives() {
    if (sNatives != null) {
      return sNatives;
    }
    return VideoSurfaceViewJni.get();
  }

  public static void notifyVideoSurfaceChanged(Surface surface) {
    getNatives().onVideoSurfaceChanged(surface);
  }

  private final BaseStarboardBridge mBridge;

  public VideoSurfaceView(Context context) {
    super(context);
    mBridge = getStarboardBridge(context);
    initialize(context);
  }

  public VideoSurfaceView(Context context, AttributeSet attrs) {
    super(context, attrs);
    mBridge = getStarboardBridge(context);
    initialize(context);
  }

  public VideoSurfaceView(Context context, AttributeSet attrs, int defStyleAttr) {
    super(context, attrs, defStyleAttr);
    mBridge = getStarboardBridge(context);
    initialize(context);
  }

  public VideoSurfaceView(Context context, AttributeSet attrs, int defStyleAttr, int defStyleRes) {
    super(context, attrs, defStyleAttr, defStyleRes);
    mBridge = getStarboardBridge(context);
    initialize(context);
  }

  private void initialize(Context context) {
    setBackgroundColor(Color.TRANSPARENT);
    getHolder().addCallback(new SurfaceHolderCallback());

    // TODO: Avoid recreating the surface when the player bounds change.
    // Recreating the surface is time-consuming and complicates synchronizing
    // punch-out video when the position / size is animated.
  }

  private static BaseStarboardBridge getStarboardBridge(Context context) {
    Context appContext = context.getApplicationContext();
    if (appContext instanceof BaseStarboardBridge.HostApplication) {
      return ((BaseStarboardBridge.HostApplication) appContext).getStarboardBridge();
    }
    return BaseStarboardBridge.getInstance();
  }

  private BaseStarboardBridge getBridge() {
    if (mBridge != null) {
      return mBridge;
    }
    return getStarboardBridge(getContext());
  }

  private class SurfaceHolderCallback implements SurfaceHolder.Callback {

    boolean mSawInitialChange = false;

    @Override
    public void surfaceCreated(SurfaceHolder holder) {
      BaseStarboardBridge bridge = getBridge();
      if (bridge != null) {
        bridge.onVideoSurfaceCreated(holder.getSurface());
      } else {
        notifyVideoSurfaceChanged(holder.getSurface());
      }
    }

    @Override
    public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
      // We should only ever see the initial change after creation.
      if (mSawInitialChange) {
        Log.e(TAG, "Video surface changed; decoding may break");
      }
      mSawInitialChange = true;
    }

    @Override
    public void surfaceDestroyed(SurfaceHolder holder) {
      BaseStarboardBridge bridge = getBridge();
      if (bridge != null) {
        bridge.onVideoSurfaceDestroyed(holder.getSurface());
      } else {
        notifyVideoSurfaceChanged(null);
      }
    }
  }

  public static Surface getCurrentSurface() {
    BaseStarboardBridge bridge = BaseStarboardBridge.getInstance();
    return bridge != null ? bridge.getVideoSurface() : null;
  }
}
