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
        assertThat(overrides.contains("--single-process")).isTrue();
        assertThat(overrides.contains("--force-video-overlays")).isTrue();
        assertThat(overrides.contains("--autoplay-policy=no-user-gesture-required")).isTrue();
        assertThat(overrides.contains("--user-level-memory-pressure-signal-params")).isTrue();
        assertThat(overrides.contains("--force-device-scale-factor=1")).isTrue();
    }

    @Test
    public void testDefaultJsFlagOverridesList() {
        String overrides = CommandLineOverrideHelper.getDefaultJsFlagOverridesList().toString();
        assertThat(overrides.contains("--optimize-for-size")).isTrue();
    }

    @Test
    public void testDefaultEnableFeatureOverridesList() {
        String overrides = CommandLineOverrideHelper.getDefaultEnableFeatureOverridesList().toString();
        assertThat(overrides.contains("LogJsConsoleMessages")).isTrue();
        assertThat(overrides.contains("LimitImageDecodeCacheSize:mb/24")).isTrue();
        assertThat(overrides.contains("LimitImageDecodeCacheAge:seconds/5")).isTrue();
    }

    @Test
    public void testDefaultDisableFeatureOverridesList() {
        String overrides =
            CommandLineOverrideHelper.getDefaultDisableFeatureOverridesList().toString();
        assertThat(overrides.contains("AImageReader")).isTrue();
    }

    @Test
    public void testDefaultBlinkEnableFeatureOverridesList() {
        String overrides =
            CommandLineOverrideHelper.getDefaultBlinkEnableFeatureOverridesList().toString();
        assertThat(overrides.contains("MediaSourceNewAbortAndDuration")).isTrue();
        assertThat(overrides.contains("PreciseMemoryInfo")).isTrue();
    }

    @Test
    public void testFlagOverrides_NullParam() {
        CommandLineOverrideHelper.getFlagOverrides(null);

        Assert.assertTrue(CommandLine.getInstance().hasSwitch("single-process"));
        Assert.assertTrue(CommandLine.getInstance().hasSwitch("force-video-overlays"));
        Assert.assertTrue(
            CommandLine.getInstance().hasSwitch("user-level-memory-pressure-signal-params"));

        String expected = "no-user-gesture-required";
        String actual = CommandLine.getInstance().getSwitchValue("autoplay-policy");
        Assert.assertEquals(expected, actual);

        expected = "1";
        actual = CommandLine.getInstance().getSwitchValue("force-device-scale-factor");
        Assert.assertEquals(expected, actual);

        actual = CommandLine.getInstance().getSwitchValue("enable-features");
        expected = CommandLineOverrideHelper.getDefaultEnableFeatureOverridesList().toString();
        Assert.assertEquals(expected, actual);

        actual = CommandLine.getInstance().getSwitchValue("disable-features");
        expected = CommandLineOverrideHelper.getDefaultDisableFeatureOverridesList().toString();
        Assert.assertEquals(expected, actual);

        actual = CommandLine.getInstance().getSwitchValue("enable-blink-features");
        expected =
            CommandLineOverrideHelper.getDefaultBlinkEnableFeatureOverridesList().toString();
        Assert.assertEquals(expected, actual);
    }

    @Test
    public void testFlagOverrides_SingleArg() {
        String[] commandLineArgs = {"--enable-features=TestFeature1;TestFeature2"};
        CommandLineOverrideHelper.CommandLineOverrideHelperParams params =
            new CommandLineOverrideHelper.CommandLineOverrideHelperParams(
                true, commandLineArgs);
        CommandLineOverrideHelper.getFlagOverrides(params);

        String actual = CommandLine.getInstance().getSwitchValue("enable-features");
        String expected =
            CommandLineOverrideHelper.getDefaultEnableFeatureOverridesList().toString()
                + ",TestFeature1,TestFeature2";
        Assert.assertEquals(expected, actual);
    }

    @Test
    public void testFlagOverrides_MultipleArgs() {
        String[] commandLineArgs = {
        "--enable-features=TestFeature1;TestFeature2",
        "--disable-features=TestFeature3",
        "--js-flags=--test-flag;--another-flag"
        };
        CommandLineOverrideHelper.CommandLineOverrideHelperParams params =
            new CommandLineOverrideHelper.CommandLineOverrideHelperParams(
                true, commandLineArgs);
        CommandLineOverrideHelper.getFlagOverrides(params);

        String enableFeatures = CommandLine.getInstance().getSwitchValue("enable-features");
        String expectedEnable =
            CommandLineOverrideHelper.getDefaultEnableFeatureOverridesList().toString()
                + ",TestFeature1,TestFeature2";
        Assert.assertEquals(expectedEnable, enableFeatures);

        String disableFeatures = CommandLine.getInstance().getSwitchValue("disable-features");
        String expectedDisable =
            CommandLineOverrideHelper.getDefaultDisableFeatureOverridesList().toString()
                + ",TestFeature3";
        Assert.assertEquals(expectedDisable, disableFeatures);

        String jsFlags = CommandLine.getInstance().getSwitchValue("js-flags");
        String expectedJs =
            CommandLineOverrideHelper.getDefaultJsFlagOverridesList().toString()
                + ",--test-flag,--another-flag";
        Assert.assertEquals(expectedJs, jsFlags);
    }

    @Test
    public void testFlagOverrides_WithRegularSwitch() {
        String[] commandLineArgs = {"--some-other-switch=value"};
        CommandLineOverrideHelper.CommandLineOverrideHelperParams params =
            new CommandLineOverrideHelper.CommandLineOverrideHelperParams(
                true, commandLineArgs);
        CommandLineOverrideHelper.getFlagOverrides(params);

        Assert.assertTrue(CommandLine.getInstance().hasSwitch("some-other-switch"));
        String actual = CommandLine.getInstance().getSwitchValue("some-other-switch");
        Assert.assertEquals("value", actual);
    }

    @Test
    public void testFlagOverrides_EmptyAndNullArgs() {
        String[] commandLineArgs = {
        "--enable-features=TestFeature1;", null, "--disable-features=TestFeature2"
        };
        CommandLineOverrideHelper.CommandLineOverrideHelperParams params =
            new CommandLineOverrideHelper.CommandLineOverrideHelperParams(
                true, commandLineArgs);
        CommandLineOverrideHelper.getFlagOverrides(params);

        String enableFeatures = CommandLine.getInstance().getSwitchValue("enable-features");
        String expectedEnable =
            CommandLineOverrideHelper.getDefaultEnableFeatureOverridesList().toString()
                + ",TestFeature1";
        Assert.assertEquals(expectedEnable, enableFeatures);

        String disableFeatures = CommandLine.getInstance().getSwitchValue("disable-features");
        String expectedDisable =
            CommandLineOverrideHelper.getDefaultDisableFeatureOverridesList().toString()
                + ",TestFeature2";
        Assert.assertEquals(expectedDisable, disableFeatures);
    }

    @Test
    public void testFlagOverrides_FeaturesWithValues() {
        String[] commandLineArgs = {"--enable-features=TestFeature1=value1;TestFeature2=value2"};
        CommandLineOverrideHelper.CommandLineOverrideHelperParams params =
            new CommandLineOverrideHelper.CommandLineOverrideHelperParams(
                true, commandLineArgs);
        CommandLineOverrideHelper.getFlagOverrides(params);

        String enableFeatures = CommandLine.getInstance().getSwitchValue("enable-features");
        String expectedEnable =
            CommandLineOverrideHelper.getDefaultEnableFeatureOverridesList().toString()
                + ",TestFeature1=value1,TestFeature2=value2";
        Assert.assertEquals(expectedEnable, enableFeatures);
    }
}
