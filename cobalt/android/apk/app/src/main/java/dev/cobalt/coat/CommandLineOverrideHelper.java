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

import androidx.annotation.NonNull;
import java.util.ArrayList;
import java.util.List;
import java.util.StringJoiner;
import org.chromium.base.CommandLine;

/**
 * Helper class to parse runtime command-line argument overrides before the native library is
 * loaded. Default Cobalt switches and feature overrides are applied in C++ via {@code
 * cobalt/app/cobalt_switch_defaults_android.cc}.
 */
public final class CommandLineOverrideHelper {
  private CommandLineOverrideHelper() {} // Prevent instantiation.

  public static void getFlagOverrides(@NonNull List<String> commandLineArgs) {
    List<String> cliOverrides = new ArrayList<>();
    StringJoiner jsFlagOverrides = new StringJoiner(",");
    StringJoiner enableFeatureOverrides = new StringJoiner(",");
    StringJoiner disableFeatureOverrides = new StringJoiner(",");
    StringJoiner blinkEnableFeatureOverrides = new StringJoiner(",");
    StringJoiner traceStartupOverrides = new StringJoiner(",");
    StringJoiner enableH5vccSettings = new StringJoiner(";");

    for (String param : commandLineArgs) {
      if (param == null || param.isEmpty()) {
        continue;
      }
      String[] parts = param.split("=", 2);
      if (parts.length != 2) {
        cliOverrides.add(param);
        continue;
      }

      String key = parts[0];
      String value = parts[1];
      for (String v : value.split(";")) {
        if (key.equals("--js-flags")) {
          jsFlagOverrides.add(v);
        } else if (key.equals("--enable-features")) {
          enableFeatureOverrides.add(v);
        } else if (key.equals("--disable-features")) {
          disableFeatureOverrides.add(v);
        } else if (key.equals("--enable-blink-features")) {
          blinkEnableFeatureOverrides.add(v);
        } else if (key.equals("--trace-startup")) {
          traceStartupOverrides.add(v);
        } else if (key.equals("--enable-h5vcc-settings")) {
          enableH5vccSettings.add(v);
        } else {
          cliOverrides.add(param);
          break; // Avoid adding the same param multiple times
        }
      }
    }

    if (jsFlagOverrides.length() > 0) {
      cliOverrides.add("--js-flags=" + jsFlagOverrides.toString());
    }
    if (enableFeatureOverrides.length() > 0) {
      cliOverrides.add("--enable-features=" + enableFeatureOverrides.toString());
    }
    if (disableFeatureOverrides.length() > 0) {
      cliOverrides.add("--disable-features=" + disableFeatureOverrides.toString());
    }
    if (blinkEnableFeatureOverrides.length() > 0) {
      cliOverrides.add("--enable-blink-features=" + blinkEnableFeatureOverrides.toString());
    }
    if (traceStartupOverrides.length() > 0) {
      cliOverrides.add("--trace-startup=" + traceStartupOverrides.toString());
    }
    if (enableH5vccSettings.length() > 0) {
      cliOverrides.add("--enable-h5vcc-settings=" + enableH5vccSettings.toString());
    }

    CommandLine.getInstance().appendSwitchesAndArguments(cliOverrides.toArray(new String[0]));
  }
}
