// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.FeatureMap;

/** Java accessor for base/feature_map.h state. */
@JNINamespace("signin")
public final class SigninFeatureMap extends FeatureMap {
    private static final SigninFeatureMap sInstance = new SigninFeatureMap();

    // Do not instantiate this class.
    private SigninFeatureMap() {}

    /**
     * @return the singleton SigninFeatureMap.
     */
    public static SigninFeatureMap getInstance() {
        return sInstance;
    }

    /** Convenience method to call {@link #isEnabledInNative(String)} statically. */
    public static boolean isEnabled(String featureName) {
        return getInstance().isEnabledInNative(featureName);
    }

    @Override
    protected long getNativeMap() {
        return SigninFeatureMapJni.get().getNativeMap();
    }

    @NativeMethods
    public interface Natives {
        long getNativeMap();
    }
}
