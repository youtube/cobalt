// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

package dev.cobalt.browser;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

/** Exposes the native CobaltContentBrowserClient class functionality. */
public class CobaltContentBrowserClient {
  @JNINamespace("cobalt")
  @NativeMethods
  interface Natives {
    void flushCookiesAndLocalStorage();
    void dispatchBlur();
    void dispatchFocus();
  }

  public static void flushCookiesAndLocalStorage() {
    CobaltContentBrowserClientJni.get().flushCookiesAndLocalStorage();
  }

  public static void dispatchBlur() {
    CobaltContentBrowserClientJni.get().dispatchBlur();
  }

  public static void dispatchFocus() {
    CobaltContentBrowserClientJni.get().dispatchFocus();
  }
}
