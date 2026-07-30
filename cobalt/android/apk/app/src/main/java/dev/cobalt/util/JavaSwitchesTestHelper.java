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

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

/**
 * Exposes {@link JavaSwitches} internals to native browser tests.
 *
 * <p>Kept separate from JavaSwitches so that each generated JNI header has a single native includer
 * that calls all of its functions (JavaSwitches_jni.h is used by the native library loader).
 */
@JNINamespace("cobalt")
public class JavaSwitchesTestHelper {
  private JavaSwitchesTestHelper() {}

  @CalledByNative
  public static boolean shouldApplyExperimentConfigs() {
    return JavaSwitches.shouldApplyExperimentConfigs();
  }
}
