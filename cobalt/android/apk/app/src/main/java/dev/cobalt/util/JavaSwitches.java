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
<<<<<<< HEAD
import java.util.StringJoiner;
=======
import org.chromium.base.BaseSwitches;
>>>>>>> 088a1fa3c3 (cobalt: Move experiments using C++ flags into C++ layer (#13330))
import org.chromium.base.ContextUtils;
import org.chromium.base.DeviceInfo;
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

<<<<<<< HEAD
  public static List<String> getDefaultCommandLineArgs() {
    List<String> defaultArgs = new ArrayList<>();
    defaultArgs.add(DEFAULT_DISABLE_QUIC);
    defaultArgs.add(ENABLE_LOW_END_DEVICE_MODE_SWITCH);
    if (!"arm64".equals(DeviceInfo.getArch()) && !"x86_64".equals(DeviceInfo.getArch())) {
      defaultArgs.add("--force-gpu-mem-available-mb=" + DEFAULT_FORCE_GPU_MEM_AVAILABLE_MB);
    }
    defaultArgs.add(
        "--js-flags=--initial-old-space-size="
            + DEFAULT_INITIAL_OLD_SPACE_SIZE
            + ";--max-old-space-size="
            + DEFAULT_MAX_OLD_SPACE_SIZE);
    defaultArgs.add("--force-device-scale-factor=" + DEFAULT_FORCE_DEVICE_SCALE_FACTOR);
    return defaultArgs;
=======
  /**
   * Applies pre-native switches to {@code args} and stashes the sanitized map for C++ expansion
   * ({@code cobalt/android/androidtv_flag_map.cc}). In crash-streak safe mode, no switches are
   * applied.
   */
  public static void apply(Map<String, String> javaSwitches, List<String> args) {
    apply(javaSwitches, shouldApplyExperimentConfigs(), args);
>>>>>>> 088a1fa3c3 (cobalt: Move experiments using C++ flags into C++ layer (#13330))
  }

  @VisibleForTesting
  static void apply(
      Map<String, String> javaSwitches, boolean shouldApplyExperimentConfigs, List<String> args) {
    if (!shouldApplyExperimentConfigs || javaSwitches == null) {
      javaSwitches = Collections.emptyMap();
    }
<<<<<<< HEAD

    List<String> extraCommandLineArgs = new ArrayList<>();
    StringJoiner jsFlags = new StringJoiner(";");

    if (!javaSwitches.containsKey(JavaSwitches.ENABLE_QUIC)) {
      extraCommandLineArgs.add(DEFAULT_DISABLE_QUIC);
    }

    // TODO(cobalt, b/563373348): Investigate performance impact on high-end devices. Use Java
    // switch due to IsLowEndDevice called before Finch is initialized. We should migrate to Finch
    // if high-end device benefits from removing this low-end-device-mode flag.
    if (!javaSwitches.containsKey(JavaSwitches.DISABLE_LOW_END_DEVICE_MODE)) {
      extraCommandLineArgs.add(ENABLE_LOW_END_DEVICE_MODE_SWITCH);
    }

    if (javaSwitches.containsKey(JavaSwitches.USE_MINOR_MS_FOR_MINOR_GC)) {
      jsFlags.add("--minor-ms");
      jsFlags.add("--minor-ms-min-new-space-capacity-for-concurrent-marking-mb=0");
    }

    String oldTime = getSanitizedNumericValue(javaSwitches, JavaSwitches.V8_SET_BYTECODE_OLD_TIME);
    if (oldTime != null) {
      jsFlags.add("--flush-bytecode");
      jsFlags.add("--bytecode-old-time=" + oldTime);
    }

    String initialOldSpace =
        getSanitizedNumericValue(javaSwitches, JavaSwitches.V8_INITIAL_OLD_SPACE_SIZE);
    if (initialOldSpace != null) {
      jsFlags.add("--initial-old-space-size=" + initialOldSpace);
    } else {
      jsFlags.add("--initial-old-space-size=" + DEFAULT_INITIAL_OLD_SPACE_SIZE);
    }

    if (javaSwitches.containsKey(JavaSwitches.V8_DISABLE_SPARKPLUG)) {
      jsFlags.add("--no-sparkplug");
    }

    String maxOldSpace = getSanitizedNumericValue(javaSwitches, JavaSwitches.V8_MAX_OLD_SPACE_SIZE);
    if (maxOldSpace != null) {
      jsFlags.add("--max-old-space-size=" + maxOldSpace);
    } else {
      jsFlags.add("--max-old-space-size=" + DEFAULT_MAX_OLD_SPACE_SIZE);
    }

    String forceGpuMem =
        getSanitizedNumericValue(javaSwitches, JavaSwitches.FORCE_GPU_MEM_AVAILABLE_MB);
    if (forceGpuMem != null) {
      extraCommandLineArgs.add("--force-gpu-mem-available-mb=" + forceGpuMem);
    } else if (!"arm64".equals(DeviceInfo.getArch()) && !"x86_64".equals(DeviceInfo.getArch())) {
      extraCommandLineArgs.add(
          "--force-gpu-mem-available-mb=" + DEFAULT_FORCE_GPU_MEM_AVAILABLE_MB);
    }

    if (javaSwitches.containsKey(JavaSwitches.DISABLE_GPU_MEMORY_BUFFER_COMPOSITOR_RESOURCES)) {
      extraCommandLineArgs.add("--disable-gpu-memory-buffer-compositor-resources");
    }

    String limit = getSanitizedNumericValue(javaSwitches, JavaSwitches.GPU_IMAGE_CACHE_LIMIT_ITEMS);
    if (limit != null) {
      extraCommandLineArgs.add("--cc-image-cache-limit-items=" + limit);
    }

    String decodeLimit =
        getSanitizedNumericValue(javaSwitches, JavaSwitches.LIMIT_IMAGE_DECODE_CACHE_SIZE_MB);
    if (decodeLimit != null) {
      extraCommandLineArgs.add("--cc-image-cache-limit-mbs=" + decodeLimit);
    }

    String budget =
        getSanitizedNumericValue(javaSwitches, JavaSwitches.DECODED_IMAGE_WORKING_SET_BUDGET_BYTES);
    if (budget != null) {
      extraCommandLineArgs.add("--decoded-image-working-set-budget-bytes=" + budget);
    }

    if (javaSwitches.containsKey(JavaSwitches.ENABLE_SCALING_CLIPPED_IMAGES)) {
      extraCommandLineArgs.add("--enable-scaling-clipped-images");
    }

    StringJoiner mojoPipeParams = new StringJoiner("/");
    String subresourceSize =
        getSanitizedNumericValue(
            javaSwitches, JavaSwitches.COBALT_DYNAMIC_MOJO_PIPE_SUBRESOURCE_SIZE);
    if (subresourceSize != null) {
      mojoPipeParams.add("subresource_size/" + subresourceSize);
    }

    String mediaSize =
        getSanitizedNumericValue(javaSwitches, JavaSwitches.COBALT_DYNAMIC_MOJO_PIPE_MEDIA_SIZE);
    if (mediaSize != null) {
      mojoPipeParams.add("media_size/" + mediaSize);
    }

    if (javaSwitches.containsKey(JavaSwitches.ENABLE_COBALT_DYNAMIC_MOJO_PIPE_SIZING)
        || mojoPipeParams.length() > 0) {
      if (mojoPipeParams.length() > 0) {
        extraCommandLineArgs.add(
            "--enable-features=CobaltDynamicMojoPipeSizing:" + mojoPipeParams.toString());
      } else {
        extraCommandLineArgs.add("--enable-features=CobaltDynamicMojoPipeSizing");
      }
    }

    if (javaSwitches.containsKey(
        JavaSwitches.ENABLE_COBALT_CONTENT_LENGTH_AWARE_MOJO_PIPE_SIZING)) {
      extraCommandLineArgs.add("--enable-features=CobaltContentLengthAwareMojoPipeSizing");
    }

    StringJoiner featureParams = new StringJoiner("/");
    String interestAreaSize =
        getSanitizedNumericValue(javaSwitches, JavaSwitches.INTEREST_AREA_SIZE_IN_PIXELS);
    if (interestAreaSize != null) {
      featureParams.add("size_in_pixels/" + interestAreaSize);
    }

    String reclaimDelay =
        getSanitizedNumericValue(javaSwitches, JavaSwitches.RECLAIM_DELAY_IN_SECONDS);
    if (reclaimDelay != null) {
      featureParams.add("reclaim_delay_s/" + reclaimDelay);
    }

    if (featureParams.length() > 0) {
      extraCommandLineArgs.add("--enable-features=SmallerInterestArea:" + featureParams.toString());
    }

    if (jsFlags.length() > 0) {
      extraCommandLineArgs.add("--js-flags=" + jsFlags.toString());
    }

    if (javaSwitches.containsKey(JavaSwitches.AVOID_CC_REUSE_RESOURCE)) {
      extraCommandLineArgs.add("--avoid-cc-reuse-resource");
    }

    if (javaSwitches.containsKey(JavaSwitches.COBALT_BYPASS_RESOURCE_LOAD_SCHEDULER)) {
      extraCommandLineArgs.add(
          "--enable-features=" + JavaSwitches.COBALT_BYPASS_RESOURCE_LOAD_SCHEDULER);
    }

    if (javaSwitches.containsKey(JavaSwitches.COBALT_BYPASS_HTML_PRELOAD_SCANNER)) {
      extraCommandLineArgs.add(
          "--enable-features=" + JavaSwitches.COBALT_BYPASS_HTML_PRELOAD_SCANNER);
    }

    if (javaSwitches.containsKey(JavaSwitches.ENABLE_COBALT_MMAP_FONT_CACHE)) {
      extraCommandLineArgs.add("--enable-features=CobaltMmapFontCache");
    }

    if (javaSwitches.containsKey(JavaSwitches.AREA_BASED_VIDEO_BUFFER_BUDGET)) {
      extraCommandLineArgs.add("--enable-features=AreaBasedVideoBufferBudget");
    }

    if (javaSwitches.containsKey(
        JavaSwitches.ALLOW_CRITICAL_MEMORY_PRESSURE_HANDLING_IN_FOREGROUND)) {
      extraCommandLineArgs.add("--allow-critical-memory-pressure-handling-in-foreground");
    }

    if (javaSwitches.containsKey(JavaSwitches.EVICT_MEMORY_CACHE_ON_CRITICAL_MEMORY_PRESSURE)) {
      extraCommandLineArgs.add(
          "--enable-features=" + JavaSwitches.EVICT_MEMORY_CACHE_ON_CRITICAL_MEMORY_PRESSURE);
    }

    if (javaSwitches.containsKey(JavaSwitches.DISABLE_LESS_AGGRESSIVE_PARKABLE_STRING)) {
      extraCommandLineArgs.add("--disable-features=LessAggressiveParkableString");
    }

    List<String> enabledMemoryPressureFeatures = new ArrayList<>();
    if (javaSwitches.containsKey(JavaSwitches.ENABLE_MODERATE_MEMORY_PRESSURE)) {
      enabledMemoryPressureFeatures.add("CobaltEnableModerateMemoryPressure");
    }
    if (javaSwitches.containsKey(JavaSwitches.MEMORY_PRESSURE_COOLDOWN_IN_SECONDS)) {
      String cooldown = javaSwitches.get(JavaSwitches.MEMORY_PRESSURE_COOLDOWN_IN_SECONDS);
      if (cooldown != null) {
        String cooldownVal = cooldown.replaceAll("[^0-9]", "");
        if (!cooldownVal.isEmpty()) {
          enabledMemoryPressureFeatures.add(
              "CobaltMemoryPressureCooldown:cooldown-seconds/" + cooldownVal);
        }
      }
    }
    if (!enabledMemoryPressureFeatures.isEmpty()) {
      extraCommandLineArgs.add(
          "--enable-features=" + String.join(",", enabledMemoryPressureFeatures));
    }

    // Convert the Java switch to a command-line flag so C++ code and non-Activity Java components
    // (such as NetworkStatus) can query
    // CommandLine.getInstance().hasSwitch("use-starboard-lifecycle").
    if (javaSwitches.containsKey(JavaSwitches.USE_STARBOARD_LIFECYCLE)) {
      extraCommandLineArgs.add("--" + USE_STARBOARD_LIFECYCLE_SWITCH);
    }

    if (javaSwitches.containsKey(JavaSwitches.ENABLE_ACTIVITY_LIFECYCLE_COORDINATION)) {
      extraCommandLineArgs.add("--enable-activity-lifecycle-coordination");
    }

    if (javaSwitches.containsKey(JavaSwitches.FORCE_720P_UI_ON_1GB_DEVICES)
        && DeviceUtil.is1GbDevice()
        && DeviceUtil.isDisplayAtLeast1080p()) {
      extraCommandLineArgs.add("--force-device-scale-factor=1.5");
    } else {
      extraCommandLineArgs.add("--force-device-scale-factor=" + DEFAULT_FORCE_DEVICE_SCALE_FACTOR);
    }

    return extraCommandLineArgs;
=======
    addSwitchesReadBeforeNativeInit(args, javaSwitches);
    sNativeKeyValues = toNativeKeyValues(javaSwitches);
>>>>>>> 088a1fa3c3 (cobalt: Move experiments using C++ flags into C++ layer (#13330))
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
