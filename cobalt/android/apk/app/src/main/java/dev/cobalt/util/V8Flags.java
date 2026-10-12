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
 * <p>NOTE: Unlike Chromium switches ({@code const char kFoo[] = "..."}), V8 defines its flags using
 * macros ({@code DEFINE_BOOL}, {@code DEFINE_INT}, etc.) in {@code
 * //v8/src/flags/flag-definitions.h}, which {@code java_cpp_strings} cannot parse to autogenerate
 * Java constants.
 */
public final class V8Flags {
  public static final String MINOR_MS = "minor-ms";
  public static final String MINOR_MS_MIN_NEW_SPACE_CAPACITY_FOR_CONCURRENT_MARKING_MB =
      "minor-ms-min-new-space-capacity-for-concurrent-marking-mb";
  public static final String FLUSH_BYTECODE = "flush-bytecode";
  public static final String BYTECODE_OLD_TIME = "bytecode-old-time";
  public static final String INITIAL_OLD_SPACE_SIZE = "initial-old-space-size";
  public static final String MAX_OLD_SPACE_SIZE = "max-old-space-size";
  public static final String NO_SPARKPLUG = "no-sparkplug";

  private V8Flags() {}
}
