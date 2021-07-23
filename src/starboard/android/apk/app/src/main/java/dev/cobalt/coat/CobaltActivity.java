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

import android.app.NativeActivity;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.content.pm.PackageManager;
import android.content.pm.PackageManager.NameNotFoundException;
import android.media.AudioManager;
import android.net.Uri;
import android.os.Bundle;
import android.util.Pair;
import android.view.ViewGroup.LayoutParams;
import android.view.ViewParent;
import android.widget.FrameLayout;
import dev.cobalt.media.MediaCodecUtil;
import dev.cobalt.media.VideoSurfaceView;
import dev.cobalt.util.DisplayUtil;
import dev.cobalt.util.Log;
import dev.cobalt.util.UsedByNative;
import java.security.Timestamp;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/** Native activity that has the required JNI methods called by the Starboard implementation. */
public abstract class CobaltActivity extends NativeActivity {

  // A place to put args while debugging so they're used even when starting from the launcher.
  // This should always be empty in submitted code.
  private static final String[] DEBUG_ARGS = {};

  private static final String URL_ARG = "--url=";
  private static final java.lang.String META_DATA_APP_URL = "cobalt.APP_URL";

  private static final String SPLASH_URL_ARG = "--fallback_splash_screen_url=";
  private static final String SPLASH_TOPICS_ARG = "--fallback_splash_screen_topics=";
  private static final java.lang.String META_DATA_SPLASH_URL = "cobalt.SPLASH_URL";
  private static final java.lang.String META_DATA_SPLASH_TOPICS = "cobalt.SPLASH_TOPIC";

  private static final String FORCE_MIGRATION_FOR_STORAGE_PARTITIONING =
      "--force_migration_for_storage_partitioning";
  private static final String META_FORCE_MIGRATION_FOR_STORAGE_PARTITIONING =
      "cobalt.force_migration_for_storage_partitioning";

  @SuppressWarnings("unused")
  private CobaltA11yHelper a11yHelper;

  private VideoSurfaceView videoSurfaceView;
  private KeyboardEditor keyboardEditor;

  private boolean forceCreateNewVideoSurfaceView = false;

  private long timeInNanoseconds;

  private static native void nativeLowMemoryEvent();

  @Override
  protected void onCreate(Bundle savedInstanceState) {
    // Record the application start timestamp.
    timeInNanoseconds = System.nanoTime();

    // To ensure that volume controls adjust the correct stream, make this call
    // early in the app's lifecycle. This connects the volume controls to
    // STREAM_MUSIC whenever the target activity or fragment is visible.
    setVolumeControlStream(AudioManager.STREAM_MUSIC);

    String startDeepLink = getIntentUrlAsString(getIntent());
    if (getStarboardBridge() == null) {
      // Cold start - Instantiate the singleton StarboardBridge.
      StarboardBridge starboardBridge = createStarboardBridge(getArgs(), startDeepLink);
      ((StarboardBridge.HostApplication) getApplication()).setStarboardBridge(starboardBridge);
    } else {
      // Warm start - Pass the deep link to the running Starboard app.
      getStarboardBridge().handleDeepLink(startDeepLink);
    }

    // super.onCreate() will cause an APP_CMD_START in native code,
    // so make sure to initialize any state beforehand that might be touched by
    // native code invocations.
    super.onCreate(savedInstanceState);

    videoSurfaceView = new VideoSurfaceView(this);
    a11yHelper = new CobaltA11yHelper(videoSurfaceView);
    addContentView(
        videoSurfaceView, new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT));

