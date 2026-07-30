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

package org.chromium.content_browsertests_apk;

import dev.cobalt.coat.BaseStarboardBridge;
import org.jni_zero.CalledByNativeForTesting;
import org.jni_zero.JNINamespace;

/** Helper to simulate system ApplicationExitInfo exit reasons for Cobalt browsertests. */
@JNINamespace("cobalt")
public class MockProcessExitReasonHelper {

  // Defined as literal integer constants matching android.app.ApplicationExitInfo (API 30+)
  // to avoid ClassNotFoundException/NoClassDefFoundError on API < 30 devices during class loading.
  public static final int REASON_EXIT_SELF = 1;
  public static final int REASON_LOW_MEMORY = 3;
  public static final int REASON_CRASH = 4;
  public static final int REASON_USER_REQUESTED = 10;

  @CalledByNativeForTesting
  public static void setMockExitReasonForTesting(int reason) {
    BaseStarboardBridge.setWasLowMemoryKilledForTesting(reason == REASON_LOW_MEMORY);
  }

  @CalledByNativeForTesting
  public static void setMockEmptyExitReasonsForTesting() {
    BaseStarboardBridge.setWasLowMemoryKilledForTesting(false);
  }

  @CalledByNativeForTesting
  public static void setMockNullExitReasonsForTesting() {
    BaseStarboardBridge.setWasLowMemoryKilledForTesting(false);
  }

  @CalledByNativeForTesting
  public static void setMockThrowsExceptionForTesting() {
    BaseStarboardBridge.setWasLowMemoryKilledForTesting(false);
  }

  @CalledByNativeForTesting
  public static void resetForTesting() {
    BaseStarboardBridge.setWasLowMemoryKilledForTesting(null);
  }
}
