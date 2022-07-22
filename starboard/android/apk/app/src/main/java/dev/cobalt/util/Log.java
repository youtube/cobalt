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

import java.lang.reflect.Method;
import java.util.Locale;

/**
 * Logging wrapper to allow for better control of Proguard log stripping. Many dependent
 * configurations have rules to strip logs which makes it hard to control from the app.
 *
 * <p>The implementation is using reflection.
 */
public final class Log {
  public static final String TAG = "starboard";

  private static Method logV;
  private static Method logD;
  private static Method logI;
  private static Method logW;
  private static Method logE;

  static {
    initLogging();
  }

  private Log() {}

  private static void initLogging() {
    try {
      logV =
          android.util.Log.class.getDeclaredMethod(
              "v", String.class, String.class, Throwable.class);
      logD =
          android.util.Log.class.getDeclaredMethod(
              "d", String.class, String.class, Throwable.class);
      logI =
          android.util.Log.class.getDeclaredMethod(
              "i", String.class, String.class, Throwable.class);
      logW =
          android.util.Log.class.getDeclaredMethod(
              "w", String.class, String.class, Throwable.class);
      logE =
          android.util.Log.class.getDeclaredMethod(
              "e", String.class, String.class, Throwable.class);
    } catch (Throwable e) {
      // ignore
    }
  }

  private static Throwable getThrowableToLog(Object[] args) {
    if (args == null || args.length == 0) return null;
    Object lastArg = args[args.length - 1];
    if (!(lastArg instanceof Throwable)) return null;
    return (Throwable) lastArg;
  }

  /** Returns a formatted log message, using the supplied format and arguments. */
  private static String formatLog(String messageTemplate, Throwable tr, Object... params) {
    if ((params != null) && ((tr == null && params.length > 0) || params.length > 1)) {
      messageTemplate = String.format(Locale.US, messageTemplate, params);
    }
    return messageTemplate;
  }

  private static int logWithMethod(
      Method logMethod, String tag, String messageTemplate, Object... args) {
    try {
      if (logMethod != null) {
        Throwable tr = getThrowableToLog(args);
        String msg = formatLog(messageTemplate, tr, args);
        return (int) logMethod.invoke(null, tag, msg, tr);
      }
    } catch (Throwable e) {
      // ignore
    }
    return 0;
  }

  public static int v(String tag, String messageTemplate, Object... args) {
    return logWithMethod(logV, tag, messageTemplate, args);
  }

  public static int d(String tag, String messageTemplate, Object... args) {
    return logWithMethod(logD, tag, messageTemplate, args);
  }

  public static int i(String tag, String messageTemplate, Object... args) {
    return logWithMethod(logI, tag, messageTemplate, args);
  }

  public static int w(String tag, String messageTemplate, Object... args) {
    return logWithMethod(logW, tag, messageTemplate, args);
  }

  public static int w(String tag, Throwable tr) {
    return logWithMethod(logW, tag, "", tr);
  }

  public static int e(String tag, String messageTemplate, Object... args) {
    return logWithMethod(logE, tag, messageTemplate, args);
  }
}