    if (KeyboardInputConnection.nativeHasOnScreenKeyboard()) {
      keyboardEditor = new KeyboardEditor(this);
      addContentView(
          keyboardEditor, new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT));
    }
  }

  /**
   * Instantiates the StarboardBridge. Apps not supporting sign-in should inject an instance of
   * NoopUserAuthorizer. Apps may subclass StarboardBridge if they need to override anything.
   */
  protected abstract StarboardBridge createStarboardBridge(String[] args, String startDeepLink);

  @UsedByNative
  protected StarboardBridge getStarboardBridge() {
    return ((StarboardBridge.HostApplication) getApplication()).getStarboardBridge();
  }

  @Override
  protected void onStart() {
    if (!isReleaseBuild()) {
      getStarboardBridge().getAudioOutputManager().dumpAllOutputDevices();
      MediaCodecUtil.dumpAllDecoders();
    }
    if (forceCreateNewVideoSurfaceView) {
      Log.w(TAG, "Force to create a new video surface.");
      createNewSurfaceView();
    }

    DisplayUtil.cacheDefaultDisplay(this);

    getStarboardBridge().onActivityStart(this, keyboardEditor);
    super.onStart();
  }

  @Override
  protected void onStop() {
    getStarboardBridge().onActivityStop(this);
    super.onStop();

    if (VideoSurfaceView.getCurrentSurface() != null) {
      forceCreateNewVideoSurfaceView = true;
    }
  }

  @Override
  protected void onDestroy() {
    super.onDestroy();
    getStarboardBridge().onActivityDestroy(this);
  }

  @Override
  public boolean onSearchRequested() {
    return getStarboardBridge().onSearchRequested();
  }

  /** Returns true if the argument list contains an arg starting with argName. */
  private static boolean hasArg(List<String> args, String argName) {
    for (String arg : args) {
      if (arg.startsWith(argName)) {
        return true;
      }
    }
    return false;
  }

  /**
   * Get argv/argc style args, if any from intent extras. Returns empty array if there are none
   *
   * <p>To use, invoke application via, eg, adb shell am start --esa args arg1,arg2 \
   * dev.cobalt.coat/dev.cobalt.app.MainActivity
   */
  protected String[] getArgs() {
    Bundle extras = getIntent().getExtras();
    CharSequence[] argsExtra =
        (extras == null || isReleaseBuild()) ? null : extras.getCharSequenceArray("args");

    List<String> args = new ArrayList<>(Arrays.asList(DEBUG_ARGS));
    if (argsExtra != null) {
      for (int i = 0; i < argsExtra.length; i++) {
        // Replace escaped commas with commas. In order to have a comma in the arg string, it has
        // to be escaped when forming the Intent with "am start --esa". However, "am" doesn't remove
        // the escape after splitting on unescaped commas, so it's still in the string we get.
        args.add(argsExtra[i].toString().replace("\\,", ","));
      }
    }

    // If the URL arg isn't specified, get it from AndroidManifest.xml.
    boolean hasUrlArg = hasArg(args, URL_ARG);
    // If the splash screen url arg isn't specified, get it from AndroidManifest.xml.
    boolean hasSplashUrlArg = hasArg(args, SPLASH_URL_ARG);
    // If the splash screen topics arg isn't specified, get it from AndroidManifest.xml.
    boolean hasSplashTopicsArg = hasArg(args, SPLASH_TOPICS_ARG);
    if (!hasUrlArg || !hasSplashUrlArg || !hasSplashTopicsArg) {
      try {
        ActivityInfo ai =
            getPackageManager()
                .getActivityInfo(getIntent().getComponent(), PackageManager.GET_META_DATA);
        if (ai.metaData != null) {
          if (!hasUrlArg) {
            String url = ai.metaData.getString(META_DATA_APP_URL);
            if (url != null) {
              args.add(URL_ARG + url);
            }
          }
          if (!hasSplashUrlArg) {
            String splashUrl = ai.metaData.getString(META_DATA_SPLASH_URL);
            if (splashUrl != null) {
              args.add(SPLASH_URL_ARG + splashUrl);
            }
          }
          if (!hasSplashTopicsArg) {
            String splashTopics = ai.metaData.getString(META_DATA_SPLASH_TOPICS);
            if (splashTopics != null) {
              args.add(SPLASH_TOPICS_ARG + splashTopics);
            }
          }
          if (ai.metaData.getBoolean(META_FORCE_MIGRATION_FOR_STORAGE_PARTITIONING)) {
            args.add(FORCE_MIGRATION_FOR_STORAGE_PARTITIONING);
          }
        }
      } catch (NameNotFoundException e) {
        throw new RuntimeException("Error getting activity info", e);
      }
    }

    addCustomProxyArgs(args);

    return args.toArray(new String[0]);
  }

  private static void addCustomProxyArgs(List<String> args) {
    Pair<String, String> config = detectSystemProxyConfig();

    if (config.first == null || config.second == null) {
      return;
    }

    try {
      int port = Integer.parseInt(config.second);
      if (port <= 0 || port > 0xFFFF) {
        return;
      }

      String customProxy = String.format("--proxy=\"http=http://%s:%d\"", config.first, port);
      Log.i(TAG, "addCustomProxyArgs: " + customProxy);
      args.add(customProxy);
    } catch (NumberFormatException e) {
      Log.w(TAG, String.format("http.proxyPort: %s is not valid number", config.second), e);
    }
  }

  private static Pair<String, String> detectSystemProxyConfig() {
    String httpHost = System.getProperty("http.proxyHost", null);
    String httpPort = System.getProperty("http.proxyPort", null);
    return new Pair<String, String>(httpHost, httpPort);
  }

  protected boolean isReleaseBuild() {
    return StarboardBridge.isReleaseBuild();
  }

  @Override
  protected void onNewIntent(Intent intent) {
    getStarboardBridge().handleDeepLink(getIntentUrlAsString(intent));
  }

  /**
   * Returns the URL from an Intent as a string. This may be overridden for additional processing.
   */
  protected String getIntentUrlAsString(Intent intent) {
    Uri intentUri = intent.getData();
    return (intentUri == null) ? null : intentUri.toString();
  }

  @Override
  protected void onActivityResult(int requestCode, int resultCode, Intent data) {
    super.onActivityResult(requestCode, resultCode, data);
    getStarboardBridge().onActivityResult(requestCode, resultCode, data);
  }

  @Override
  public void onRequestPermissionsResult(
      int requestCode, String[] permissions, int[] grantResults) {
    getStarboardBridge().onRequestPermissionsResult(requestCode, permissions, grantResults);
  }

  public void resetVideoSurface() {
    runOnUiThread(
        new Runnable() {
          @Override
          public void run() {
            createNewSurfaceView();
          }
        });
  }

  public void setVideoSurfaceBounds(final int x, final int y, final int width, final int height) {
    runOnUiThread(
        new Runnable() {
          @Override
          public void run() {
            LayoutParams layoutParams = videoSurfaceView.getLayoutParams();
            // Since videoSurfaceView is added directly to the Activity's content view, which is a
            // FrameLayout, we expect its layout params to become FrameLayout.LayoutParams.
            if (layoutParams instanceof FrameLayout.LayoutParams) {
              ((FrameLayout.LayoutParams) layoutParams).setMargins(x, y, x + width, y + height);
            } else {
              Log.w(
                  TAG,
                  "Unexpected video surface layout params class "
                      + layoutParams.getClass().getName());
            }
            layoutParams.width = width;
            layoutParams.height = height;
            // Even though as a NativeActivity we're not using the Android UI framework, by setting
            // the  layout params it will force a layout to be requested. That will cause the
            // SurfaceView to position its underlying Surface to match the screen coordinates of
            // where the view would be in a UI layout and to set the surface transform matrix to
            // match the view's size.
            videoSurfaceView.setLayoutParams(layoutParams);
          }
        });
  }

  private void createNewSurfaceView() {
    ViewParent parent = videoSurfaceView.getParent();
    if (parent instanceof FrameLayout) {
      FrameLayout frameLayout = (FrameLayout) parent;
      int index = frameLayout.indexOfChild(videoSurfaceView);
      frameLayout.removeView(videoSurfaceView);
      videoSurfaceView = new VideoSurfaceView(this);
      a11yHelper = new CobaltA11yHelper(videoSurfaceView);
      frameLayout.addView(
          videoSurfaceView,
          index,
          new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT));
    } else {
      Log.w(TAG, "Unexpected surface view parent class " + parent.getClass().getName());
    }
  }

  @Override
  public void onLowMemory() {
    super.onLowMemory();
    nativeLowMemoryEvent();
  }

  public long getAppStartTimestamp() {
    return timeInNanoseconds;
  }
}
