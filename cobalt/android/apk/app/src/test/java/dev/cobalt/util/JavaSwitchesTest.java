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

import static com.google.common.truth.Truth.assertThat;

import java.util.HashMap;
import java.util.List;
import java.util.Map;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;

/** Tests for JavaSwitches. */
@RunWith(RobolectricTestRunner.class)
public class JavaSwitchesTest {

  @Test
  public void getExtraCommandLineArgs_EmptySwitches() {
    Map<String, String> javaSwitches = new HashMap<>();
    List<String> args = JavaSwitches.getExtraCommandLineArgs(javaSwitches);

    // Default switches when none are provided.
    assertThat(args).contains("--disable-quic");
    assertThat(args).contains("--enable-low-end-device-mode");
    assertThat(args).contains("--disable-rgba-4444-textures");
    assertThat(args).hasSize(3);
  }

  @Test
  public void getExtraCommandLineArgs_AllSwitches() {
    Map<String, String> javaSwitches = new HashMap<>();
    javaSwitches.put(JavaSwitches.ENABLE_QUIC, "true");
    javaSwitches.put(JavaSwitches.DISABLE_LOW_END_DEVICE_MODE, "true");
    javaSwitches.put(JavaSwitches.V8_JITLESS, "true");
    javaSwitches.put(JavaSwitches.V8_WRITE_PROTECT_CODE_MEMORY, "true");
    javaSwitches.put(JavaSwitches.V8_GC_INTERVAL, "1000");
    javaSwitches.put(JavaSwitches.V8_INITIAL_OLD_SPACE_SIZE, "128");
    javaSwitches.put(JavaSwitches.V8_MAX_OLD_SPACE_SIZE, "256");
    javaSwitches.put(JavaSwitches.V8_MAX_SEMI_SPACE_SIZE, "16");
    javaSwitches.put(JavaSwitches.CC_LAYER_TREE_OPTIMIZATION, "0");
    javaSwitches.put(JavaSwitches.DISABLE_SPLASH_SCREEN, "true");
    javaSwitches.put(JavaSwitches.FORCE_IMAGE_SPLASH_SCREEN, "true");

    List<String> args = JavaSwitches.getExtraCommandLineArgs(javaSwitches);

    assertThat(args).doesNotContain("--disable-quic");
    assertThat(args).doesNotContain("--enable-low-end-device-mode");
    assertThat(args).doesNotContain("--disable-rgba-4444-textures");

    assertThat(args).contains("--cc-layer-tree-optimization=0");

    assertThat(args).contains("--js-flags=--jitless");
    assertThat(args).contains("--js-flags=--write-protect-code-memory");
    assertThat(args).contains("--js-flags=--gc-interval=1000");
    assertThat(args).contains("--js-flags=--initial-old-space-size=128");
    assertThat(args).contains("--js-flags=--max-old-space-size=256");
    assertThat(args).contains("--js-flags=--max-semi-space-size=16");
    assertThat(args).contains("--disable-splash-screen");
    assertThat(args).contains("--force-image-splash-screen");
    assertThat(args).hasSize(9);
  }

  @Test
  public void getExtraCommandLineArgs_LowEndDeviceModeNoSimulatedMemory() {
    Map<String, String> javaSwitches = new HashMap<>();
    javaSwitches.put(JavaSwitches.ENABLE_LOW_END_DEVICE_MODE_NO_SIMULATED_MEMORY, "true");

    List<String> args = JavaSwitches.getExtraCommandLineArgs(javaSwitches);

    assertThat(args).contains("--enable-low-end-device-mode");
    assertThat(args).contains("--enable-low-end-device-mode-no-simulated-memory");
  }

  @Test
  public void getExtraCommandLineArgs_SanitizeValues() {
    Map<String, String> javaSwitches = new HashMap<>();
    javaSwitches.put(JavaSwitches.V8_GC_INTERVAL, "1,000ms");
    javaSwitches.put(JavaSwitches.V8_INITIAL_OLD_SPACE_SIZE, "128 MiB");

    List<String> args = JavaSwitches.getExtraCommandLineArgs(javaSwitches);

    assertThat(args).contains("--js-flags=--gc-interval=1000");
    assertThat(args).contains("--js-flags=--initial-old-space-size=128");
  }
}
