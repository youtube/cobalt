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

package dev.cobalt.util;

import static com.google.common.truth.Truth.assertThat;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import org.chromium.base.ContextUtils;
import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.RuntimeEnvironment;

/** Unit tests for {@link JavaSwitches}. */
@RunWith(RobolectricTestRunner.class)
public class JavaSwitchesTest {

  @Before
  public void setUp() {
    ContextUtils.initApplicationContextForTests(RuntimeEnvironment.getApplication());
    clearConfigFiles();
    JavaSwitches.setOverrideForTesting(null);
    JavaSwitches.takeNativeKeyValues();
  }

  @After
  public void tearDown() {
    JavaSwitches.setOverrideForTesting(null);
    DeviceUtil.resetForTesting();
    clearConfigFiles();
  }

  private void clearConfigFiles() {
    if (ContextUtils.getApplicationContext() != null) {
      File cacheDir = ContextUtils.getApplicationContext().getCacheDir();
      if (cacheDir != null) {
        new File(cacheDir, CobaltPrefNames.VARIATIONS_BEACON_FILENAME).delete();
        new File(cacheDir, CobaltPrefNames.METRICS_CONFIG_FILENAME).delete();
        new File(cacheDir, CobaltPrefNames.EXPERIMENT_CONFIG_FILENAME).delete();
      }
    }
  }

  private void writeVariationsBeacon(int crashStreak, boolean exitedCleanly) throws IOException {
    File cacheDir = ContextUtils.getApplicationContext().getCacheDir();
    File file = new File(cacheDir, CobaltPrefNames.VARIATIONS_BEACON_FILENAME);
    String json =
        "{\""
            + CobaltPrefNames.VARIATIONS_CRASH_STREAK
            + "\":"
            + crashStreak
            + ",\""
            + CobaltPrefNames.STABILITY_EXITED_CLEANLY
            + "\":"
            + exitedCleanly
            + "}";
    try (FileOutputStream fos = new FileOutputStream(file)) {
      fos.write(json.getBytes(StandardCharsets.UTF_8));
    }
  }

  private void writeMetricsConfig(int crashStreak) throws IOException {
    File cacheDir = ContextUtils.getApplicationContext().getCacheDir();
    File file = new File(cacheDir, CobaltPrefNames.METRICS_CONFIG_FILENAME);
    String json = "{\"" + CobaltPrefNames.VARIATIONS_CRASH_STREAK + "\":" + crashStreak + "}";
    try (FileOutputStream fos = new FileOutputStream(file)) {
      fos.write(json.getBytes(StandardCharsets.UTF_8));
    }
  }

  private void writeExperimentConfigWithThreshold(int threshold) throws IOException {
    File cacheDir = ContextUtils.getApplicationContext().getCacheDir();
    File file = new File(cacheDir, CobaltPrefNames.EXPERIMENT_CONFIG_FILENAME);
    String json =
        "{\""
            + CobaltExperimentNames.FINCH_PARAMETERS
            + "\":{\""
            + CobaltExperimentNames.CRASH_STREAK_EMPTY_CONFIG_THRESHOLD
            + "\":"
            + threshold
            + "}}";
    try (FileOutputStream fos = new FileOutputStream(file)) {
      fos.write(json.getBytes(StandardCharsets.UTF_8));
    }
  }

  @Test
  public void testShouldApplyExperimentConfigs_DefaultWhenNotInitialized() {
    // When override is not set and no config files exist, defaults to true.
    assertThat(JavaSwitches.shouldApplyExperimentConfigs()).isTrue();
  }

  @Test
  public void testShouldApplyExperimentConfigs_LowCrashStreak() throws IOException {
    writeMetricsConfig(1);
    assertThat(JavaSwitches.shouldApplyExperimentConfigs()).isTrue();
  }

