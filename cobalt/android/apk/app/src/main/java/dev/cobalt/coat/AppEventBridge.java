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

import org.jni_zero.NativeMethods;

/**
 * Java utility bridge that forwards Android Activity lifecycle and system events directly to
 * Cobalt's native C++ AppEventDelegate via JNI.
 *
 * <p>Lifetime: Utility class with static methods bound to the application lifetime.
 *
 * <p>Threading: All methods must be invoked on the Android main UI thread.
 */
public class AppEventBridge {
  public static void handlePreloadEvent(long timestamp) {
    AppEventBridgeJni.get().handlePreloadEvent(timestamp);
  }

  public static void handleStartEvent(long timestamp) {
    AppEventBridgeJni.get().handleStartEvent(null, null, timestamp);
  }

  public static void handleBlurEvent(long timestamp) {
    AppEventBridgeJni.get().handleBlurEvent(timestamp);
  }

  public static void handleFocusEvent(long timestamp) {
    AppEventBridgeJni.get().handleFocusEvent(timestamp);
  }

  public static void handleConcealEvent(long timestamp) {
    AppEventBridgeJni.get().handleConcealEvent(timestamp);
  }

  public static void handleRevealEvent(long timestamp) {
    AppEventBridgeJni.get().handleRevealEvent(timestamp);
  }

  public static void handleFreezeEvent(long timestamp) {
    AppEventBridgeJni.get().handleFreezeEvent(timestamp);
  }

  public static void handleUnfreezeEvent(long timestamp) {
    AppEventBridgeJni.get().handleUnfreezeEvent(timestamp);
  }

  public static void handleStopEvent(long timestamp) {
    AppEventBridgeJni.get().handleStopEvent(timestamp);
  }

  public static void handleStartEvent(String[] args, String link, long timestamp) {
    AppEventBridgeJni.get().handleStartEvent(args, link, timestamp);
  }

  public static void handleOsNetworkEvent(boolean online) {
    AppEventBridgeJni.get().handleOsNetworkEvent(online);
  }

  @NativeMethods
  interface Natives {
    void handlePreloadEvent(long timestamp);

    void handleBlurEvent(long timestamp);

    void handleFocusEvent(long timestamp);

    void handleConcealEvent(long timestamp);

    void handleRevealEvent(long timestamp);

    void handleFreezeEvent(long timestamp);

    void handleUnfreezeEvent(long timestamp);

    void handleStopEvent(long timestamp);

    void handleStartEvent(String[] args, String link, long timestamp);

    void handleOsNetworkEvent(boolean online);
  }
}
