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
  }

  @Override
  protected void onCreate(Bundle savedInstanceState) {
    super.onCreate(savedInstanceState);

    String startDeepLink = getIntentUrlAsString(getIntent());
    if (getStarboardBridge() == null) {
      // Cold start - Instantiate the singleton BaseStarboardBridge.
      BaseStarboardBridge starboardBridge = createStarboardBridge(getArgs(), startDeepLink);
      ((BaseStarboardBridge.HostApplication) getApplication()).setStarboardBridge(starboardBridge);
      // Spawn the loader thread.
      MainActivityJni.get().startLoader();
    } else if (savedInstanceState == null) {
      // Warm start - Pass the deep link to the running Starboard app.
      getStarboardBridge().handleDeepLink(startDeepLink);
    }
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