  @Test
  public void testShouldApplyExperimentConfigs_HighCrashStreak_DefaultThreshold()
      throws IOException {
    writeMetricsConfig(CobaltCrashStreakThreshold.DEFAULT_CRASH_STREAK_EMPTY_CONFIG_THRESHOLD - 1);
    assertThat(JavaSwitches.shouldApplyExperimentConfigs()).isTrue();

    writeMetricsConfig(CobaltCrashStreakThreshold.DEFAULT_CRASH_STREAK_EMPTY_CONFIG_THRESHOLD);
    assertThat(JavaSwitches.shouldApplyExperimentConfigs()).isFalse();

    writeMetricsConfig(CobaltCrashStreakThreshold.DEFAULT_CRASH_STREAK_EMPTY_CONFIG_THRESHOLD + 1);
    assertThat(JavaSwitches.shouldApplyExperimentConfigs()).isFalse();
  }

  @Test
  public void testShouldApplyExperimentConfigs_CustomThresholdFromExperimentConfig()
      throws IOException {
    // Custom threshold is 2. Crash streak 2 triggers safe mode.
    writeExperimentConfigWithThreshold(2);
    writeMetricsConfig(1);
    assertThat(JavaSwitches.shouldApplyExperimentConfigs()).isTrue();

    writeMetricsConfig(2);
    assertThat(JavaSwitches.shouldApplyExperimentConfigs()).isFalse();
  }

  @Test
  public void testShouldApplyExperimentConfigs_CustomThresholdHigher() throws IOException {
    // Custom threshold is 5. Crash streak 3 does not trigger safe mode.
    writeExperimentConfigWithThreshold(5);
    writeMetricsConfig(3);
    assertThat(JavaSwitches.shouldApplyExperimentConfigs()).isTrue();

    writeMetricsConfig(5);
    assertThat(JavaSwitches.shouldApplyExperimentConfigs()).isFalse();
  }

  @Test
  public void testShouldApplyExperimentConfigs_VariationsBeacon_CleanExit_BelowThreshold()
      throws IOException {
    // Stored streak 3, clean exit -> effective streak 3 < threshold 4 -> true.
    writeVariationsBeacon(3, true);
    assertThat(JavaSwitches.shouldApplyExperimentConfigs()).isTrue();
  }

  @Test
  public void testShouldApplyExperimentConfigs_VariationsBeacon_DirtyExit_ReachesThreshold()
      throws IOException {
    // Stored streak 3, dirty exit (crash) -> pending increment 3 + 1 = 4 >= threshold 4 -> false.
    writeVariationsBeacon(3, false);
    assertThat(JavaSwitches.shouldApplyExperimentConfigs()).isFalse();
  }

  @Test
  public void testShouldApplyExperimentConfigs_VariationsBeacon_DirtyExit_BelowThreshold()
      throws IOException {
    // Stored streak 2, dirty exit (crash) -> pending increment 2 + 1 = 3 < threshold 4 -> true.
    writeVariationsBeacon(2, false);
    assertThat(JavaSwitches.shouldApplyExperimentConfigs()).isTrue();
  }

  @Test
  public void testShouldApplyExperimentConfigs_VariationsBeaconTakesPrecedenceOverMetricsConfig()
      throws IOException {
    // Metrics Config says 0, but Variations beacon says 4 (threshold reached).
    writeMetricsConfig(0);
    writeVariationsBeacon(4, true);
    assertThat(JavaSwitches.shouldApplyExperimentConfigs()).isFalse();
  }

  @Test
  public void testShouldApplyExperimentConfigs_MalformedJson() throws IOException {
    File cacheDir = ContextUtils.getApplicationContext().getCacheDir();
    File file = new File(cacheDir, CobaltPrefNames.METRICS_CONFIG_FILENAME);
    try (FileOutputStream fos = new FileOutputStream(file)) {
      fos.write("invalid json content".getBytes(StandardCharsets.UTF_8));
    }
    assertThat(JavaSwitches.shouldApplyExperimentConfigs()).isTrue();
  }

