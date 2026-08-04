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
    // The dedicated graphics SurfaceView publishes its Surface to Starboard; its
    // ANativeWindow backs the inner library's Ozone SbWindow (see main_activity.cc).
    void nativeOnSurfaceCreated(Surface surface);
    void nativeOnSurfaceDestroyed();
  }

  // Starboard is booted exactly once, gated on the graphics surface existing.
  private boolean mStarboardStarted;

  @Override
  protected void onCreate(Bundle savedInstanceState) {
    super.onCreate(savedInstanceState);

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

    // Platform AudioSink init is deferred together with the rest of the media subsystem.
    // initializePlatformAudioSink() runs SbAudioSinkImpl's startup min-frames probe, whose render
    // thread reads a Starboard FeatureList feature (kReleaseVideoFramesAfterAudioStarts) before
    // anything initializes the list. The list lives in the loader (Android toolchain) and nplb's
    // inner lib (Linux toolchain) can't seed it, so the probe aborts. The media/audio/player/drm
    // nplb tests that actually need the sink are excluded via the gtest filter, so the sink isn't
    // needed yet. Re-enable this (and initialize the FeatureList) when the media subsystem is
    // brought up.
    // getStarboardBridge().initializePlatformAudioSink();

    // The inner libcobalt is Linux and renders through Ozone-Starboard, whose SbWindowCreate()
    // needs a real on-screen ANativeWindow. Provide one via a dedicated graphics SurfaceView, and
    // gate the native Starboard boot on surfaceCreated so SbWindowCreate() finds the surface
    // synchronously (the x11/raspi modular platforms always have a display; here we wait for the
    // Android view system to hand us one). Any state native code touches in startup must be
    // initialized above first. The loader is spawned only on cold start (a warm start reuses the
    // already-running Starboard process).
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
