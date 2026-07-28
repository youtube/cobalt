// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

package org.chromium.content_browsertests_apk;

import android.app.Activity;
import android.app.Service;
import android.view.Window;
import android.view.WindowManager;
import dev.cobalt.coat.StarboardBridge;
import dev.cobalt.util.Holder;
import org.chromium.base.StrictModeContext;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.content_public.browser.BrowserStartupController;
import org.chromium.content_public.browser.BrowserStartupController.StartupCallback;
import org.chromium.native_test.NativeBrowserTest;
import org.chromium.native_test.NativeBrowserTestActivity;

/** An Activity base class for running browser tests against Cobalt. */
public abstract class CobaltBrowserTestActivity extends NativeBrowserTestActivity
    implements StarboardBridge.HostApplication {
  private static final String TAG = "CobaltBrowserTest";

  private StarboardBridge mStarboardBridge;

  @Override
  public void setStarboardBridge(StarboardBridge starboardBridge) {
    mStarboardBridge = starboardBridge;
  }

  @Override
  public StarboardBridge getStarboardBridge() {
    return mStarboardBridge;
  }

  @Override
  protected void initializeBrowserProcess() {
    try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
      LibraryLoader.getInstance().ensureInitialized();
    }

    mStarboardBridge =
        new StarboardBridge(
            getApplicationContext(),
            new Holder<Activity>(),
            new Holder<Service>(),
            null, // ArtworkDownloader is not needed for tests
            new String[0], // args
            ""); // startDeepLink
    ((StarboardBridge.HostApplication) getApplication()).setStarboardBridge(mStarboardBridge);

    Window wind = this.getWindow();
    wind.addFlags(WindowManager.LayoutParams.FLAG_DISMISS_KEYGUARD);
    wind.addFlags(WindowManager.LayoutParams.FLAG_SHOW_WHEN_LOCKED);
    wind.addFlags(WindowManager.LayoutParams.FLAG_TURN_SCREEN_ON);

    BrowserStartupController.getInstance()
        .setContentMainCallbackForTests(
            () -> {
              runTests();
            });
    BrowserStartupController.getInstance()
        .startBrowserProcessesAsync(
            LibraryProcessType.PROCESS_BROWSER,
            false,
            false,
            new StartupCallback() {
              @Override
              public void onSuccess() {
                NativeBrowserTest.javaStartupTasksComplete();
              }

              @Override
              public void onFailure() {
                throw new RuntimeException("Failed to startBrowserProcessesAsync()");
              }
            });
  }

  @Override
  protected String getUserDataDirectoryCommandLineSwitch() {
    return "user-data-dir";
  }
}
