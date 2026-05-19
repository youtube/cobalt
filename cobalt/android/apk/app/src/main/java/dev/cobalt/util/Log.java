// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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
 * Logging wrapper that delegates directly to Chromium's base Log class at compile time.
 * This proxy completely eliminates JNI reflection overhead, retains optimal ProGuard
 * dead-code stripping, and restores global usability of Log.d and Log.v.
 */
public final class Log {
  public static final String TAG = "starboard";

  private Log() {}

  public static void v(String tag, String messageTemplate, Object... args) {
    org.chromium.base.Log.v(tag, messageTemplate, args);
  }

  public static void d(String tag, String messageTemplate, Object... args) {
    org.chromium.base.Log.d(tag, messageTemplate, args);
  }

  public static void i(String tag, String messageTemplate, Object... args) {
    org.chromium.base.Log.i(tag, messageTemplate, args);
  }

  public static void w(String tag, String messageTemplate, Object... args) {
    org.chromium.base.Log.w(tag, messageTemplate, args);
  }

  public static void e(String tag, String messageTemplate, Object... args) {
    org.chromium.base.Log.e(tag, messageTemplate, args);
  }

}
