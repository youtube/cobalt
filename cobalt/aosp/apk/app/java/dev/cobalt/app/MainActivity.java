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

package dev.cobalt.app;

import android.app.Activity;
import android.app.Service;
import android.os.Bundle;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import dev.cobalt.coat.BaseCobaltActivity;
import dev.cobalt.coat.BaseStarboardBridge;
import dev.cobalt.coat.CobaltService;
import dev.cobalt.libraries.services.clientloginfo.ClientLogInfoModule;
import dev.cobalt.util.DisplayUtil;
import dev.cobalt.util.Holder;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

/**
 * Main Activity for the "Cobalt on AOSP" app.
 *
 * <p>The lifecycle is handled by the base class. MainActivity loads the evergreen loader library,
 * creates the BaseStarboardBridge, and spawns the native Starboard thread.
 */
@JNINamespace("starboard")
public class MainActivity extends BaseCobaltActivity {

  static {
    // Each aosp apk ships exactly one app loader under its natural name
    // via android_apk shared_libraries: libloader_app.so for the production
    // app or libelf_loader_sandbox.so for everything else. Load whichever is
    // present.
    UnsatisfiedLinkError loadError = null;
    for (String lib : new String[] {"loader_app", "elf_loader_sandbox"}) {
      try {
        System.loadLibrary(lib);
        loadError = null;
        break;
      } catch (UnsatisfiedLinkError e) {
        loadError = e;
      }
    }
    if (loadError != null) {
      throw loadError;
    }
  }

  @NativeMethods
  interface Natives {
    // Spawns the loader thread, whose main() runs the app loader.
    void startLoader();
    void nativeOnSurfaceCreated(Surface surface);
    void nativeOnSurfaceDestroyed();
  }

  private boolean mStarboardStarted;

  @Override
  protected void onCreate(Bundle savedInstanceState) {
    super.onCreate(savedInstanceState);

    // BaseStarboardBridge.getDisplaySize()/getDisplayDpi() read the DisplayMetrics DisplayUtil
    // caches, which stays null until cacheDefaultDisplay() runs. CobaltActivity does this from
    // onStart(); the AOSP app doesn't build CobaltActivity, so do it here.
    DisplayUtil.cacheDefaultDisplay(this);

    String startDeepLink = getIntentUrlAsString(getIntent());
    boolean coldStart = getStarboardBridge() == null;
    if (coldStart) {
      // Cold start - Instantiate the singleton BaseStarboardBridge.
      BaseStarboardBridge starboardBridge = createStarboardBridge(getArgs(), startDeepLink);
      ((BaseStarboardBridge.HostApplication) getApplication()).setStarboardBridge(starboardBridge);
    } else if (savedInstanceState == null) {
      // Warm start - Pass the deep link to the running Starboard app.
      getStarboardBridge().handleDeepLink(startDeepLink);
    }

    // The NDK uses an ANativeWindow to represent a producer of an image queue - it can send the
    // produced images to other consumers or it can be displayed on screen.
    // The SurfaceView creates a drawing surface in the view hierarchy, creating an ANativeWindow
    // set to act as the rendering surface that we need to render cobalt UI. It's not immediately
    // available, so we set a callback to wait for the surface, store it and then start the loader.
    SurfaceView surfaceView = new SurfaceView(this);
    surfaceView
        .getHolder()
        .addCallback(
            new SurfaceHolder.Callback() {
              @Override
              public void surfaceCreated(SurfaceHolder holder) {
                MainActivityJni.get().nativeOnSurfaceCreated(holder.getSurface());
                if (coldStart && !mStarboardStarted) {
                  mStarboardStarted = true;
                  // Spawn the loader thread.
                  MainActivityJni.get().startLoader();
                }
              }

              @Override
              public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {}

              @Override
              public void surfaceDestroyed(SurfaceHolder holder) {
                MainActivityJni.get().nativeOnSurfaceDestroyed();
              }
            });
    setContentView(surfaceView);
  }

  @Override
  protected BaseStarboardBridge createStarboardBridge(String[] args, String startDeepLink) {
    Holder<Activity> activityHolder = new Holder<>();
    Holder<Service> serviceHolder = new Holder<>();
    BaseStarboardBridge bridge =
        new BaseStarboardBridge(
            getApplicationContext(), activityHolder, serviceHolder, args, startDeepLink);

    CobaltService.Factory clientLogInfoFactory =
        new ClientLogInfoModule().provideFactory(getApplicationContext());
    bridge.registerCobaltService(clientLogInfoFactory);

    return bridge;
  }
}
