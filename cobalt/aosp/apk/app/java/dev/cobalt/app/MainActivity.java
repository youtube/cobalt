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

package dev.cobalt.app;

import android.app.Activity;
import android.app.Service;
import android.media.AudioManager;
import android.os.Bundle;
import android.view.View;

import dev.cobalt.coat.BaseCobaltActivity;
import dev.cobalt.coat.CobaltService;
import dev.cobalt.coat.BaseStarboardBridge;
import dev.cobalt.libraries.services.clientloginfo.ClientLogInfoModule;
import dev.cobalt.util.Holder;

/**
 * Main Activity for the "Cobalt on AOSP" app.
 *
 * <p>The real work is done in the abstract base class. This class is really just some factory
 * methods to "inject" things that can be customized.
 */
public class MainActivity extends BaseCobaltActivity {

  static {
    // Each aosp apk ships exactly one evergreen loader under its natural name via android_apk
    // shared_libraries: libloader_app.so for the production app (slot / update management) or
    // libelf_loader_sandbox.so for everything else. Load whichever is present — only one is, so
    // the other throws UnsatisfiedLinkError and we fall through. loadLibrary triggers JNI_OnLoad
    // (starboard/aosp/shared/starboard_library_loader.cc → copied_base base::android::InitVM),
    // without which GEN_JNI's J.N dispatches would UnsatisfiedLinkError at the first @NativeMethods
    // call. chrobalt/AndroidTV instead loads via org.chromium.base.library_loader in onCreate.
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

  // Spawns the native Starboard thread (the loader's main()). Replaces the GameActivity boot:
  // where GameActivity's super.onCreate() used to drive the AAR glue into native
  // GameActivity_onCreate, this plain Activity calls native directly. Implemented in
  // starboard/aosp/shared/android_main.cc.
  private static native void nativeStartStarboard();

  @Override
  protected void onCreate(Bundle savedInstanceState) {
    super.onCreate(savedInstanceState);

    // Record the application start timestamp (read back via getAppStartTimestamp()).
    mTimeInNanoseconds = System.nanoTime();

    // To ensure that volume controls adjust the correct stream, make this call early in the app's
    // lifecycle. This connects the volume controls to STREAM_MUSIC whenever this Activity is
    // visible.
    setVolumeControlStream(AudioManager.STREAM_MUSIC);

    String startDeepLink = getIntentUrlAsString(getIntent());
    if (getStarboardBridge() == null) {
      // Cold start - Instantiate the singleton BaseStarboardBridge.
      BaseStarboardBridge starboardBridge = createStarboardBridge(getArgs(), startDeepLink);
      ((BaseStarboardBridge.HostApplication) getApplication()).setStarboardBridge(starboardBridge);
    } else {
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

    // nplb doesn't render; the Starboard video Surface (VideoSurfaceView) is dropped. Set a plain
    // placeholder content view so the Activity still has a valid window.
    setContentView(new View(this));

    // Spawn the native Starboard thread (replaces the GameActivity boot). Any state native code
    // touches must be initialized above first.
    nativeStartStarboard();
  }

  @Override
  protected BaseStarboardBridge createStarboardBridge(String[] args, String startDeepLink) {
    Holder<Activity> activityHolder = new Holder<>();
    Holder<Service> serviceHolder = new Holder<>();
    BaseStarboardBridge bridge = new BaseStarboardBridge(
        getApplicationContext(), activityHolder, serviceHolder, args, startDeepLink);

    CobaltService.Factory clientLogInfoFactory =
        new ClientLogInfoModule().provideFactory(getApplicationContext());
    bridge.registerCobaltService(clientLogInfoFactory);

    return bridge;
  }
}
