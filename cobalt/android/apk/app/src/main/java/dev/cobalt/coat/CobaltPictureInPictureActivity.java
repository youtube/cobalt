// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

import android.app.Activity;
import android.app.PictureInPictureParams;
import android.content.Context;
import android.content.Intent;
import android.os.Build;
import android.os.Bundle;
import android.util.Rational;
import android.view.View;
import android.view.ViewGroup;
import dev.cobalt.util.Log;
import org.chromium.base.ContextUtils;
import org.chromium.components.thinwebview.CompositorView;
import org.chromium.components.thinwebview.CompositorViewFactory;
import org.chromium.components.thinwebview.ThinWebViewConstraints;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.base.IntentRequestTracker;
import org.chromium.ui.base.WindowAndroid;
import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

/**
 * A simple Activity to host the Video Picture-in-Picture window on Android TV. This Activity is
 * launched by C++ CobaltVideoOverlayWindow and immediately enters PiP mode.
 */
@JNINamespace("cobalt")
public class CobaltPictureInPictureActivity extends Activity {
  private long mNativeCobaltVideoOverlayWindow = 0;
  private CompositorView mCompositorView;
  private ActivityWindowAndroid mWindowAndroid;

  /**
   * Called by C++ to launch the Picture-in-Picture window.
   *
   * @param webContents Used to retrieve the foreground Activity (the main Cobalt app). This
   *     Activity serves as the Context needed to construct and start the Intent.
   * @param nativePointer The memory address of the C++ CobaltVideoOverlayWindow that initiated the
   *     PiP request. We pack this into the Intent so that when the new Java Activity starts, it can
   *     extract the pointer and use JNI to bind itself back to the C++ object.
   */
  @CalledByNative
  public static void launchActivity(WebContents webContents, long nativePointer) {
    Log.d(TAG, "Reached Java launchActivity! nativePtr=" + nativePointer);
    if (webContents == null) {
      return;
    }
    Activity activity = null;
    WindowAndroid window = webContents.getTopLevelNativeWindow();
    if (window != null) {
      activity = window.getActivity().get();
    }
    Context context = (activity != null) ? activity : ContextUtils.getApplicationContext();

    Intent intent = new Intent(context, CobaltPictureInPictureActivity.class);
    intent.putExtra("native_pointer", nativePointer);
    if (activity == null) {
      intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
    }
    context.startActivity(intent);
  }

  @Override
  protected void onCreate(Bundle savedInstanceState) {
    super.onCreate(savedInstanceState);

    Intent intent = getIntent();
    if (intent == null) {
      finish();
      return;
    }
    mNativeCobaltVideoOverlayWindow = intent.getLongExtra("native_pointer", 0);
    if (mNativeCobaltVideoOverlayWindow == 0) {
      finish();
      return;
    }

    // Register this Java instance with the native C++ controller.
    getNatives()
        .setJavaActivity(mNativeCobaltVideoOverlayWindow, CobaltPictureInPictureActivity.this);

    mWindowAndroid =
        new ActivityWindowAndroid(
            this,
            /* listenToActivityState= */ true,
            IntentRequestTracker.createFromActivity(this),
            null,
            /* trackOcclusion= */ false);

    if (sCompositorViewForTesting != null) {
      mCompositorView = sCompositorViewForTesting;
    } else {
      mCompositorView =
          CompositorViewFactory.create(this, mWindowAndroid, new ThinWebViewConstraints());
    }
    if (mCompositorView == null) {
      finish();
      return;
    }
    addContentView(
        mCompositorView.getView(),
        new ViewGroup.LayoutParams(
            ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));

    mCompositorView
        .getView()
        .addOnLayoutChangeListener(
            new View.OnLayoutChangeListener() {
              @Override
              public void onLayoutChange(
                  View v,
                  int left,
                  int top,
                  int right,
                  int bottom,
                  int oldLeft,
                  int oldTop,
                  int oldRight,
                  int oldBottom) {
                if (mNativeCobaltVideoOverlayWindow == 0) return;
                if (top == bottom || left == right) return;
                getNatives()
                    .onViewSizeChanged(mNativeCobaltVideoOverlayWindow, right - left, bottom - top);
              }
            });

    getNatives().compositorViewCreated(mNativeCobaltVideoOverlayWindow, mCompositorView);

    // Attempt to enter PiP mode immediately using system default aspect ratio,
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
      PictureInPictureParams params = new PictureInPictureParams.Builder().build();
      if (!enterPictureInPictureMode(params)) {
        finish();
      }
    } else {
      finish();
    }
  }

  @CalledByNative
  public WindowAndroid getWindowAndroid() {
    return mWindowAndroid;
  }

  @CalledByNative
  public void closeActivity() {
    // Do NOT clear mNativeCobaltVideoOverlayWindow here, otherwise
    // onDestroy() will fail to notify C++ OnActivityDestroyed!
    finish();
  }

  @CalledByNative
  public void updateVideoSize(int width, int height) {
    if (isFinishing() || isDestroyed()) return;
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O && width > 0 && height > 0) {
      try {
        PictureInPictureParams params =
            new PictureInPictureParams.Builder()
                .setAspectRatio(new Rational(width, height))
                .build();
        setPictureInPictureParams(params);
      } catch (IllegalStateException e) {
        // Ignore if the WindowManager token is already invalid
      }
    }
  }

  @Override
  protected void onDestroy() {
    if (mNativeCobaltVideoOverlayWindow != 0) {
      getNatives().onActivityDestroyed(mNativeCobaltVideoOverlayWindow);
      mNativeCobaltVideoOverlayWindow = 0;
    }
    if (mCompositorView != null) {
      mCompositorView.destroy();
      mCompositorView = null;
    }
    if (mWindowAndroid != null) {
      mWindowAndroid.destroy();
      mWindowAndroid = null;
    }
    super.onDestroy();
  }

  private static Natives sNatives;

  public static void setNativesForTesting(Natives natives) {
    sNatives = natives;
  }

  private static Natives getNatives() {
    if (sNatives != null) {
      return sNatives;
    }
    return CobaltPictureInPictureActivityJni.get();
  }

  private static CompositorView sCompositorViewForTesting;

  public static void setCompositorViewForTesting(CompositorView compositorView) {
    sCompositorViewForTesting = compositorView;
  }

  @NativeMethods
  public interface Natives {
    void setJavaActivity(
        long nativeCobaltVideoOverlayWindow, CobaltPictureInPictureActivity caller);

    void onActivityDestroyed(long nativeCobaltVideoOverlayWindow);

    void onViewSizeChanged(long nativeCobaltVideoOverlayWindow, int width, int height);

    void compositorViewCreated(long nativeCobaltVideoOverlayWindow, CompositorView compositorView);
  }
}
