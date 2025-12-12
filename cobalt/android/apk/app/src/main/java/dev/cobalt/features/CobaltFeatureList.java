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
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.build.annotations.MainDex;

/** A FeatureList for our Cobalt features to be used in Java. */
@JNINamespace("cobalt::features")
@MainDex
public class CobaltFeatureList {
    private CobaltFeatureList() {}
    /**
     * Returns whether the specified feature is enabled or not.
     *
     * Note: Features queried through this API must be added to the array
     * |kFeaturesExposedToJava| in //cobalt/common/features/cobalt_feature_list.cc
     *
     * @param featureName The name of the feature to query.
     * @return Whether the feature is enabled or not.
     */
    public static boolean isEnabled(String featureName) {
        Boolean testValue = FeatureList.getTestValueForFeature(featureName);
        if (testValue != null) { return testValue; }
        assert FeatureList.isNativeInitialized();
        return CobaltFeatureListJni.get().isEnabled(featureName);
    }

    @NativeMethods
    interface Natives {
        boolean isEnabled(String featureName);
    }

    public static final String NON_TUNNELED_DECODE_ONLY = "NonTunneledDecodeOnly";
}
