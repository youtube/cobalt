// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.flags;

import androidx.annotation.AnyThread;

import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;

/**
 * A boolean-type {@link CachedFieldTrialParameter}.
 */
public class BooleanCachedFieldTrialParameter extends CachedFieldTrialParameter {
    private boolean mDefaultValue;

    public BooleanCachedFieldTrialParameter(
            String featureName, String variationName, boolean defaultValue) {
        super(featureName, variationName, FieldTrialParameterType.BOOLEAN);
        mDefaultValue = defaultValue;
    }

    /**
     * @return the value of the field trial parameter that should be used in this run.
     */
    @AnyThread
    public boolean getValue() {
        CachedFlagsSafeMode.getInstance().onFlagChecked();

        String preferenceName = getSharedPreferenceKey();
        boolean defaultValue = getDefaultValue();

        Boolean value = ValuesOverridden.getBool(preferenceName);
        if (value != null) {
            return value;
        }

        synchronized (ValuesReturned.sBoolValues) {
            value = ValuesReturned.sBoolValues.get(preferenceName);
            if (value != null) {
                return value;
            }

            value = CachedFlagsSafeMode.getInstance().getBooleanFieldTrialParam(
                    preferenceName, defaultValue);
            if (value == null) {
                value = ChromeSharedPreferences.getInstance().readBoolean(
                        preferenceName, defaultValue);
            }

            ValuesReturned.sBoolValues.put(preferenceName, value);
        }
        return value;
    }

    public boolean getDefaultValue() {
        return mDefaultValue;
    }

    @Override
    void cacheToDisk() {
        boolean value = ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                getFeatureName(), getParameterName(), getDefaultValue());
        ChromeSharedPreferences.getInstance().writeBoolean(getSharedPreferenceKey(), value);
    }

    /**
     * Forces the parameter to return a specific value for testing.
     *
     * Caveat: this does not affect the value returned by native, only by
     * {@link CachedFieldTrialParameter}.
     *
     * @param overrideValue the value to be returned
     */
    public void setForTesting(boolean overrideValue) {
        ValuesOverridden.setOverrideForTesting(
                getSharedPreferenceKey(), String.valueOf(overrideValue));
    }
}
