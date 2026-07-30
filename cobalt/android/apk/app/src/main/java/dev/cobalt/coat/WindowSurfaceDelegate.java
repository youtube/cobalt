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

import android.view.SurfaceHolder;
import dev.cobalt.shell.ContentViewRenderView;
import dev.cobalt.shell.ShellManager;
import dev.cobalt.util.Log;

/**
 * Handles the window surface lifecycle when rendering UI directly to the window surface.
 *
 * <p>Normally, Cobalt renders UI to an internal {@link android.view.SurfaceView} which sits on top
 * of the window. When the "enable-window-surface-ui" experiment is active, this internal
 * SurfaceView is disabled, and we render directly to the window's own surface.
 *
 * <p>This class implements {@link SurfaceHolder.Callback2} and takes control of the window's
 * surface (via {@link Window#takeSurface(SurfaceHolder.Callback)}). It then forwards the surface
 * lifecycle callbacks to the {@link ContentViewRenderView}'s JNI surface callbacks, which would
 * normally be triggered by the internal SurfaceView.
 *
 * <p>Lifetime and Ownership: This class is owned by {@link CobaltActivity} and its lifetime is
 * bound to the activity's lifecycle.
 *
 * <p>Threading Model: This class is UI thread-affine. All methods and callbacks must be called on
 * the main/UI thread.
 */
public class WindowSurfaceDelegate implements SurfaceHolder.Callback2 {
  private final ShellManager mShellManager;

  public WindowSurfaceDelegate(ShellManager shellManager) {
    mShellManager = shellManager;
  }

  private SurfaceHolder.Callback getSurfaceCallback() {
    ContentViewRenderView renderView =
        mShellManager != null ? mShellManager.getContentViewRenderView() : null;
    return renderView != null ? renderView.getSurfaceCallback() : null;
  }

  @Override
  public void surfaceCreated(SurfaceHolder holder) {
    Log.d(TAG, "WindowSurfaceDelegate: surfaceCreated");
    SurfaceHolder.Callback callback = getSurfaceCallback();
    if (callback == null) {
      return;
    }
    callback.surfaceCreated(holder);
  }

  @Override
  public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
    Log.d(TAG, "WindowSurfaceDelegate: surfaceChanged format=%d %dx%d", format, width, height);
    SurfaceHolder.Callback callback = getSurfaceCallback();
    if (callback == null) {
      return;
    }
    callback.surfaceChanged(holder, format, width, height);
  }

  @Override
  public void surfaceDestroyed(SurfaceHolder holder) {
    Log.d(TAG, "WindowSurfaceDelegate: surfaceDestroyed");
    SurfaceHolder.Callback callback = getSurfaceCallback();
    if (callback == null) {
      return;
    }
    callback.surfaceDestroyed(holder);
  }

  @Override
  public void surfaceRedrawNeeded(SurfaceHolder holder) {
    Log.d(TAG, "WindowSurfaceDelegate: surfaceRedrawNeeded");
    SurfaceHolder.Callback callback = getSurfaceCallback();
    if (!(callback instanceof SurfaceHolder.Callback2)) {
      return;
    }
    ((SurfaceHolder.Callback2) callback).surfaceRedrawNeeded(holder);
  }
}
