// Copyright 2017 Google Inc. All Rights Reserved.
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

import static dev.cobalt.util.Log.TAG;

import android.util.Log;
import java.lang.reflect.Method;

/** Utility class for accessing system properties via reflection. */
public class SystemPropertiesHelper {
  private static Method getStringMethod;
  static {
    try {
      getStringMethod = ClassLoader.getSystemClassLoader()
          .loadClass("android.os.SystemProperties")
          .getMethod("get", String.class);
      if (getStringMethod == null) {
        Log.e(TAG, "Couldn't load system properties getString");
      }
    } catch (Exception exception) {
      Log.e(TAG, "Exception looking up system properties methods: ", exception);
    }
  }

  private SystemPropertiesHelper() {}

  public static String getString(String property) {
    if (getStringMethod != null) {
      try {
        return (String) getStringMethod.invoke(null, new Object[] { property });
      } catch (Exception exception) {
        Log.e(TAG, "Exception getting system property: ", exception);
      }
    }
    return null;
  }
}
