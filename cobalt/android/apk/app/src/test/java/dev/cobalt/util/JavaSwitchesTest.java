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
    DeviceUtil.resetForTesting();
  }

  @After
  public void tearDown() {
    JavaSwitches.setOverrideForTesting(null);
    clearConfigFiles();
    DeviceUtil.resetForTesting();
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

  @Test
  public void testGetExtraCommandLineArgs_NullSwitches() {
    List<String> args = JavaSwitches.getExtraCommandLineArgs(null);
    assertThat(args).containsExactly("--enable-low-end-device-mode");
  }

  @Test
  public void testGetExtraCommandLineArgs_EmptySwitches() {
    List<String> args = JavaSwitches.getExtraCommandLineArgs(new HashMap<>());
    assertThat(args).containsExactly("--enable-low-end-device-mode");
  }

  @Test
  public void testGetExtraCommandLineArgs_LowEndDeviceMode_EnabledByDefault() {
    List<String> args = JavaSwitches.getExtraCommandLineArgs(new HashMap<>());
    assertThat(args).contains("--enable-low-end-device-mode");
  }

  @Test
  public void testGetExtraCommandLineArgs_LowEndDeviceMode_NotForcedByExperiment() {
    Map<String, String> switches = new HashMap<>();
    switches.put(JavaSwitches.DISABLE_LOW_END_DEVICE_MODE, "1");

    List<String> args = JavaSwitches.getExtraCommandLineArgs(switches);

    // No --enable-low-end-device-mode switch is emitted, so base::SysInfo::IsLowEndDevice()
    // falls
    // back to the physical memory threshold. Devices at or below it stay low-end.
    assertThat(args).doesNotContain("--enable-low-end-device-mode");
    assertThat(args).contains("--cobalt-java-switches=DisableLowEndDeviceMode=1");
  }

  @Test
  public void testGetExtraCommandLineArgs_UseStarboardLifecycle_EmitsEarlySwitch() {
    Map<String, String> switches = new HashMap<>();
    switches.put(JavaSwitches.USE_STARBOARD_LIFECYCLE, "1");

    List<String> args = JavaSwitches.getExtraCommandLineArgs(switches);

    assertThat(args).contains("--use-starboard-lifecycle");
    assertThat(args).contains("--cobalt-java-switches=UseStarboardLifeCycle=1");
  }

  @Test
  public void testGetExtraCommandLineArgs_ExperimentsAllowed_SerializesSwitchesIntoSingleFlag() {
    Map<String, String> switches = new HashMap<>();
    switches.put(JavaSwitches.ENABLE_QUIC, "1");
    switches.put(JavaSwitches.USE_MINOR_MS_FOR_MINOR_GC, "1");
    switches.put(JavaSwitches.V8_SET_BYTECODE_OLD_TIME, "10");
    switches.put(JavaSwitches.V8_INITIAL_OLD_SPACE_SIZE, "128");
    switches.put(JavaSwitches.V8_DISABLE_SPARKPLUG, "1");
    switches.put(JavaSwitches.V8_MAX_OLD_SPACE_SIZE, "1024");
    switches.put(JavaSwitches.FORCE_GPU_MEM_AVAILABLE_MB, "256");

    JavaSwitches.setOverrideForTesting(true);
    List<String> args = JavaSwitches.getExtraCommandLineArgs(switches);

    String serializedArg = null;
    for (String arg : args) {
      if (arg.startsWith("--cobalt-java-switches=")) {
        serializedArg = arg;
        break;
      }
    }
    assertThat(serializedArg).isNotNull();
    assertThat(serializedArg).contains("EnableQUIC=1");
    assertThat(serializedArg).contains("UseMinorMSForMinorGC=1");
    assertThat(serializedArg).contains("V8SetBytecodeOldTime=10");
    assertThat(serializedArg).contains("V8InitialOldSpaceSize=128");
    assertThat(serializedArg).contains("V8DisableSparkplug=1");
    assertThat(serializedArg).contains("V8MaxOldSpaceSize=1024");
    assertThat(serializedArg).contains("ForceGpuMemAvailableMb=256");
  }

  @Test
  public void testGetExtraCommandLineArgs_ExperimentsNotAllowed_OmitsSerializedJavaSwitches() {
    Map<String, String> switches = new HashMap<>();
    switches.put(JavaSwitches.ENABLE_QUIC, "1");
    switches.put(JavaSwitches.V8_MAX_OLD_SPACE_SIZE, "1024");
    switches.put(JavaSwitches.USE_STARBOARD_LIFECYCLE, "1");

    JavaSwitches.setOverrideForTesting(false);
    List<String> args = JavaSwitches.getExtraCommandLineArgs(switches);

    assertThat(args).containsExactly("--enable-low-end-device-mode");
    for (String arg : args) {
      assertThat(arg).doesNotContain("cobalt-java-switches");
    }
  }

  @Test
  public void testGetDefaultCommandLineArgs() {
    List<String> args = JavaSwitches.getDefaultCommandLineArgs();
    // Safe mode / no experiment config must not silently flip low-end mode off.
    assertThat(args).containsExactly("--enable-low-end-device-mode");
  }

  @Test
  public void testGetExtraCommandLineArgs_NullValuesInMap() {
    Map<String, String> switches = new HashMap<>();
    switches.put(JavaSwitches.V8_INITIAL_OLD_SPACE_SIZE, null);

    JavaSwitches.setOverrideForTesting(true);
    List<String> args = JavaSwitches.getExtraCommandLineArgs(switches);

    assertThat(args).contains("--cobalt-java-switches=V8InitialOldSpaceSize=");
  }

  @Test
  public void testGetExtraCommandLineArgs_Force720pUiOn1GbDevices_1GbAnd1080p() {
    Map<String, String> switches = new HashMap<>();
    switches.put(JavaSwitches.FORCE_720P_UI_ON_1GB_DEVICES, "1");

    DeviceUtil.setIs1GbDeviceForTesting(true);
    DeviceUtil.setIsDisplayAtLeast1080pForTesting(true);

    List<String> args = JavaSwitches.getExtraCommandLineArgs(switches);
    assertThat(args).contains("--cobalt-java-switches=Force720pUiOn1GbDevices=1");
  }

  @Test
  public void testGetExtraCommandLineArgs_Force720pUiOn1GbDevices_1GbAnd720p() {
    Map<String, String> switches = new HashMap<>();
    switches.put(JavaSwitches.FORCE_720P_UI_ON_1GB_DEVICES, "1");

    DeviceUtil.setIs1GbDeviceForTesting(true);
    DeviceUtil.setIsDisplayAtLeast1080pForTesting(false);

    List<String> args = JavaSwitches.getExtraCommandLineArgs(switches);
    for (String arg : args) {
      assertThat(arg).doesNotContain("Force720pUiOn1GbDevices");
    }
  }

  @Test
  public void testGetExtraCommandLineArgs_Force720pUiOn1GbDevices_2GbAnd1080p() {
    Map<String, String> switches = new HashMap<>();
    switches.put(JavaSwitches.FORCE_720P_UI_ON_1GB_DEVICES, "1");

    DeviceUtil.setIs1GbDeviceForTesting(false);
    DeviceUtil.setIsDisplayAtLeast1080pForTesting(true);

    List<String> args = JavaSwitches.getExtraCommandLineArgs(switches);
    for (String arg : args) {
      assertThat(arg).doesNotContain("Force720pUiOn1GbDevices");
    }
  }
}
