// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.flags;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Flag;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;

import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * CachedFlags are Flags that may be used before native is loaded and the FeatureList is
 * initialized.
 *
 * They return a flag value read from native in a previous run, using SharedPreferences as
 * persistence.
 *
 * @see {@link #isEnabled()} for more details about the logic.
 *
 * To cache a flag from ChromeFeatureList:
 * - Create a static CachedFlag object in {@link ChromeFeatureList} "sMyFlag"
 * - Add it to the list {@link ChromeFeatureList#sFlagsCachedFullBrowser}
 * - Call {@code ChromeFeatureList.sMyFlag.isEnabled()} to query whether the cached flag is enabled.
 *   Consider this the source of truth for whether the flag is turned on in the current session.
 *
 * Metrics caveat: For cached flags that are queried before native is initialized, when a new
 * experiment configuration is received the metrics reporting system will record metrics as if the
 * experiment is enabled despite the experimental behavior not yet taking effect. This will be
 * remedied on the next process restart.
 */
public class CachedFlag extends Flag {
    private final boolean mDefaultValue;
    @Nullable
    private final String mLegacySharedPreferenceKey;

    public CachedFlag(String featureName, boolean defaultValue) {
        super(featureName);
        mLegacySharedPreferenceKey = null;
        mDefaultValue = defaultValue;
    }

    /**
     * @deprecated This is the constructor for legacy CachedFlags that predate unifying them
     * under a SharedPreferences prefix.
     *
     * TODO(crbug.com/1446352): After Oct/2023, remove this constructor and migrate all usages to
     * the constructor without |sharedPreferenceKey|.
     */
    @Deprecated
    public CachedFlag(String featureName, String sharedPreferenceKey, boolean defaultValue) {
        super(featureName);
        mLegacySharedPreferenceKey = sharedPreferenceKey;
        mDefaultValue = defaultValue;
    }

    /**
     * Rules from highest to lowest priority:
     * 1. If the flag has been forced by @EnableFeatures/@DisableFeatures or
     *    {@link CachedFlag#setForTesting}, the forced value is returned.
     * 2. If a value was previously returned in the same run, the same value is returned for
     *    consistency.
     * 3. If native is loaded, the value from {@link ChromeFeatureList} is returned.
     * 4. If in a previous run, the value from {@link ChromeFeatureList} was cached to SharedPrefs,
     *    it is returned.
     * 5. The default value passed as a parameter is returned.
     */
    @Override
    public boolean isEnabled() {
        CachedFlagsSafeMode.getInstance().onFlagChecked();

        String preferenceName = getSharedPreferenceKey();

        Boolean flag;
        synchronized (ValuesReturned.sBoolValues) {
            Map<String, Boolean> boolValuesReturned = ValuesReturned.sBoolValues;
            flag = boolValuesReturned.get(preferenceName);
            if (flag != null) {
                return flag;
            }

            flag = CachedFlagsSafeMode.getInstance().isEnabled(
                    mFeatureName, preferenceName, mDefaultValue);
            if (flag == null) {
                SharedPreferencesManager prefs = ChromeSharedPreferences.getInstance();
                if (prefs.contains(preferenceName)) {
                    flag = prefs.readBoolean(preferenceName, false);
                } else {
                    flag = mDefaultValue;
                }
            }

            boolValuesReturned.put(preferenceName, flag);
        }
        return flag;
    }

    /**
     * @return the default value to be returned if no value is cached.
     */
    public boolean getDefaultValue() {
        return mDefaultValue;
    }

    @Override
    protected void clearInMemoryCachedValueForTesting() {
        // ValuesReturned is cleared by CachedFeatureFlags#resetFlagsForTesting().
    }

    /**
     * Forces a feature to be enabled or disabled for testing.
     *
     * @deprecated do not call this from tests; use @EnableFeatures/@DisableFeatures instead,
     * since batched tests need to be split by feature flag configuration.
     */
    @VisibleForTesting
    @Deprecated
    public void setForTesting(@Nullable Boolean value) {
        setValueReturnedForTesting(value);
    }

    private void setValueReturnedForTesting(@Nullable Boolean value) {
        synchronized (ValuesReturned.sBoolValues) {
            ValuesReturned.sBoolValues.put(getSharedPreferenceKey(), value);
        }
    }

    /**
     * Caches the value of the feature from {@link ChromeFeatureList} to SharedPrefs.
     */
    void cacheFeature() {
        boolean isEnabledInNative = ChromeFeatureList.isEnabled(mFeatureName);

        ChromeSharedPreferences.getInstance().writeBoolean(
                getNewSharedPreferenceKey(), isEnabledInNative);

        // TODO(crbug.com/1446352): After Oct/2023, stop writing the legacy SharedPreferences key.
        if (mLegacySharedPreferenceKey != null) {
            ChromeSharedPreferences.getInstance().writeBoolean(
                    mLegacySharedPreferenceKey, isEnabledInNative);
        }
    }

    private String getNewSharedPreferenceKey() {
        return ChromePreferenceKeys.FLAGS_CACHED.createKey(mFeatureName);
    }

    String getSharedPreferenceKey() {
        // TODO(crbug.com/1446352): After Oct/2023, return the new SharedPreferences key and stop
        // returning legacy keys.
        if (mLegacySharedPreferenceKey != null) {
            return mLegacySharedPreferenceKey;
        } else {
            return getNewSharedPreferenceKey();
        }
    }

    /**
     * Sets the feature flags to use in JUnit and instrumentation tests.
     *
     * @deprecated Do not call this from tests; use @EnableFeatures/@DisableFeatures annotations
     * instead.
     */
    @Deprecated
    public static void setFeaturesForTesting(Map<String, Boolean> features) {
        for (Map.Entry<String, Boolean> entry : features.entrySet()) {
            CachedFlag possibleCachedFlag = ChromeFeatureList.sAllCachedFlags.get(entry.getKey());
            if (possibleCachedFlag != null) {
                possibleCachedFlag.setValueReturnedForTesting(entry.getValue());
            }
        }
    }

    public static void resetDiskForTesting() {
        ChromeSharedPreferences.getInstance().removeKeysWithPrefix(
                ChromePreferenceKeys.FLAGS_CACHED);

        // TODO(crbug.com/1446352): After Oct/2023, remove this since all legacy SharedPreferences
        // keys will have been migrated to prefixed keys.
        for (Map.Entry<String, CachedFlag> e : ChromeFeatureList.sAllCachedFlags.entrySet()) {
            String legacyPreferenceKey = e.getValue().mLegacySharedPreferenceKey;
            if (legacyPreferenceKey != null) {
                ChromeSharedPreferences.getInstance().removeKey(legacyPreferenceKey);
            }
        }
    }

    /**
     * Create a Map of feature names -> {@link CachedFlag} from multiple lists of CachedFlags.
     */
    public static Map<String, CachedFlag> createCachedFlagMap(
            List<List<CachedFlag>> allCachedFlagsLists) {
        HashMap<String, CachedFlag> cachedFlagMap = new HashMap<>();
        for (List<CachedFlag> cachedFlagsList : allCachedFlagsLists) {
            for (CachedFlag cachedFlag : cachedFlagsList) {
                cachedFlagMap.put(cachedFlag.getFeatureName(), cachedFlag);
            }
        }
        return cachedFlagMap;
    }
}
