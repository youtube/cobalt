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

package dev.cobalt.util;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.InputStreamReader;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Map;
import java.util.StringJoiner;
import org.chromium.base.BaseSwitches;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.json.JSONObject;

/** Defines the constant names for feature switches used in Kimono. */
@JNINamespace("cobalt")
public class JavaSwitches extends CobaltJavaSwitchNames {
  private static final String TAG = "JavaSwitches";

  public static final String ENABLE_LOW_END_DEVICE_MODE_SWITCH =
      "--" + BaseSwitches.ENABLE_LOW_END_DEVICE_MODE;

  /**
   * Alias for {@link CobaltJavaSwitchNames#FORCE720P_UI_ON1_GB_DEVICES} to preserve the historical
   * underscore before digits (`_720P_` / `_1GB_`).
   */
  public static final String FORCE_720P_UI_ON_1GB_DEVICES = FORCE720P_UI_ON1_GB_DEVICES;

  private static Boolean sOverrideForTesting;

  public static void setOverrideForTesting(Boolean override) {
    sOverrideForTesting = override;
  }

  @CalledByNative
  public static boolean shouldApplyExperimentConfigs() {
    if (sOverrideForTesting != null) {
      return sOverrideForTesting;
    }
    // Read persisted crash streak and threshold directly from Variations beacon and
    // Experiment Config in the cache directory.
    // This allows Java to determine whether safe mode / empty config is active during early
    // startup before native singletons and libraries are initialized, with zero extra disk writes.
    try {
      if (ContextUtils.getApplicationContext() == null) {
        return true;
      }
      File cacheDir = ContextUtils.getApplicationContext().getCacheDir();
      if (cacheDir == null) {
        return true;
      }

      // Check the Variations beacon file first, where CleanExitBeacon synchronously records
      // exit state and crash streak on Android. Fall back to Metrics Config.
      File beaconFile = new File(cacheDir, CobaltPrefNames.VARIATIONS_BEACON_FILENAME);
      boolean isBeaconFormat = true;
      if (!beaconFile.exists()) {
        beaconFile = new File(cacheDir, CobaltPrefNames.METRICS_CONFIG_FILENAME);
        isBeaconFormat = false;
        if (!beaconFile.exists()) {
          return true;
        }
      }
      String content = readFileToString(beaconFile);
      if (content == null || content.isEmpty()) {
        return true;
      }

      JSONObject json = new JSONObject(content);
      int crashStreak = json.optInt(CobaltPrefNames.VARIATIONS_CRASH_STREAK, 0);

      boolean exitedCleanly = true;
      if (isBeaconFormat) {
        exitedCleanly = json.optBoolean(CobaltPrefNames.STABILITY_EXITED_CLEANLY, true);
      } else {
        JSONObject userExp = json.optJSONObject("user_experience_metrics");
        if (userExp != null) {
          JSONObject stability = userExp.optJSONObject("stability");
          if (stability != null) {
            exitedCleanly = stability.optBoolean("exited_cleanly", true);
          }
        }
      }

      // If the previous session crashed (did not exit cleanly), native C++
      // CleanExitBeacon::Initialize() will increment the crash streak on startup.
      // Java must account for this pending increment so that Java and C++ evaluate
      // the exact same threshold during early startup.
      if (!exitedCleanly) {
        crashStreak++;
      }

      int threshold = readCrashStreakEmptyConfigThreshold(cacheDir);
      if (crashStreak >= threshold) {
        return false;
      }
    } catch (Exception e) {
      Log.w(TAG, "Failed to read crash streak or experiment config from disk", e);
    }
    return true;
  }

