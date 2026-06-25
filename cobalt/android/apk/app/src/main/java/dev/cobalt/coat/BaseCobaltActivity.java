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

import android.app.Activity;
import android.content.ComponentName;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.content.pm.PackageManager;
import android.content.pm.PackageManager.NameNotFoundException;
import android.media.AudioManager;
import android.net.Uri;
import android.os.Bundle;
import androidx.annotation.VisibleForTesting;
import dev.cobalt.util.Log;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.regex.Pattern;

/**
 * Base Activity shared by all Cobalt embedders. It owns the StarboardBridge lifecycle
 * notifications; and performs intent and command-line argument handling
 */
public abstract class BaseCobaltActivity extends Activity {
  static final String URL_ARG = "--url=";
  private static final String META_DATA_APP_URL = "cobalt.APP_URL";

  // This key differs in naming format for legacy reasons
  public static final String COMMAND_LINE_ARGS_KEY = "commandLineArgs";

  private static final Pattern URL_PARAM_PATTERN = Pattern.compile("^[a-zA-Z0-9_=]*$");

  protected long mTimeInNanoseconds;

  Bundle getActivityMetaData() {
    ComponentName componentName = getIntent().getComponent();
    if (componentName == null) {
      Log.w(TAG, "Activity intent has no component; cannot get metadata.");
      return null;
    }
    ActivityInfo ai;
    try {
      ai = getPackageManager().getActivityInfo(componentName, PackageManager.GET_META_DATA);
    } catch (NameNotFoundException e) {
      Log.e(TAG, "Error getting activity info", e);
      return null;
    }
    if (ai == null) {
      return null;
    }
    return ai.metaData;
  }

  static String[] getCommandLineParamsFromIntent(Intent intent, String key) {
    return intent != null ? intent.getStringArrayExtra(key) : null;
  }

  /**
   * Instantiates the BaseStarboardBridge. Apps may subclass BaseStarboardBridge if they need to
   * override anything.
   */
  protected abstract BaseStarboardBridge createStarboardBridge(String[] args, String startDeepLink);

  protected BaseStarboardBridge getStarboardBridge() {
    return ((BaseStarboardBridge.HostApplication) getApplication()).getStarboardBridge();
  }

  @Override
  protected void onCreate(Bundle savedInstanceState) {
    // Record the application start timestamp.
    mTimeInNanoseconds = System.nanoTime();

    // To ensure that volume controls adjust the correct stream, make this call
    // early in the app's lifecycle. This connects the volume controls to
    // STREAM_MUSIC whenever the target activity or fragment is visible.
    setVolumeControlStream(AudioManager.STREAM_MUSIC);

    super.onCreate(savedInstanceState);
  }

  @Override
  protected void onStart() {
    getStarboardBridge().onActivityStart(this);
    super.onStart();
  }

  @Override
  protected void onStop() {
    getStarboardBridge().onActivityStop(this);
    super.onStop();
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
  static boolean hasArg(List<String> args, String argName) {
    for (String arg : args) {
      if (arg.startsWith(argName)) {
        return true;
      }
    }
    return false;
  }

  /**
   * Get argv/argc style args, if any from intent extras. Returns empty array if there are none.
   *
   * <p>To use, invoke application via, eg, adb shell am start --esa commandLineArgs arg1,arg2 \
   * dev.cobalt.coat/dev.cobalt.app.MainActivity
   */
  protected String[] getArgs() {
    String[] commandLineArgs = null;
    Intent intent = getIntent();
    if (!isReleaseBuild()) {
      commandLineArgs = getCommandLineParamsFromIntent(intent, COMMAND_LINE_ARGS_KEY);
    }
    return constructArgs(commandLineArgs, getActivityMetaData(), intent.getExtras());
  }

  @VisibleForTesting
  static String[] constructArgs(String[] commandLineArgs, Bundle metaData, Bundle extras) {
    ArrayList<String> args = new ArrayList<>();
    if (commandLineArgs != null) {
      args.addAll(Arrays.asList(commandLineArgs));
    }

    // If the URL arg isn't specified, get it from AndroidManifest.xml.
    if (!hasArg(args, URL_ARG) && metaData != null) {
      String url = metaData.getString(META_DATA_APP_URL);
      if (url != null) {
        args.add(URL_ARG + url);
      }
    }

    CharSequence[] urlParams = (extras == null) ? null : extras.getCharSequenceArray("url_params");
    if (urlParams != null) {
      appendUrlParamsToUrl(args, urlParams);
    }

    return args.toArray(new String[0]);
  }

  @VisibleForTesting
  static void appendUrlParamsToUrl(List<String> args, CharSequence[] urlParams) {
    int idx = -1;
    for (int i = 0; i < args.size(); i++) {
      if (args.get(i).startsWith(URL_ARG)) {
        idx = i;
        break;
      }
    }

    if (idx >= 0) {
      StringBuilder urlBuilder = new StringBuilder();
      urlBuilder.append(args.get(idx));
      // append & if ? is already in the url, otherwise append ?
      if (urlBuilder.indexOf("?") > 0) {
        urlBuilder.append("&");
      } else {
        urlBuilder.append("?");
      }

      for (int j = 0; j < urlParams.length; j++) {
        // sanitize the input before append to the url.
        String paramKeyValuePair = urlParams[j].toString();
        if (URL_PARAM_PATTERN.matcher(paramKeyValuePair).matches()) {
          urlBuilder.append(paramKeyValuePair);
          urlBuilder.append('&');
        }
      }

      urlBuilder.deleteCharAt(urlBuilder.length() - 1);
      args.set(idx, urlBuilder.toString());
    }
  }

  protected boolean isReleaseBuild() {
    return BaseStarboardBridge.isReleaseBuild();
  }

  protected boolean isDevelopmentBuild() {
    return BaseStarboardBridge.isDevelopmentBuild();
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
    return (intentUri == null) ? "" : intentUri.toString();
  }

  /*
   * TODO(https://crbug.com/495203133): move the implementation from CobaltActivity here when
   * bringing up Cobalt on AOSP.
   */
  public void resetVideoSurface() {}

  public void setVideoSurfaceBounds(final int x, final int y, final int width, final int height) {}

  public long getAppStartTimestamp() {
    return mTimeInNanoseconds;
  }
}
