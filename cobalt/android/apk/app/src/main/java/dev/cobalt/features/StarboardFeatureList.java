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

package dev.cobalt.features;

import org.chromium.base.FeatureList;
import org.chromium.build.annotations.NullMarked;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

/** A FeatureList for our Starboard features to be used in Java. */
@JNINamespace("starboard::features")
@NullMarked
public class StarboardFeatureList {
    private StarboardFeatureList() {}
    /**
     * Returns whether the specified feature is enabled or not.
     *
     * Note: This queries features defined with STARBOARD_FEATURE in
     * //starboard/extension/feature_config.h
     *
     * @param featureName The name of the feature to query.
     * @return Whether the feature is enabled or not.
     */
    public static boolean isEnabled(String featureName) {
        assert FeatureList.isNativeInitialized();
        return StarboardFeatureListJni.get().isEnabled(featureName);
    }

    /** Native Methods for StarboardFeatureList. */
    @NativeMethods
    public interface Natives {
        boolean isEnabled(String featureName);
    }
}
