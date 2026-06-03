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

package dev.cobalt.coat;

import android.view.KeyEvent;

/**
 * Utility methods for keyboard event handling.
 *
 * Provides helper methods to identify specific key codes that require custom handling, such as
 * partner-specific fallback keys that must be intercepted before IME dispatch to allow OS-level
 * fallback.
 *
 * This is a utility class with static methods and cannot be instantiated. It is owned by the
 * Cobalt Android runtime team and remains active throughout the application's lifetime.
 *
 * This class is stateless and thread-safe. All methods are static and can be
 * safely called from any thread.
 */
public final class KeyboardUtils {
  private KeyboardUtils() {}

  /**
   * Returns true if the key code is a partner-specific fallback key that should not be consumed by
   * Cobalt, allowing it to fall back to the OS (e.g. to switch TV channels).
   *
   * This is similar to the isGamepadButton defined in android.view.KeyEvent but custom for Cobalt-
   * specific key events that need to be handled before an IME dispatch.
   *
   * Note: These keys must be intercepted here in Java before they are dispatched to the IME
   * pipeline. The Chromium IME dispatch is asynchronous and immediately returns true (consumed) to
   * the Android OS, which prevents the OS-level fallback mechanism from triggering.
   *
   * Consequently, filtering these keys in Cobalt's native keycode conversion (e.g.
   * keyboard_code_conversion_android_cobalt.cc) is too late to allow fallback, as the key events will
   * still be consumed although they are mapped to VKEY_UNKNOWN.
   */
  public static boolean isPartnerFallbackKey(int keyCode) {
    switch (keyCode) {
      case KeyEvent.KEYCODE_11:
      case KeyEvent.KEYCODE_12:
      case KeyEvent.KEYCODE_TV_TERRESTRIAL_DIGITAL:
      case KeyEvent.KEYCODE_TV_SATELLITE_BS:
      case KeyEvent.KEYCODE_TV_SATELLITE_CS:
      case KeyEvent.KEYCODE_TV_SATELLITE_SERVICE:
        return true;
      default:
        return false;
    }
  }
}
