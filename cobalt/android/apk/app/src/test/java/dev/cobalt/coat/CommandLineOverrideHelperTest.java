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

package dev.cobalt.coat;

import static com.google.common.truth.Truth.assertThat;

import java.util.List;
import org.chromium.base.CommandLine;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;

/** CommandLineOverrideHelperTest. */
@RunWith(RobolectricTestRunner.class)
public class CommandLineOverrideHelperTest {
    @Before
    public void setUp() {
        // Create a fresh CommandLine instance for each test.
        CommandLine.init(null);
    }

    @Test
    public void testDefaultCommandLineOverridesList() {
        List<String> overrides = CommandLineOverrideHelper.getDefaultCommandLineOverridesList();
        assertThat(overrides.contains("--enable-low-end-device-mode")).isTrue();
        assertThat(overrides.contains("--force-gpu-mem-available-mb=32")).isTrue();
    }

    @Test
    public void testDefaultJsFlagOverridesList() {
        String overrides = CommandLineOverrideHelper.getDefaultJsFlagOverridesList().toString();
        assertThat(overrides.contains("--optimize_for_size=true")).isTrue();
        assertThat(overrides.contains("--concurrent_marking=false")).isTrue();
    }

    @Test
    public void testDefaultEnableFeatureOverridesList() {
        String overrides = CommandLineOverrideHelper.getDefaultEnableFeatureOverridesList().toString();
        assertThat(overrides.contains("LogJsConsoleMessages")).isTrue();
        assertThat(overrides.contains("LimitImageDecodeCacheSize:mb/24")).isTrue();
    }

    @Test
    public void testDefaultDisableFeatureOverridesList() {
        String overrides = CommandLineOverrideHelper.getDefaultDisableFeatureOverridesList().toString();
        assertThat(overrides.contains("AImageReader")).isTrue();
    }

    @Test
    public void testDefaultBlinkEnableFeatureOverridesList() {
        String overrides = CommandLineOverrideHelper.getDefaultBlinkEnableFeatureOverridesList().toString();
        assertThat(overrides.contains("MediaSourceNewAbortAndDuration")).isTrue();
    }

    @Test
    public void testFlagOverrides_NullParam() {
      CommandLineOverrideHelper.getFlagOverrides(null);

      Assert.assertTrue(CommandLine.getInstance().hasSwitch("single-process"));
      Assert.assertTrue(CommandLine.getInstance().hasSwitch("force-video-overlays"));
      Assert.assertTrue(CommandLine.getInstance().hasSwitch("user-level-memory-pressure-signal-params"));
      Assert.assertTrue(CommandLine.getInstance().hasSwitch("enable-low-end-device-mode"));
      Assert.assertTrue(CommandLine.getInstance().hasSwitch("disable-rgba-4444-textures"));

      String expected = "no-user-gesture-required";
      String actual = CommandLine.getInstance().getSwitchValue("autoplay-policy");
      Assert.assertEquals(expected, actual);

      expected = "1";
      actual = CommandLine.getInstance().getSwitchValue("force-device-scale-factor");
      Assert.assertEquals(expected, actual);

      expected = "passthrough";
      actual = CommandLine.getInstance().getSwitchValue("use-cmd-decoder");
      Assert.assertEquals(expected, actual);

      expected = "32";
      actual = CommandLine.getInstance().getSwitchValue("force-gpu-mem-available-mb");
      Assert.assertEquals(expected, actual);

      actual = CommandLine.getInstance().getSwitchValue("enable-features");
      expected = "LogJsConsoleMessages,LimitImageDecodeCacheSize:mb/24";
      Assert.assertEquals(expected, actual);

      actual = CommandLine.getInstance().getSwitchValue("disable-features");
      expected = "AImageReader";
      Assert.assertEquals(expected, actual);

      actual = CommandLine.getInstance().getSwitchValue("blink-enable-features");
      expected = "MediaSourceNewAbortAndDuration";
      Assert.assertEquals(expected, actual);
    }

    // TODO(b/420912320): Add tests with non-null params to flag overrides
}
