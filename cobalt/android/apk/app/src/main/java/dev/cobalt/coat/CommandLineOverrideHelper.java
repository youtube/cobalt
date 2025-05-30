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

import java.util.ArrayList;
import java.util.List;
import org.chromium.base.CommandLine;

/** Helper class to provide commandLine Overrides. */
public final class CommandLineOverrideHelper {
    private static List<String> sCliOverrides =
            getDefaultCommandLineOverridesList();
    private static StringBuilder sV8ParamOverrides =
        getDefaultV8OverridesList();
    private static StringBuilder sEnableFeatureOverrides =
        getDefaultEnableFeatureOverridesList();
    private static StringBuilder sDisableFeatureOverrides =
        getDefaultDisableFeatureOverridesList();
    private static StringBuilder sBlinkEnableFeatureOverrides =
        getDefaultBlinkEnableFeatureOverridesList();

    /** Param class to simplify #getFlagOverrides method signature */
    public static class CommandLineOverrideHelperParams {
        public CommandLineOverrideHelperParams(
            boolean shouldSetJNIPrefix,
            boolean isOfficialBuild,
            String[] commandLineOverrides,
            String[] v8CommandLineOverrides,
            String[] enableFeaturesCommandLineOverrides,
            String[] disableFeaturesCommandLineOverrides,
            String[] blinkEnableFeaturesCommandLineOverrides) {
            mShouldSetJNIPrefix = shouldSetJNIPrefix;
            mIsOfficialBuild = isOfficialBuild;
            mCommandLineOverrides = commandLineOverrides;
            mV8CommandLineOverrides = v8CommandLineOverrides;
            mEnableFeaturesCommandLineOverrides = enableFeaturesCommandLineOverrides;
            mDisableFeaturesCommandLineOverrides = disableFeaturesCommandLineOverrides;
            mBlinkEnableFeaturesCommandLineOverrides = blinkEnableFeaturesCommandLineOverrides;
        }

        private boolean mShouldSetJNIPrefix;
        private boolean mIsOfficialBuild;
        private String[] mCommandLineOverrides;
        private String[] mV8CommandLineOverrides;
        private String[] mEnableFeaturesCommandLineOverrides;
        private String[] mDisableFeaturesCommandLineOverrides;
        private String[] mBlinkEnableFeaturesCommandLineOverrides;
    }

    // This can be returned as a list, since it does not need to be a single
    // string object. The others can be combined into a single String because
    // they need to be enclosed in the feature's enable/disable header.
    public static List<String> getDefaultCommandLineOverridesList() {
        List<String> paramOverrides = new ArrayList<>();

        // Disable first run experience.
        paramOverrides.add("--disable-fre");
        paramOverrides.add("--no-first-run");
        // Run Cobalt as a single process.
        paramOverrides.add("--single-process");
        // Enable Blink to work in overlay video mode.
        paramOverrides.add("--force-video-overlays");
        // Autoplay video with url.
        paramOverrides.add("--autoplay-policy=no-user-gesture-required");
        // Remove below if Cobalt rebase to m120+.
        paramOverrides.add("--user-level-memory-pressure-signal-params");
        // Disable rescaling Webpage.
        paramOverrides.add("--force-device-scale-factor=1");
        // Enable low end device mode.
        paramOverrides.add("--enable-low-end-device-mode");
        // Disables RGBA_4444 textures which
        // causes rendering artifacts when
        // low-end-device-mode is enabled.
        paramOverrides.add("--disable-rgba-4444-textures");

        return paramOverrides;
    }

    public static StringBuilder getDefaultV8OverridesList() {
        StringBuilder paramOverrides = new StringBuilder();

        // Trades a little V8 performance for significant memory savings.
        paramOverrides.append("--optimize_for_size=true");
        paramOverrides.append(",");
        // Disable concurrent-marking due to b/415843979
        paramOverrides.append("--concurrent_marking=false");

        return paramOverrides;
    }

    public static StringBuilder getDefaultEnableFeatureOverridesList() {
        StringBuilder paramOverrides = new StringBuilder();

        // Pass javascript console log to adb log.
        paramOverrides.append("LogJsConsoleMessages");
        paramOverrides.append(",");
        // Limit decoded image cache to 32 mbytes.
        paramOverrides.append("LimitImageDecodeCacheSize:mb/32");

        return paramOverrides;
    }

    public static StringBuilder getDefaultDisableFeatureOverridesList() {
        StringBuilder paramOverrides = new StringBuilder();

        // Use SurfaceTexture for decode-to-texture mode.
        paramOverrides.append("AImageReader");

        return paramOverrides;
    }

    public static StringBuilder getDefaultBlinkEnableFeatureOverridesList() {
        StringBuilder paramOverrides = new StringBuilder();

        // Align with MSE spec for MediaSource.duration.
        paramOverrides.append("MediaSourceNewAbortAndDuration");

        return paramOverrides;
    }

    public static void getFlagOverrides(
        CommandLineOverrideHelperParams params) {
        if (params != null) {
            if (params.mShouldSetJNIPrefix) {
                // Helps Kimono build avoid package name conflict with cronet.
                sCliOverrides.add("--cobalt-jni-prefix");
            }
            if (!params.mIsOfficialBuild) {
                sCliOverrides.add(
                  "--remote-allow-origins="
                  + "https://chrome-devtools-frontend.appspot.com");
            }

            if (params.mCommandLineOverrides != null) {
                for (String param: params.mCommandLineOverrides) {
                    sCliOverrides.add(param);
                }
            }
            if (params.mV8CommandLineOverrides != null) {
                for (String param: params.mV8CommandLineOverrides) {
                    sV8ParamOverrides.append(",");
                    sV8ParamOverrides.append(param);
                }
            }
            if (params.mEnableFeaturesCommandLineOverrides != null) {
                for (String param: params.mEnableFeaturesCommandLineOverrides) {
                    sEnableFeatureOverrides.append(",");
                    sEnableFeatureOverrides.append(param);
                }
            }

            if (params.mDisableFeaturesCommandLineOverrides != null) {
                for (String param: params.mDisableFeaturesCommandLineOverrides) {
                    sDisableFeatureOverrides.append(",");
                    sDisableFeatureOverrides.append(param);
                }
            }
            if (params.mBlinkEnableFeaturesCommandLineOverrides != null) {
                for (String param: params.mBlinkEnableFeaturesCommandLineOverrides) {
                    sDisableFeatureOverrides.append(",");
                    sBlinkEnableFeatureOverrides.append(param);
                }
            }
        }
        String[] cliArgs = sCliOverrides.toArray(new String[0]);
        CommandLine.getInstance().appendSwitchesAndArguments(cliArgs);
        CommandLine.getInstance().appendSwitchesAndArguments(
            new String[]{"--js-flags=" + sV8ParamOverrides.toString() });
        CommandLine.getInstance().appendSwitchesAndArguments(
            new String[]{"--enable-features="
            + sEnableFeatureOverrides.toString() });
        CommandLine.getInstance().appendSwitchesAndArguments(
            new String[]{"--disable-features="
            + sDisableFeatureOverrides.toString() });
        CommandLine.getInstance().appendSwitchesAndArguments(
            new String[]{"--blink-enable-features="
            + sBlinkEnableFeatureOverrides.toString() });

    }
}
