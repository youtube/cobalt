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
import android.os.Build;
import android.util.AttributeSet;
import android.view.Surface;
import android.view.SurfaceControl;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import androidx.annotation.NonNull;
import androidx.annotation.RequiresApi;
import androidx.annotation.VisibleForTesting;
import dev.cobalt.coat.BaseStarboardBridge;
import dev.cobalt.util.Log;
import java.lang.ref.WeakReference;
import org.jni_zero.CalledByNative;
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

  public static void notifyVideoSurfaceChanged(Surface surface) {
    VideoSurfaceViewJni.get().onVideoSurfaceChanged(surface);
  }

  @NonNull private final BaseStarboardBridge mBridge;
  private static volatile WeakReference<VideoSurfaceView> sInstance;

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
    sInstance = new WeakReference<>(this);
    setBackgroundColor(Color.TRANSPARENT);
    getHolder().addCallback(new SurfaceHolderCallback());

    // TODO: Avoid recreating the surface when the player bounds change.
    // Recreating the surface is time-consuming and complicates synchronizing
    // punch-out video when the position / size is animated.
  }

  @NonNull
  private static BaseStarboardBridge getStarboardBridge(Context context) {
    Context appContext = context.getApplicationContext();
    if (appContext instanceof BaseStarboardBridge.HostApplication) {
      BaseStarboardBridge bridge =
          ((BaseStarboardBridge.HostApplication) appContext).getStarboardBridge();
      if (bridge != null) {
        return bridge;
      }
    }
    return BaseStarboardBridge.getInstance();
  }

  private class SurfaceHolderCallback implements SurfaceHolder.Callback {

    boolean mSawInitialChange = false;

    @Override
    public void surfaceCreated(SurfaceHolder holder) {
      MediaCodecCache.allowCaching();
      mBridge.onVideoSurfaceCreated(holder.getSurface());
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
      // Must be called before onVideoSurfaceDestroyed(): the decoder teardown it triggers then
      // releases the codec instead of caching it with a surface that is about to be destroyed.
      MediaCodecCache.stopCaching();
      mBridge.onVideoSurfaceDestroyed(holder.getSurface());
    }
  }

  public static Surface getCurrentSurface() {
    BaseStarboardBridge bridge = BaseStarboardBridge.getInstance();
    return bridge != null ? bridge.getVideoSurface() : null;
  }

  @RequiresApi(Build.VERSION_CODES.Q)
  private static class Api29Helper {
    /**
     * Sets the visibility of the video SurfaceControl {@code sc} in {@code transaction}.
     *
     * <p>Both the visibility and the alpha are set. SurfaceView keeps managing its SurfaceControl
     * and shows it again on its own updates, e.g. when the player bounds change, which undoes
     * setVisibility(false). However, it leaves the alpha alone unless it is z-ordered above its
     * window, so the alpha is what keeps the surface hidden until it is shown again. Note that
     * SurfaceView.getSurfaceControl() only documents reparenting as a supported usage.
     */
    static SurfaceControl.Transaction setVisibility(
        SurfaceControl.Transaction transaction, SurfaceControl sc, boolean visible) {
      return transaction.setVisibility(sc, visible).setAlpha(sc, visible ? 1.0f : 0.0f);
    }

    static void applyVisibility(SurfaceControl sc, boolean visible) {
      try (SurfaceControl.Transaction transaction = new SurfaceControl.Transaction()) {
        setVisibility(transaction, sc, visible).apply();
      }
    }
  }

  /**
   * Sets visibility of the underlying video SurfaceControl via SurfaceFlinger transaction. Only
   * supported on Android 10 (API 29) and above.
   */
  @CalledByNative
  public static void setVideoSurfaceVisible(boolean visible) {
    if (Build.VERSION.SDK_INT < Build.VERSION_CODES.Q) {
      return;
    }
    WeakReference<VideoSurfaceView> localInstance = sInstance;
    VideoSurfaceView view = localInstance != null ? localInstance.get() : null;
    if (view == null) {
      return;
    }
    try {
      SurfaceControl sc = view.getSurfaceControl();
      if (sc == null || !sc.isValid()) {
        return;
      }
      Api29Helper.applyVisibility(sc, visible);
      Log.i(TAG, "setVideoSurfaceVisible: " + visible);
    } catch (Throwable t) {
      // Catch Throwable (both Exception and Error) to prevent JNI crashes if the JVM
      // throws linkage errors (e.g., NoClassDefFoundError) during ClassLoader unloading
      // in teardown. See b/455621481.
      Log.w(TAG, "Failed to set SurfaceControl visibility", t);
    }
  }

  @RequiresApi(Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
  private static class Api34Helper {
    static void applyVisibilityWithNextFrame(
        VideoSurfaceView view, SurfaceControl sc, boolean visible) {
      SurfaceControl.Transaction transaction =
          Api29Helper.setVisibility(new SurfaceControl.Transaction(), sc, visible);
      try {
        // On success, the system takes ownership of the transaction, so don't close it.
        view.applyTransactionToFrame(transaction);
      } catch (IllegalStateException e) {
        transaction.close();
        throw e;
      }
    }
  }

  /**
   * Shows or hides the video surface together with the next frame queued to it, so that the
   * visibility change and the frame are displayed in the same SurfaceFlinger transaction. Must be
   * called while no frames are being rendered to the surface, otherwise it is undefined which frame
   * the change is applied with. Only supported on Android 14 (API 34) and above.
   *
   * @return true if the change was attached to the next frame.
   */
  @VisibleForTesting
  static boolean setVideoSurfaceVisibleWithNextFrame(boolean visible) {
    if (Build.VERSION.SDK_INT < Build.VERSION_CODES.UPSIDE_DOWN_CAKE) {
      return false;
    }
    WeakReference<VideoSurfaceView> localInstance = sInstance;
    VideoSurfaceView view = localInstance != null ? localInstance.get() : null;
    if (view == null) {
      return false;
    }
    try {
      SurfaceControl sc = view.getSurfaceControl();
      if (sc == null || !sc.isValid()) {
        return false;
      }
      Api34Helper.applyVisibilityWithNextFrame(view, sc, visible);
      Log.i(TAG, "setVideoSurfaceVisible: " + visible + " (with next frame)");
      return true;
    } catch (Throwable t) {
      // E.g., IllegalStateException if the surface does not exist. Catch Throwable (both
      // Exception and Error) to prevent JNI crashes if the JVM throws linkage errors (e.g.,
      // NoClassDefFoundError) during ClassLoader unloading in teardown. See b/455621481.
      Log.w(TAG, "Failed to attach SurfaceControl visibility to the next frame", t);
      return false;
    }
  }

  /**
   * Shows the video surface together with the next frame queued to it, falling back to showing it
   * immediately if attaching to the next frame is not supported or fails.
   */
  @CalledByNative
  public static void showVideoSurfaceWithNextFrame() {
    if (!setVideoSurfaceVisibleWithNextFrame(true)) {
      setVideoSurfaceVisible(true);
    }
  }

  static void setInstanceForTesting(VideoSurfaceView view) {
    sInstance = view != null ? new WeakReference<>(view) : null;
  }
}
