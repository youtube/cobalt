// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import java.util.Collections;
import java.util.Map;
import org.chromium.build.annotations.NullMarked;

/**
 * Java accessor for state of feature flags and their field trial parameters.
 */
@NullMarked
public abstract class FeatureMap {
    protected FeatureMap() {}

    /**
     * Should return the native pointer to the specific base::FeatureMap for the component/layer.
     */
    protected abstract long getNativeMap();

    /**
     * Returns whether the specified feature is enabled or not.
     */
    public boolean isEnabledInNative(String featureName) {
        return false;
    }

    /**
     * Returns a field trial param for the specified feature.
     */
    public String getFieldTrialParamByFeature(String featureName, String paramName) {
        return "";
    }

    /**
     * Returns a field trial param as a boolean for the specified feature.
     */
    public boolean getFieldTrialParamByFeatureAsBoolean(
            String featureName, String paramName, boolean defaultValue) {
        return defaultValue;
    }

    /**
     * Returns a field trial param as an int for the specified feature.
     */
    public int getFieldTrialParamByFeatureAsInt(
            String featureName, String paramName, int defaultValue) {
        return defaultValue;
    }

    /**
     * Returns a field trial param as a double for the specified feature.
     */
    public double getFieldTrialParamByFeatureAsDouble(
            String featureName, String paramName, double defaultValue) {
        return defaultValue;
    }

    /** Returns all the field trial parameters for the specified feature. */
    public Map<String, String> getFieldTrialParamsForFeature(String featureName) {
        return Collections.emptyMap();
    }
}
