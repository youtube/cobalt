package dev.cobalt.coat;

import static dev.cobalt.util.Log.TAG;

import dev.cobalt.util.Log;
import dev.cobalt.util.UsedByNative;

public class CrashContextProvider {
  @SuppressWarnings("unused")
  @UsedByNative
  public static void setContext(String key, String value) {
    Log.e(TAG, "Colin test: setContext:" + key + ", " + value);
  }
}
