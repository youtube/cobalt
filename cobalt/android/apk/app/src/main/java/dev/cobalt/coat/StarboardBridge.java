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

package dev.cobalt.coat;

import static dev.cobalt.util.Log.TAG;

import android.app.Activity;
import android.app.Service;
import android.content.Context;
import dev.cobalt.shell.StartupGuard;
import dev.cobalt.util.Holder;
import dev.cobalt.util.Log;
import java.util.HashMap;
import org.chromium.content_public.browser.WebContents;

/** Implementation of the required JNI methods called by the Starboard C++ code. */
public class StarboardBridge extends BaseStarboardBridge {

  private CobaltMediaSession mCobaltMediaSession;
  private VolumeStateReceiver mVolumeStateReceiver;
  private PlatformError mPlatformError;

  public StarboardBridge(
      Context appContext,
      Holder<Activity> activityHolder,
      Holder<Service> serviceHolder,
      ArtworkDownloader artworkDownloader,
      String[] args,
      String startDeepLink) {
    super(appContext, activityHolder, serviceHolder, args, startDeepLink);
    mCobaltMediaSession = new CobaltMediaSession(appContext, activityHolder, artworkDownloader);
    mVolumeStateReceiver = new VolumeStateReceiver(appContext);
  }

  @Override
  protected void onActivityStop(Activity activity) {
    Log.i(TAG, "onActivityStop ran");
    beforeSuspend();
    mCobaltMediaSession.onActivityStop();
    if (mActivityHolder.get() == activity) {
      mActivityHolder.set(null);
    }
    mSysConfigChangeReceiver.setForeground(false);
  }

  @Override
  void raisePlatformError(int errorType, long data, String url) {
    StartupGuard.getInstance().setStartupMilestone(37);
    mPlatformError = new PlatformError(mActivityHolder, errorType, data, url);
    mPlatformError.raise();
  }

  @Override
  public boolean isPlatformErrorShowing() {
    if (mPlatformError != null) {
      return mPlatformError.isShowing();
    }
    return false;
  }

  void onRequestPermissionsResult(int requestCode, String[] permissions, int[] grantResults) {
    mAudioPermissionRequester.onRequestPermissionsResult(requestCode, permissions, grantResults);
  }

  @Override
  public void resetVideoSurface() {
    Activity activity = mActivityHolder.get();
    if (activity instanceof CobaltActivity) {
      ((CobaltActivity) activity).resetVideoSurface();
    }
  }

  @Override
  public void setVideoSurfaceBounds(final int x, final int y, final int width, final int height) {
    Activity activity = mActivityHolder.get();
    if (activity instanceof CobaltActivity) {
      ((CobaltActivity) activity).setVideoSurfaceBounds(x, y, width, height);
    }
  }

  public CobaltMediaSession cobaltMediaSession() {
    return mCobaltMediaSession;
  }

  @Override
  public void setCrashContext(String key, String value) {
    CrashContext.INSTANCE.setCrashContext(key, value);
  }

  public HashMap<String, String> getCrashContext() {
    return CrashContext.INSTANCE.getCrashContext();
  }

  public void registerCrashContextUpdateHandler(CrashContextUpdateHandler handler) {
    CrashContext.INSTANCE.registerCrashContextUpdateHandler(handler);
  }

  public void setWebContents(WebContents webContents) {
    mCobaltMediaSession.setWebContents(webContents);
    mVolumeStateReceiver.setWebContents(webContents);
  }

  @Override
  protected void hideSplashScreen() {
    StartupGuard.getInstance().disarm();
  }

  @Override
  protected void setStartupMilestone(int milestone) {
    StartupGuard.getInstance().setStartupMilestone(milestone);
  }

  @Override
  protected void setStartupDiagnosisInfo(String key, String value) {
    StartupGuard.getInstance().setDiagnosisInfo(key, value);
  }
}
