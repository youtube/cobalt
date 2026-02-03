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

import java.util.ArrayList;
import java.util.List;
import java.util.Map;

/**
 * Defines the constant names for feature switches used in Kimono.
 */
public class JavaSwitches {
  public static final String ENABLE_QUIC = "EnableQUIC";
  public static final String DISABLE_STARTUP_GUARD = "DisableStartupGuard";
  public static final String DISABLE_LOW_END_DEVICE_MODE = "DisableLowEndDeviceMode";

  /** V8 flag to enable jitless mode. Value type: Boolean (presence means true) */
  public static final String V8_JITLESS = "V8Jitless";

  /** V8 flag to enable write protection for code memory. Value type: Boolean (presence means true) */
  public static final String V8_WRITE_PROTECT_CODE_MEMORY = "V8WriteProtectCodeMemory";

  /** V8 flag to set the GC interval. Value type: Integer */
  public static final String V8_GC_INTERVAL = "V8GcInterval";

  /** V8 flag to set the initial old space size. Value type: Integer (MiB) */
  public static final String V8_INITIAL_OLD_SPACE_SIZE = "V8InitialOldSpaceSize";

  /** V8 flag to set the maximum old space size. Value type: Integer (MiB) */
  public static final String V8_MAX_OLD_SPACE_SIZE = "V8MaxOldSpaceSize";

  /** V8 flag to set the maximum semi space size. Value type: Integer (MiB) */
  public static final String V8_MAX_SEMI_SPACE_SIZE = "V8MaxSemiSpaceSize";

  public static List<String> getExtraCommandLineArgs(Map<String, String> javaSwitches) {
    List<String> extraCommandLineArgs = new ArrayList<>();
    if (!javaSwitches.containsKey(JavaSwitches.ENABLE_QUIC)) {
      extraCommandLineArgs.add("--disable-quic");
    }
    if (!javaSwitches.containsKey(JavaSwitches.DISABLE_LOW_END_DEVICE_MODE)) {
      extraCommandLineArgs.add("--enable-low-end-device-mode");
      extraCommandLineArgs.add("--disable-rgba-4444-textures");
    }

    if (javaSwitches.containsKey(JavaSwitches.V8_JITLESS)) {
      extraCommandLineArgs.add("--js-flags=--jitless");
    }
    if (javaSwitches.containsKey(JavaSwitches.V8_WRITE_PROTECT_CODE_MEMORY)) {
      extraCommandLineArgs.add("--js-flags=--write-protect-code-memory");
    }
    if (javaSwitches.containsKey(JavaSwitches.V8_GC_INTERVAL)) {
      extraCommandLineArgs.add(
          "--js-flags=--gc-interval="
              + javaSwitches.get(JavaSwitches.V8_GC_INTERVAL).replaceAll("[^0-9]", ""));
    }
    if (javaSwitches.containsKey(JavaSwitches.V8_INITIAL_OLD_SPACE_SIZE)) {
      extraCommandLineArgs.add(
          "--js-flags=--initial-old-space-size="
              + javaSwitches.get(JavaSwitches.V8_INITIAL_OLD_SPACE_SIZE).replaceAll("[^0-9]", ""));
    }
    if (javaSwitches.containsKey(JavaSwitches.V8_MAX_OLD_SPACE_SIZE)) {
      extraCommandLineArgs.add(
          "--js-flags=--max-old-space-size="
              + javaSwitches.get(JavaSwitches.V8_MAX_OLD_SPACE_SIZE).replaceAll("[^0-9]", ""));
    }
    if (javaSwitches.containsKey(JavaSwitches.V8_MAX_SEMI_SPACE_SIZE)) {
      extraCommandLineArgs.add(
          "--js-flags=--max-semi-space-size="
              + javaSwitches.get(JavaSwitches.V8_MAX_SEMI_SPACE_SIZE).replaceAll("[^0-9]", ""));
    }
    return extraCommandLineArgs;
  }
}
