// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.crash.browser;

import org.chromium.build.NativeLibraries;
import org.chromium.build.annotations.DoNotInline;
import org.chromium.build.annotations.UsedByReflection;

/**
 * Cobalt-specific entry point for the Crashpad handler process.
 * This is launched via /system/bin/app_process from the native side.
 * 
 * NOTE: Using traditional native method instead of @NativeMethods to avoid
 * org.chromium.base.natives.GEN_JNI name collision in Google3 monolithic build.
 *
 * This class provides a decoupled entry point for the crashpad_handler process,
 * ensuring minimal initialization and isolation from the main application's JAR
 * symbols. This is critical for stable crash reporting on memory-constrained 
 * legacy hardware (API 24-28).
 */
public final class CrashpadMainCobalt {
    @UsedByReflection("crashpad_android.cc")
    public static void main(String[] argv) {
        loadNativeLibraries();
        nativeCrashpadMain(argv);
    }

    /**
     * References to NativeLibraries are in a separate method to avoid R8 inlining
     * issues when classes (like NativeLibraries) might not exist in all APK splits.
     */
    @DoNotInline
    private static void loadNativeLibraries() {
        try {
            System.loadLibrary("chrome_crashpad_handler");
        } catch (UnsatisfiedLinkError e) {
            throw new RuntimeException(e);
        }
    }

    @UsedByReflection("crashpad_handler_jni.cc")
    private static native void nativeCrashpadMain(String[] argv);
}
