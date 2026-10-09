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

/**
 * Constants for V8 engine flags passed via {@code --js-flags}.
 *
 * <p>Unlike Chromium switches ({@code const char kFoo[] = "..."}), V8 declares its flags in {@code
 * //v8/src/flags/flag-definitions.h} using X-macros ({@code DEFINE_BOOL}, {@code DEFINE_INT},
 * {@code DEFINE_UINT}, {@code DEFINE_SIZE_T}, etc.), which Chromium's {@code java_cpp_strings}
 * generator cannot parse. Therefore, the V8 flag names used by Cobalt on Android are defined here
 * with references to their native definitions in {@code //v8/src/flags/flag-definitions.h}.
 *
 * <p>Note: V8's flag parser treats hyphens ({@code -}) and underscores ({@code _}) as equivalent
 * (normalizing {@code -} to {@code _} in {@code //v8/src/flags/flags.cc}). Consistent with {@link
 * org.chromium.base.BaseSwitches} and {@link CobaltNativeSwitches}, these constants omit the
 * leading {@code "--"} prefix.
 */
public final class V8Flags {
  /**
   * Enables the Minor Mark-Sweep young generation garbage collector.
   *
   * <p>Corresponds to {@code DEFINE_BOOL(minor_ms, ...)} in {@code
   * //v8/src/flags/flag-definitions.h}.
   */
  public static final String MINOR_MS = "minor-ms";

  /**
   * Minimum new space capacity in MBs for using young generation concurrent marking.
   *
   * <p>Corresponds to {@code DEFINE_UINT(minor_ms_min_new_space_capacity_for_concurrent_marking_mb,
   * ...)} in {@code //v8/src/flags/flag-definitions.h}.
   */
  public static final String MINOR_MS_MIN_NEW_SPACE_CAPACITY_FOR_CONCURRENT_MARKING_MB =
      "minor-ms-min-new-space-capacity-for-concurrent-marking-mb";

  /**
   * Enables flushing of bytecode when it has not been executed recently.
   *
   * <p>Corresponds to {@code DEFINE_BOOL(flush_bytecode, ...)} in {@code
   * //v8/src/flags/flag-definitions.h}.
   */
  public static final String FLUSH_BYTECODE = "flush-bytecode";

  /**
   * Number of seconds before V8 flushes bytecode.
   *
   * <p>Corresponds to {@code DEFINE_INT(bytecode_old_time, ...)} in {@code
   * //v8/src/flags/flag-definitions.h}.
   */
  public static final String BYTECODE_OLD_TIME = "bytecode-old-time";

  /**
   * Initial old space size in Mbytes.
   *
   * <p>Corresponds to {@code DEFINE_SIZE_T(initial_old_space_size, ...)} in {@code
   * //v8/src/flags/flag-definitions.h}.
   */
  public static final String INITIAL_OLD_SPACE_SIZE = "initial-old-space-size";

  /**
   * Max size of the old space in Mbytes.
   *
   * <p>Corresponds to {@code DEFINE_SIZE_T(max_old_space_size, ...)} in {@code
   * //v8/src/flags/flag-definitions.h}.
   */
  public static final String MAX_OLD_SPACE_SIZE = "max-old-space-size";

  /**
   * Disables the Sparkplug baseline compiler.
   *
   * <p>Negation of {@code DEFINE_BOOL(sparkplug, ...)} in {@code //v8/src/flags/flag-definitions.h}
   * (V8 boolean flags automatically support a {@code no-} prefix in {@code
   * //v8/src/flags/flags.cc}).
   */
  public static final String NO_SPARKPLUG = "no-sparkplug";

  private V8Flags() {}
}
