// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.url;

import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;

/**
 * Helpers for OriginJavaTest that need to call into native code.
 */
@JNINamespace("url")
public class OriginJavaTestHelper {
    public static void testOriginEquivalence() {
        OriginJavaTestHelperJni.get().testOriginEquivalence();
    }

    @NativeMethods
    interface Natives {
        void testOriginEquivalence();
    }
}
