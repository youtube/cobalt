// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.magic_stack;

import static org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType.DEFAULT_BROWSER_PROMO;

import android.content.Context;

import org.chromium.base.ObserverList;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.settings.SettingsNavigationFactory;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Map.Entry;
import java.util.Set;

/**
 * Provides information regarding chrome home modules enabled states.
 *
 * <p>This class serves as a single chrome home modules setting logic gateway.
 */
@NullMarked
public class HomeModulesConfigManager {
    /** An interface to use for getting home modules related updates. */
    interface HomeModulesStateListener {
        /** Called when the home modules' specific module type is disabled or enabled. */
        void onModuleConfigChanged(@ModuleType int moduleType, boolean isEnabled);
    }

    private final SharedPreferencesManager mSharedPreferencesManager;
    private final ObserverList<HomeModulesStateListener> mHomepageStateListeners;

    /** A map of <ModuleType, ModuleEligibilityChecker>. */
    private final Map<Integer, ModuleConfigChecker> mModuleConfigCheckerMap;

    /** Static class that implements the initialization-on-demand holder idiom. */
    private static class LazyHolder {
        static HomeModulesConfigManager sInstance = new HomeModulesConfigManager();
    }

    /** Returns the singleton instance of HomeModulesConfigManager. */
    public static HomeModulesConfigManager getInstance() {
        return LazyHolder.sInstance;
    }

    private HomeModulesConfigManager() {
        mSharedPreferencesManager = ChromeSharedPreferences.getInstance();
        mHomepageStateListeners = new ObserverList<>();
        mModuleConfigCheckerMap = new HashMap<>();
    }

    public void registerModuleEligibilityChecker(
            @ModuleType int moduleType, ModuleConfigChecker eligibilityChecker) {
        mModuleConfigCheckerMap.put(moduleType, eligibilityChecker);
    }

    /**
     * Adds a {@link HomeModulesStateListener} to receive updates when the home modules state
     * changes.
     */
    void addListener(HomeModulesStateListener listener) {
        mHomepageStateListeners.addObserver(listener);
    }

    /**
     * Removes the given listener from the state listener list.
     *
     * @param listener The listener to remove.
     */
    void removeListener(HomeModulesStateListener listener) {
        mHomepageStateListeners.removeObserver(listener);
    }

    /**
     * Menu click handler on customize button.
     *
     * @param context {@link Context} used for launching a settings activity.
     */
    public void onMenuClick(Context context) {
        SettingsNavigationFactory.createSettingsNavigation()
                .startSettings(context, HomeModulesConfigSettings.class);
    }

    /**
     * Notifies any listeners about a chrome home modules state change.
     *
     * @param moduleType {@link ModuleType} needed to be notified to the listeners.
     * @param enabled True is the module type is enabled.
     */
    private void notifyModuleTypeUpdated(@ModuleType int moduleType, boolean enabled) {
        for (HomeModulesStateListener listener : mHomepageStateListeners) {
            listener.onModuleConfigChanged(moduleType, enabled);
        }
    }

    /**
     * Returns the user preference for whether the given module type is enabled.
     *
     * @param moduleType {@link ModuleType} needed to be notified to the listeners.
     */
    public boolean getPrefModuleTypeEnabled(@ModuleType int moduleType) {
        return mSharedPreferencesManager.readBoolean(getSettingsPreferenceKey(moduleType), true);
    }

    /**
     * Sets the user preference for whether the given module type is enabled.
     *
     * @param moduleType {@link ModuleType} needed to be notified to the listeners.
     * @param enabled True is the module type is enabled.
     */
    public void setPrefModuleTypeEnabled(@ModuleType int moduleType, boolean enabled) {
        mSharedPreferencesManager.writeBoolean(getSettingsPreferenceKey(moduleType), enabled);
        notifyModuleTypeUpdated(moduleType, enabled);
    }

    /**
     * Returns the set which contains all the module types that are registered and enabled according
     * to user preference. Note: this function should be called after profile is ready.
     */
    @ModuleType
    public Set<Integer> getEnabledModuleSet() {
        @ModuleType Set<Integer> enabledModuleList = new HashSet<>();
        for (Entry<Integer, ModuleConfigChecker> entry : mModuleConfigCheckerMap.entrySet()) {
            ModuleConfigChecker configChecker = entry.getValue();
            if (configChecker.isEligible() && getPrefModuleTypeEnabled(entry.getKey())) {
                enabledModuleList.add(entry.getKey());
            }
        }
        return enabledModuleList;
    }

    /** Returns a list of modules that allow users to configure in settings. */
    @ModuleType
    public List<Integer> getModuleListShownInSettings() {
        @ModuleType List<Integer> moduleListShownInSettings = new ArrayList<>();
        boolean isEducationalTipModuleAdded = false;

        for (Entry<Integer, ModuleConfigChecker> entry : mModuleConfigCheckerMap.entrySet()) {
            ModuleConfigChecker configChecker = entry.getValue();
            if (configChecker.isEligible()) {
                int moduleType = entry.getKey();
                if (HomeModulesUtils.belongsToEducationalTipModule(moduleType)) {
                    // All the educational tip modules are controlled by the same preference.
                    if (isEducationalTipModuleAdded) continue;

                    isEducationalTipModuleAdded = true;
                }

                moduleListShownInSettings.add(moduleType);
            }
        }
        return moduleListShownInSettings;
    }

    /** Returns whether it has any module to configure in settings. */
    public boolean hasModuleShownInSettings() {
        for (ModuleConfigChecker moduleConfigChecker : mModuleConfigCheckerMap.values()) {
            if (moduleConfigChecker.isEligible()) {
                return true;
            }
        }
        return false;
    }

    /** Returns the preference key of the module type. */
    String getSettingsPreferenceKey(@ModuleType int moduleType) {
        assert 0 <= moduleType && moduleType < ModuleType.NUM_ENTRIES;

        // All the educational tip modules are controlled by the same preference key.
        if (HomeModulesUtils.belongsToEducationalTipModule(moduleType)) {
            return ChromePreferenceKeys.HOME_MODULES_MODULE_TYPE.createKey(
                    String.valueOf(DEFAULT_BROWSER_PROMO));
        }

        return ChromePreferenceKeys.HOME_MODULES_MODULE_TYPE.createKey(String.valueOf(moduleType));
    }

    /** Sets a mocked instance for testing. */
    public static void setInstanceForTesting(HomeModulesConfigManager instance) {
        var oldValue = LazyHolder.sInstance;
        LazyHolder.sInstance = instance;
        ResettersForTesting.register(() -> LazyHolder.sInstance = oldValue);
    }

    public void cleanupForTesting() {
        mModuleConfigCheckerMap.clear();
    }
}
