// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.glic;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.UnownedUserDataKey;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.base.WindowAndroid;

/** JNI Bridge to dispatch native C++ Glic nudge requests to the active tab strip. */
@JNINamespace("glic")
@NullMarked
public class GlicNudgeDelegateBridge {
    private static final UnownedUserDataKey<GlicNudgeDelegate> KEY = new UnownedUserDataKey<>();

    public static void setDelegate(WindowAndroid window, @Nullable GlicNudgeDelegate delegate) {
        if (delegate == null) {
            KEY.detachFromHost(window.getUnownedUserDataHost());
        } else {
            KEY.attachToHost(window.getUnownedUserDataHost(), delegate);
        }
    }

    /** Notifies native side of user nudge activity. */
    public static void onNudgeActivity(WindowAndroid window, @GlicNudgeActivity int event) {
        GlicNudgeDelegateBridgeJni.get().onNudgeActivity(window, event);
    }

    @CalledByNative
    private static void triggerGlicNudge(
            WindowAndroid window,
            String label,
            String anchoredMessageText,
            String promptSuggestion) {
        GlicNudgeDelegate delegate = KEY.retrieveDataFromHost(window.getUnownedUserDataHost());
        if (delegate != null) {
            delegate.onTriggerGlicNudgeUi(label, anchoredMessageText, promptSuggestion);
        }
    }

    @CalledByNative
    private static void hideGlicNudge(WindowAndroid window) {
        GlicNudgeDelegate delegate = KEY.retrieveDataFromHost(window.getUnownedUserDataHost());
        if (delegate != null) {
            delegate.onHideGlicNudgeUi();
        }
    }

    @CalledByNative
    private static boolean getIsShowingGlicNudge(WindowAndroid window) {
        GlicNudgeDelegate delegate = KEY.retrieveDataFromHost(window.getUnownedUserDataHost());
        return delegate != null && delegate.getIsShowingGlicNudge();
    }

    @NativeMethods
    public interface Natives {
        void onNudgeActivity(WindowAndroid window, int event);
    }
}
