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
 * This class is thread-safe and its methods can be safely called from any thread.
 */
public final class Log {
  public static final String TAG = "starboard";

  private Log() {}

  // ===============================================================================================
  // VERBOSE (v) & DEBUG (d) Optimized Overloads
  // ===============================================================================================
  //
  // WHY:
  // Every call to a varargs method like v(tag, template, Object...) forces the Java compiler to
  // allocate a `new Object[]` array at the call site *before* entering the method.
  //
  // If logging is disabled (e.g., in production), this array is immediately discarded, causing
  // severe Garbage Collection (GC) pressure if the log is located in a hot path (like the video
  // frame rendering loop).
  //
  // By providing these optimized overloads (taking 0 to 4 parameters) and checking `isLoggable`
  // *before* delegating to the varargs `org.chromium.base.Log`, we completely avoid any array
  // allocations when logging is disabled.
  //
  // WHY ONLY v AND d:
  // `v` and `d` are verbose/debug logs that are disabled by default in production but might be
  // left in performance-critical paths. `i`, `w`, and `e` are enabled by default and should
  // never be placed in hot paths, making their allocation overhead negligible.
  // ===============================================================================================

  public static void v(String tag, String message) {
    if (!org.chromium.base.Log.isLoggable(tag, org.chromium.base.Log.VERBOSE)) return;
    org.chromium.base.Log.v(tag, message);
  }

  public static void v(String tag, String messageTemplate, Object p1) {
    if (!org.chromium.base.Log.isLoggable(tag, org.chromium.base.Log.VERBOSE)) return;
    org.chromium.base.Log.v(tag, messageTemplate, p1);
  }

  public static void v(String tag, String messageTemplate, Object p1, Object p2) {
    if (!org.chromium.base.Log.isLoggable(tag, org.chromium.base.Log.VERBOSE)) return;
    org.chromium.base.Log.v(tag, messageTemplate, p1, p2);
  }

  public static void v(String tag, String messageTemplate, Object p1, Object p2, Object p3) {
    if (!org.chromium.base.Log.isLoggable(tag, org.chromium.base.Log.VERBOSE)) return;
    org.chromium.base.Log.v(tag, messageTemplate, p1, p2, p3);
  }

  public static void v(String tag, String messageTemplate, Object p1, Object p2, Object p3, Object p4) {
    if (!org.chromium.base.Log.isLoggable(tag, org.chromium.base.Log.VERBOSE)) return;
    org.chromium.base.Log.v(tag, messageTemplate, p1, p2, p3, p4);
  }

  // Fallback for 5+ arguments (always allocates array at call site)
  public static void v(String tag, String messageTemplate, Object... args) {
    org.chromium.base.Log.v(tag, messageTemplate, args);
  }

  public static void d(String tag, String message) {
    if (!org.chromium.base.Log.isLoggable(tag, org.chromium.base.Log.DEBUG)) return;
    org.chromium.base.Log.d(tag, message);
  }

  public static void d(String tag, String messageTemplate, Object p1) {
    if (!org.chromium.base.Log.isLoggable(tag, org.chromium.base.Log.DEBUG)) return;
    org.chromium.base.Log.d(tag, messageTemplate, p1);
  }

  public static void d(String tag, String messageTemplate, Object p1, Object p2) {
    if (!org.chromium.base.Log.isLoggable(tag, org.chromium.base.Log.DEBUG)) return;
    org.chromium.base.Log.d(tag, messageTemplate, p1, p2);
  }

  public static void d(String tag, String messageTemplate, Object p1, Object p2, Object p3) {
    if (!org.chromium.base.Log.isLoggable(tag, org.chromium.base.Log.DEBUG)) return;
    org.chromium.base.Log.d(tag, messageTemplate, p1, p2, p3);
  }

  public static void d(String tag, String messageTemplate, Object p1, Object p2, Object p3, Object p4) {
    if (!org.chromium.base.Log.isLoggable(tag, org.chromium.base.Log.DEBUG)) return;
    org.chromium.base.Log.d(tag, messageTemplate, p1, p2, p3, p4);
  }

  // Fallback for 5+ arguments (always allocates array at call site)
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
