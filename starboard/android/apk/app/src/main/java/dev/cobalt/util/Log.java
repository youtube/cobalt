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
 * API for sending Starboard log output. This uses a JNI helper rather than directly calling Android
 * logging so that it remains in the app even when Android logging is stripped by Proguard.
 */
public final class Log {
  public static final String TAG = "starboard";

  private Log() {}

  private static native int nativeWrite(char priority, String tag, String msg, Throwable tr);

  public static int v(String tag, String msg) {
    return nativeWrite('v', tag, msg, null);
  }

  public static int v(String tag, String msg, Throwable tr) {
    return nativeWrite('v', tag, msg, tr);
  }

  public static int d(String tag, String msg) {
    return nativeWrite('d', tag, msg, null);
  }

  public static int d(String tag, String msg, Throwable tr) {
    return nativeWrite('d', tag, msg, tr);
  }

  public static int i(String tag, String msg) {
    return nativeWrite('i', tag, msg, null);
  }

  public static int i(String tag, String msg, Throwable tr) {
    return nativeWrite('i', tag, msg, tr);
  }

  public static int w(String tag, String msg) {
    return nativeWrite('w', tag, msg, null);
  }

  public static int w(String tag, String msg, Throwable tr) {
    return nativeWrite('w', tag, msg, tr);
  }

  public static int w(String tag, Throwable tr) {
    return nativeWrite('w', tag, "", tr);
  }

  public static int e(String tag, String msg) {
    return nativeWrite('e', tag, msg, null);
  }

  public static int e(String tag, String msg, Throwable tr) {
    return nativeWrite('e', tag, msg, tr);
  }
}
