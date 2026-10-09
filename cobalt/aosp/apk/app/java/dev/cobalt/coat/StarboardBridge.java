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

import android.app.Activity;
import android.app.Service;
import android.content.Context;
import dev.cobalt.util.Holder;

/**
 * The AOSP StarboardBridge. It shows the platform error dialog, using the AOSP PlatformError.
 *
 * <p>TODO(b/534656433): Shared code such as CobaltService refers to StarboardBridge, the Android TV
 * subclass, so AOSP has to provide a class with this name. Remove those references.
 */
public class StarboardBridge extends BaseStarboardBridge {
  // The latest platform error. It is raised again when an activity starts, if it is still pending.
  private volatile PlatformError mPlatformError;

  public StarboardBridge(
      Context appContext,
      Holder<Activity> activityHolder,
      Holder<Service> serviceHolder,
      String[] args,
      String startDeepLink) {
    super(appContext, activityHolder, serviceHolder, args, startDeepLink);
  }

  @Override
  void raisePlatformError(int errorType, long data, String url, boolean disableDismiss) {
    mPlatformError = new PlatformError(mActivityHolder, errorType, data, disableDismiss);
    mPlatformError.raise();
  }

  @Override
  protected void onActivityStart(Activity activity) {
    super.onActivityStart(activity);
    if (mPlatformError != null && mPlatformError.isPending()) {
      mPlatformError.raise();
    }
  }
}
