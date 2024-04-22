// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.url;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;

/**
 * Helpers for GURLJavaTest that need to call into native code.
 */
@JNINamespace("url")
public class GURLJavaTestHelper {
    @CalledByNative
    public static GURL createGURL(String uri) {
        return new GURL(uri);
    }

    public static void nativeInitializeICU() {
        GURLJavaTestHelperJni.get().initializeICU();
    }

    public static void nativeTestGURLEquivalence() {
        GURLJavaTestHelperJni.get().testGURLEquivalence();
    }

    @NativeMethods
    interface Natives {
        void initializeICU();
        void testGURLEquivalence();
    }
}
