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

package dev.cobalt.coat;

import org.chromium.base.FeatureMap;
import org.chromium.build.annotations.NullMarked;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

/** A featureMap for our starboard features to be used in java. */
@JNINamespace("cobalt::features")
@NullMarked
public class StarboardFeatureMap extends FeatureMap {
    private static final StarboardFeatureMap sInstance = new StarboardFeatureMap();

    // Do not instantiate this class.
    private StarboardFeatureMap() {}

    /**
     * @return the singleton {@link StarboardFeatureMap}
     */
    public static StarboardFeatureMap getInstance() {
        return sInstance;
    }

    /** Convenience method to call {@link #isEnabledInNative(String)} statically. */
    public static boolean isEnabled(String featureName) {
        return getInstance().isEnabledInNative(featureName);
    }

    @Override
    protected long getNativeMap() {
        return StarboardFeatureMapJni.get().getNativeMap();
    }

    /** Native Methods for StarboardFeatureMap. */
    @NativeMethods
    public interface Natives {
        long getNativeMap();
    }
}
