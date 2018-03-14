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

import android.os.Build;

/** A simple utility class to detect whether we're running in an emulator or not. */
public class IsEmulator {
  private IsEmulator() {}

  public static boolean isEmulator() {
    String qemu = System.getProperty("ro.kernel.qemu", "?");
    return qemu.equals("1")
        || Build.HARDWARE.contains("goldfish")
        || Build.HARDWARE.contains("ranchu");
  }
}