  @Test
  public void testShouldApplyExperimentConfigs_WithOverride() {
    JavaSwitches.setOverrideForTesting(true);
    assertThat(JavaSwitches.shouldApplyExperimentConfigs()).isTrue();

    JavaSwitches.setOverrideForTesting(false);
    assertThat(JavaSwitches.shouldApplyExperimentConfigs()).isFalse();
  }

  private static final String LOW_END = "--enable-low-end-device-mode";
  private static final String DEFAULT_SCALE = "--force-device-scale-factor=1";
  private static final String UI_720P_SCALE = "--force-device-scale-factor=1.5";

  private static List<String> apply(Map<String, String> switches, boolean shouldApply) {
    List<String> args = new ArrayList<>();
    JavaSwitches.apply(switches, shouldApply, args);
    return args;
  }

  @Test
  public void testApply_NullSwitches() {
    assertThat(apply(null, true)).containsExactly(LOW_END, DEFAULT_SCALE).inOrder();
    assertThat(JavaSwitches.takeNativeKeyValues()).isEmpty();
  }

  @Test
  public void testApply_EmptySwitches() {
    assertThat(apply(new HashMap<>(), true)).containsExactly(LOW_END, DEFAULT_SCALE).inOrder();
    assertThat(JavaSwitches.takeNativeKeyValues()).isEmpty();
  }

  @Test
  public void testApply_NativeSwitchesNotOnCommandLine() {
    Map<String, String> switches = new HashMap<>();
    switches.put(AndroidTvFlags.V8_MAX_OLD_SPACE_SIZE, "1024");
    switches.put(AndroidTvFlags.ENABLE_QUIC, "");

    // Switches expanded in C++ are handed over via takeNativeKeyValues(), not the command line.
    assertThat(apply(switches, true)).containsExactly(LOW_END, DEFAULT_SCALE).inOrder();
  }

  @Test
  public void testApply_LowEndDeviceMode_NotForcedByExperiment() {
    Map<String, String> switches = new HashMap<>();
    switches.put(JavaSwitches.DISABLE_LOW_END_DEVICE_MODE, "1");

    List<String> args = apply(switches, true);

    // No low-end switch is emitted, so base::SysInfo::IsLowEndDevice() falls back to the
    // physical memory threshold. Devices at or below it stay low-end.
    assertThat(args).containsExactly(DEFAULT_SCALE);
  }

  @Test
  public void testApply_LowEndDeviceMode_SafeModeKeepsLowEnd() {
    Map<String, String> switches = new HashMap<>();
    switches.put(JavaSwitches.DISABLE_LOW_END_DEVICE_MODE, "1");

    // Safe mode / no experiment config must not silently flip low-end mode off.
    assertThat(apply(switches, false)).containsExactly(LOW_END, DEFAULT_SCALE).inOrder();
  }

  @Test
  public void testApply_AppendsToExistingArgs() {
    List<String> args = new ArrayList<>();
    args.add("--enable-features=MetadataFeature");
    JavaSwitches.apply(new HashMap<>(), true, args);
    assertThat(args)
        .containsExactly("--enable-features=MetadataFeature", LOW_END, DEFAULT_SCALE)
        .inOrder();
  }

  @Test
  public void testTakeNativeKeyValues_SortedKeysAndNumericValues() {
    Map<String, String> switches = new HashMap<>();
    switches.put(AndroidTvFlags.V8_MAX_OLD_SPACE_SIZE, "1024");
    switches.put(AndroidTvFlags.ENABLE_QUIC, "");
    switches.put(AndroidTvFlags.AVOID_CC_REUSE_RESOURCE, "true");

    apply(switches, true);

    assertThat(JavaSwitches.takeNativeKeyValues())
        .asList()
        .containsExactly("AvoidCCReuseResource", "", "EnableQUIC", "", "V8MaxOldSpaceSize", "1024")
        .inOrder();
  }

