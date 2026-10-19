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

import androidx.annotation.VisibleForTesting;
import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.InputStreamReader;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Map;
import org.chromium.base.BaseSwitches;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.json.JSONObject;

/** Defines the constant names for feature switches used in Kimono. */
@JNINamespace("cobalt")
public class JavaSwitches {
  private static final String TAG = "JavaSwitches";

  // TODO(b/532158865): Remove once AOSP MainActivity stops using it (go/copr/13468).
  // Android TV no longer reads this; its --disable-quic default is applied in C++
  // (cobalt/android/androidtv_flag_map.cc).
  public static final String DEFAULT_DISABLE_QUIC = "--disable-quic";

  // Android TV experiment keys live in the layer that consumes them. Keys consumed in C++ are
  // defined in cobalt/android/androidtv_flags.h and expanded in C++
  // (cobalt/android/androidtv_flag_map.cc). Only the keys below are handled in Java, for one of
  // these reasons:
  // - Consumed only in Java.
  // - Consumed in C++, but must be applied in Java because C++ reads it before the native
  //   initialization hook (ExpandAndroidTvFlags()) runs.
  // When an experiment concludes, its default belongs in the same layer as its key.

  /**
   * Opts out of --enable-low-end-device-mode.
   *
   * <p>Defined in Java to meet a timing requirement: the switch is read before the native
   * initialization hook runs, by Java SysUtils.isLowEndDevice(), which caches the result, and by
   * base::SysInfo::IsLowEndDevice() in JNI_LibraryLoader_LibraryLoaded(). It is therefore applied
   * on the Java CommandLine before libcobalt.so loads (see addSwitchesReadBeforeNativeInit()).
   */
  public static final String DISABLE_LOW_END_DEVICE_MODE = "DisableLowEndDeviceMode";

  /**
   * Forces a 720p UI (--force-device-scale-factor=1.5) when the device has 1 GB of RAM or less and
   * a display of at least 1080p. See b/558324832.
   *
   * <p>Defined in Java to meet a timing requirement: --force-device-scale-factor is read in Java
   * (PhysicalDisplayAndroid, e.g. via DeviceUtils.updateDeviceSpecificUserAgentSwitch()) before
   * libcobalt.so loads, and the value is cached for the process. It is therefore applied on the
   * Java CommandLine before libcobalt.so loads (see addSwitchesReadBeforeNativeInit()).
   */
  public static final String FORCE_720P_UI_ON_1GB_DEVICES = "Force720pUiOn1GbDevices";

  @VisibleForTesting static final String DEFAULT_DEVICE_SCALE_FACTOR = "1";
  @VisibleForTesting static final String FORCE_720P_UI_DEVICE_SCALE_FACTOR = "1.5";

  /**
   * Disables the StartupGuard watchdog.
   *
   * <p>Defined in Java because it is consumed only in Java (CobaltActivity.setupStartupGuard()).
   */
  public static final String DISABLE_STARTUP_GUARD = "DisableStartupGuard";

  /**
   * StartupGuard watchdog timeout in seconds.
   *
   * <p>Defined in Java because it is consumed only in Java (CobaltActivity.setupStartupGuard()).
   */
  public static final String STARTUP_GUARD_INTERVAL_IN_SECONDS = "StartupGuardIntervalInSeconds";

  /**
   * Enables Starboard lifecycle migration.
   *
   * <p>Consumed in both layers: C++ expands it into USE_STARBOARD_LIFECYCLE_SWITCH, and Java
   * (CobaltActivity) also reads the key directly. The key itself is defined in
   * cobalt/android/androidtv_flags.h and aliased here for Java callers.
   */
  public static final String USE_STARBOARD_LIFECYCLE = AndroidTvFlags.USE_STARBOARD_LIFECYCLE;

  /** Command-line switch set by C++ when USE_STARBOARD_LIFECYCLE is enabled. */
  public static final String USE_STARBOARD_LIFECYCLE_SWITCH =
      CobaltSwitches.USE_STARBOARD_LIFECYCLE;

  private static Boolean sOverrideForTesting;
  private static String[] sNativeKeyValues;

  public static void setOverrideForTesting(Boolean override) {
    sOverrideForTesting = override;
  }

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

  /**
   * Applies pre-native switches to {@code args} and stashes the sanitized map for C++ expansion
   * ({@code cobalt/android/androidtv_flag_map.cc}). In crash-streak safe mode, no switches are
   * applied.
   */
  public static void apply(Map<String, String> javaSwitches, List<String> args) {
    apply(javaSwitches, shouldApplyExperimentConfigs(), args);
  }

  @VisibleForTesting
  static void apply(
      Map<String, String> javaSwitches, boolean shouldApplyExperimentConfigs, List<String> args) {
    if (!shouldApplyExperimentConfigs || javaSwitches == null) {
      javaSwitches = Collections.emptyMap();
    }
    addSwitchesReadBeforeNativeInit(args, javaSwitches);
    sNativeKeyValues = toNativeKeyValues(javaSwitches);
  }

  /** Returns and clears the sanitized [key0, value0, ...] array for C++ initialization. */
  @CalledByNative
  static String[] takeNativeKeyValues() {
    String[] keyValues = sNativeKeyValues == null ? new String[0] : sNativeKeyValues;
    sNativeKeyValues = null;
    return keyValues;
  }

  private static void addSwitchesReadBeforeNativeInit(
      List<String> args, Map<String, String> javaSwitches) {
    // Read by Java SysUtils.isLowEndDevice(), which caches the result, and by
    // SysInfo::IsLowEndDevice() in JNI_LibraryLoader_LibraryLoaded() (orderfile optimization).
    // TODO(cobalt, b/563373348): Investigate performance impact on high-end devices.
    if (!javaSwitches.containsKey(DISABLE_LOW_END_DEVICE_MODE)) {
      args.add("--" + BaseSwitches.ENABLE_LOW_END_DEVICE_MODE);
    }

    // Read and cached by PhysicalDisplayAndroid, via
    // DeviceUtils.updateDeviceSpecificUserAgentSwitch().
    boolean force720pUi =
        javaSwitches.containsKey(FORCE_720P_UI_ON_1GB_DEVICES)
            && DeviceUtil.is1GbDevice()
            && DeviceUtil.isDisplayAtLeast1080p();
    args.add(
        "--"
            + DisplaySwitches.FORCE_DEVICE_SCALE_FACTOR
            + "="
            + (force720pUi ? FORCE_720P_UI_DEVICE_SCALE_FACTOR : DEFAULT_DEVICE_SCALE_FACTOR));
  }

  /**
   * Converts the Java switches into a flat {@code [key0, value0, ...]} array sorted by key. Values
   * are reduced to their digits, since every value-carrying switch is numeric; a value without
   * digits becomes an empty string.
   */
  @VisibleForTesting
  static String[] toNativeKeyValues(Map<String, String> javaSwitches) {
    if (javaSwitches == null || javaSwitches.isEmpty()) {
      return new String[0];
    }
    List<String> keys = new ArrayList<>();
    for (String key : javaSwitches.keySet()) {
      if (key != null && !key.isEmpty()) {
        keys.add(key);
      }
    }
    Collections.sort(keys);

    List<String> keyValues = new ArrayList<>();
    for (String key : keys) {
      String value = javaSwitches.get(key);
      keyValues.add(key);
      keyValues.add(value == null ? "" : value.replaceAll("[^0-9]", ""));
    }
    return keyValues.toArray(new String[0]);
  }
}
