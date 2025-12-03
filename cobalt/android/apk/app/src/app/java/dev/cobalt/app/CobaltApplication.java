// Copyright 2018 The Cobalt Authors. All Rights Reserved.
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

import android.app.Application;
import android.content.Context;
import dev.cobalt.coat.StarboardBridge;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ContextUtils;
import org.chromium.base.PathUtils;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.library_loader.LibraryProcessType;

/** Android Application hosting the Starboard application. */
public class CobaltApplication extends Application implements StarboardBridge.HostApplication {
  private static final String PRIVATE_DATA_DIRECTORY_SUFFIX = "content_shell";

  StarboardBridge mStarboardBridge;

  @Override
  public void setStarboardBridge(StarboardBridge starboardBridge) {
    mStarboardBridge = starboardBridge;
  }

  @Override
  public StarboardBridge getStarboardBridge() {
    return mStarboardBridge;
  }

  @Override
  protected void attachBaseContext(Context base) {
      super.attachBaseContext(base);
      boolean isBrowserProcess = !ContextUtils.getProcessName().contains(":");
      ContextUtils.initApplicationContext(this);
      LibraryLoader.getInstance().setLibraryProcessType(isBrowserProcess
                      ? LibraryProcessType.PROCESS_BROWSER
                      : LibraryProcessType.PROCESS_CHILD);
      if (isBrowserProcess) {
          PathUtils.setPrivateDataDirectorySuffix(PRIVATE_DATA_DIRECTORY_SUFFIX);
          ApplicationStatus.initialize(this);
      }
  }
}