  private static int readCrashStreakEmptyConfigThreshold(File cacheDir) {
    File expFile = new File(cacheDir, CobaltPrefNames.EXPERIMENT_CONFIG_FILENAME);
    if (!expFile.exists()) {
      return CobaltCrashStreakThreshold.DEFAULT_CRASH_STREAK_EMPTY_CONFIG_THRESHOLD;
    }
    String expContent = readFileToString(expFile);
    if (expContent == null || expContent.isEmpty()) {
      return CobaltCrashStreakThreshold.DEFAULT_CRASH_STREAK_EMPTY_CONFIG_THRESHOLD;
    }
    try {
      JSONObject expJson = new JSONObject(expContent);
      JSONObject finchParams = expJson.optJSONObject(CobaltExperimentNames.FINCH_PARAMETERS);
      if (finchParams == null) {
        return CobaltCrashStreakThreshold.DEFAULT_CRASH_STREAK_EMPTY_CONFIG_THRESHOLD;
      }
      return finchParams.optInt(
          CobaltExperimentNames.CRASH_STREAK_EMPTY_CONFIG_THRESHOLD,
          CobaltCrashStreakThreshold.DEFAULT_CRASH_STREAK_EMPTY_CONFIG_THRESHOLD);
    } catch (Exception e) {
      return CobaltCrashStreakThreshold.DEFAULT_CRASH_STREAK_EMPTY_CONFIG_THRESHOLD;
    }
  }

  private static String readFileToString(File file) {
    StringBuilder sb = new StringBuilder();
    try (BufferedReader reader =
        new BufferedReader(
            new InputStreamReader(new FileInputStream(file), StandardCharsets.UTF_8))) {
      String line;
      while ((line = reader.readLine()) != null) {
        sb.append(line);
      }
      return sb.toString();
    } catch (Exception e) {
      return null;
    }
  }

  public static List<String> getDefaultCommandLineArgs() {
    // Only --enable-low-end-device-mode is needed before LibraryLoader initializes JNI.
    // All other default switches (--disable-quic, --force-gpu-mem-available-mb, --js-flags,
    // --force-device-scale-factor) are applied in C++ via ApplyJavaSwitches().
    List<String> defaultArgs = new ArrayList<>();
    defaultArgs.add(ENABLE_LOW_END_DEVICE_MODE_SWITCH);
    return defaultArgs;
  }

  public static List<String> getExtraCommandLineArgs(Map<String, String> javaSwitches) {
    return getExtraCommandLineArgs(javaSwitches, shouldApplyExperimentConfigs());
  }

  public static List<String> getExtraCommandLineArgs(
      Map<String, String> javaSwitches, boolean shouldApplyExperimentConfigs) {
    if (!shouldApplyExperimentConfigs) {
      return getDefaultCommandLineArgs();
    }

    if (javaSwitches == null) {
      javaSwitches = Collections.emptyMap();
    }

    List<String> extraCommandLineArgs = new ArrayList<>();

    // TODO(cobalt, b/563373348): Investigate performance impact on high-end devices. Use Java
    // switch due to IsLowEndDevice called before Finch is initialized. We should migrate to
    // Finch
    // if high-end device benefits from removing this low-end-device-mode flag.
    if (!javaSwitches.containsKey(DISABLE_LOW_END_DEVICE_MODE)) {
      extraCommandLineArgs.add(ENABLE_LOW_END_DEVICE_MODE_SWITCH);
    }

    // Convert the Java switch to a command-line flag so C++ code and non-Activity Java
    // components
    // (such as NetworkStatus) can query
    // CommandLine.getInstance().hasSwitch("use-starboard-lifecycle").
    if (javaSwitches.containsKey(USE_STARBOARD_LIFECYCLE)) {
      extraCommandLineArgs.add("--" + USE_STARBOARD_LIFECYCLE_SWITCH);
    }

    // Serialize all Kimono Java switches into a single
    // --cobalt-java-switches=Key1=Val1,Key2=Val2
    // switch so C++ (ApplyJavaSwitches) can parse it into a map and translate to native
    // switches,
    // features, and V8 flags using C++ constants.
    StringJoiner serializedSwitches = new StringJoiner(",");
    for (Map.Entry<String, String> entry : javaSwitches.entrySet()) {
      String key = entry.getKey();
      if (key == null || key.isEmpty()) {
        continue;
      }
      if (FORCE_720P_UI_ON_1GB_DEVICES.equals(key)
          && (!DeviceUtil.is1GbDevice() || !DeviceUtil.isDisplayAtLeast1080p())) {
        continue;
      }
      String val = entry.getValue();
      serializedSwitches.add(key + "=" + (val != null ? val : ""));
    }
    if (serializedSwitches.length() > 0) {
      extraCommandLineArgs.add("--" + COBALT_JAVA_SWITCHES + "=" + serializedSwitches.toString());
    }

    return extraCommandLineArgs;
  }
}