  @Test
  public void testTakeNativeKeyValues_TakeClears() {
    Map<String, String> switches = new HashMap<>();
    switches.put(AndroidTvFlags.ENABLE_QUIC, "1");
    apply(switches, true);

    assertThat(JavaSwitches.takeNativeKeyValues()).hasLength(2);
    assertThat(JavaSwitches.takeNativeKeyValues()).isEmpty();
  }

  @Test
  public void testTakeNativeKeyValues_ExperimentsNotAllowed_Empty() {
    Map<String, String> switches = new HashMap<>();
    switches.put(AndroidTvFlags.ENABLE_QUIC, "1");
    switches.put(AndroidTvFlags.V8_MAX_OLD_SPACE_SIZE, "1024");

    apply(switches, false);

    assertThat(JavaSwitches.takeNativeKeyValues()).isEmpty();
  }

  @Test
  public void testTakeNativeKeyValues_NeverSet_Empty() {
    JavaSwitches.takeNativeKeyValues();
    assertThat(JavaSwitches.takeNativeKeyValues()).isEmpty();
  }

  @Test
  public void testToNativeKeyValues_SanitizesValues() {
    Map<String, String> switches = new HashMap<>();
    switches.put(AndroidTvFlags.V8_INITIAL_OLD_SPACE_SIZE, "invalid_non_numeric");
    switches.put(AndroidTvFlags.FORCE_GPU_MEM_AVAILABLE_MB, null);
    switches.put(AndroidTvFlags.GPU_IMAGE_CACHE_LIMIT_ITEMS, "5,0=0");

    assertThat(JavaSwitches.toNativeKeyValues(switches))
        .asList()
        .containsExactly(
            "ForceGpuMemAvailableMb", "",
            "GpuImageCacheLimitItems", "500",
            "V8InitialOldSpaceSize", "")
        .inOrder();
  }

  @Test
  public void testToNativeKeyValues_DropsNullAndEmptyKeys() {
    Map<String, String> switches = new HashMap<>();
    switches.put(null, "1");
    switches.put("", "1");
    switches.put(AndroidTvFlags.ENABLE_QUIC, "1");

    assertThat(JavaSwitches.toNativeKeyValues(switches))
        .asList()
        .containsExactly("EnableQUIC", "1")
        .inOrder();
  }

  private static List<String> applyForce720pUi(
      boolean hasKey, boolean is1GbDevice, boolean isDisplayAtLeast1080p, boolean shouldApply) {
    DeviceUtil.setIs1GbDeviceForTesting(is1GbDevice);
    DeviceUtil.setIsDisplayAtLeast1080pForTesting(isDisplayAtLeast1080p);
    Map<String, String> switches = new HashMap<>();
    if (hasKey) {
      switches.put(JavaSwitches.FORCE_720P_UI_ON_1GB_DEVICES, "");
    }
    return apply(switches, shouldApply);
  }

  @Test
  public void testApply_Force720pUi_EligibleDevice() {
    assertThat(applyForce720pUi(true, true, true, true))
        .containsExactly(LOW_END, UI_720P_SCALE)
        .inOrder();
  }

  @Test
  public void testApply_Force720pUi_IneligibleDevices() {
    assertThat(applyForce720pUi(true, true, false, true)).contains(DEFAULT_SCALE); // 720p display.
    assertThat(applyForce720pUi(true, false, true, true)).contains(DEFAULT_SCALE); // > 1 GB RAM.
    assertThat(applyForce720pUi(true, false, false, true)).contains(DEFAULT_SCALE); // Neither.
  }

  @Test
  public void testApply_Force720pUi_NotInExperiment() {
    assertThat(applyForce720pUi(false, true, true, true)).contains(DEFAULT_SCALE);
  }

  @Test
  public void testApply_Force720pUi_SafeModeUsesDefault() {
    assertThat(applyForce720pUi(true, true, true, false)).contains(DEFAULT_SCALE);
  }
}
