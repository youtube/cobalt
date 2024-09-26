// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.MockitoAnnotations;

import org.chromium.base.BaseSwitches;
import org.chromium.base.ContextUtils;
import org.chromium.base.SysUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.device.DeviceClassManager;
import org.chromium.chrome.browser.flags.CachedFeatureFlags;
import org.chromium.chrome.browser.flags.CachedFlag;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.util.ChromeAccessibilityUtil;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.Arrays;
import java.util.List;

/**
 * Unit Tests for {@link TabUiFeatureUtilities}.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class TabUiFeatureUtilitiesUnitTest {
    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();

    private void setAccessibilityEnabledForTesting(Boolean value) {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> ChromeAccessibilityUtil.get().setAccessibilityEnabledForTesting(value));
    }

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        setAccessibilityEnabledForTesting(false);
        CachedFeatureFlags.resetFlagsForTesting();
    }

    @After
    public void tearDown() {
        CachedFeatureFlags.resetFlagsForTesting();
        setAccessibilityEnabledForTesting(null);
        DeviceClassManager.resetForTesting();
        SysUtils.resetForTesting();
    }

    private void cacheFeatureFlags() {
        List<CachedFlag> featuresToCache = Arrays.asList(ChromeFeatureList.sTabGridLayoutAndroid,
                ChromeFeatureList.sTabGroupsAndroid,
                ChromeFeatureList.sTabGroupsContinuationAndroid);
        CachedFeatureFlags.cacheNativeFlags(featuresToCache);
    }

    @Test
    // clang-format off
    @Features.DisableFeatures({ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID,
                                ChromeFeatureList.TAB_GROUPS_ANDROID,
                                ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID})
    @CommandLineFlags.Add({BaseSwitches.ENABLE_LOW_END_DEVICE_MODE})
    public void testCacheGridTabSwitcher_LowEnd_NoEnabledFlags() {
        // clang-format on
        cacheFeatureFlags();

        CachedFeatureFlags.resetFlagsForTesting();
        assertFalse(TabUiFeatureUtilities.isGridTabSwitcherEnabled(
                ContextUtils.getApplicationContext()));
        assertFalse(TabUiFeatureUtilities.isTabGroupsAndroidEnabled(
                ContextUtils.getApplicationContext()));
        assertFalse(TabUiFeatureUtilities.shouldUseListMode(ContextUtils.getApplicationContext()));
        assertTrue(TabUiFeatureUtilities.isTabToGtsAnimationEnabled(
                ContextUtils.getApplicationContext()));

        setAccessibilityEnabledForTesting(true);
        DeviceClassManager.resetForTesting();
        cacheFeatureFlags();

        CachedFeatureFlags.resetFlagsForTesting();
        assertFalse(TabUiFeatureUtilities.isGridTabSwitcherEnabled(
                ContextUtils.getApplicationContext()));
        assertFalse(TabUiFeatureUtilities.isTabGroupsAndroidEnabled(
                ContextUtils.getApplicationContext()));
        assertFalse(TabUiFeatureUtilities.shouldUseListMode(ContextUtils.getApplicationContext()));
        assertTrue(TabUiFeatureUtilities.isTabToGtsAnimationEnabled(
                ContextUtils.getApplicationContext()));
    }

    @Test
    // clang-format off
    @Features.EnableFeatures({ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID})
    @Features.DisableFeatures({ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID,
                                ChromeFeatureList.TAB_GROUPS_ANDROID})
    @CommandLineFlags.Add({BaseSwitches.DISABLE_LOW_END_DEVICE_MODE})
    public void testCacheGridTabSwitcher_HighEnd_Layout() {
        // clang-format on
        cacheFeatureFlags();

        CachedFeatureFlags.resetFlagsForTesting();
        assertTrue(TabUiFeatureUtilities.isGridTabSwitcherEnabled(
                ContextUtils.getApplicationContext()));
        assertFalse(TabUiFeatureUtilities.isTabGroupsAndroidEnabled(
                ContextUtils.getApplicationContext()));
        assertFalse(TabUiFeatureUtilities.isTabGroupsAndroidContinuationEnabled(
                ContextUtils.getApplicationContext()));
        assertFalse(TabUiFeatureUtilities.shouldUseListMode(ContextUtils.getApplicationContext()));
        assertTrue(TabUiFeatureUtilities.isTabToGtsAnimationEnabled(
                ContextUtils.getApplicationContext()));

        setAccessibilityEnabledForTesting(true);
        DeviceClassManager.resetForTesting();
        cacheFeatureFlags();

        CachedFeatureFlags.resetFlagsForTesting();
        assertFalse(TabUiFeatureUtilities.isGridTabSwitcherEnabled(
                ContextUtils.getApplicationContext()));
        assertFalse(TabUiFeatureUtilities.isTabGroupsAndroidEnabled(
                ContextUtils.getApplicationContext()));
        assertFalse(TabUiFeatureUtilities.isTabGroupsAndroidContinuationEnabled(
                ContextUtils.getApplicationContext()));
        assertFalse(TabUiFeatureUtilities.shouldUseListMode(ContextUtils.getApplicationContext()));
        assertTrue(TabUiFeatureUtilities.isTabToGtsAnimationEnabled(
                ContextUtils.getApplicationContext()));
    }

    @Test
    // clang-format off
    @Features.EnableFeatures({ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID})
    @Features.DisableFeatures({ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID,
                                ChromeFeatureList.TAB_GROUPS_ANDROID})
    @CommandLineFlags.Add({BaseSwitches.ENABLE_LOW_END_DEVICE_MODE})
    public void testCacheGridTabSwitcher_LowEnd_Layout() {
        // clang-format on
        cacheFeatureFlags();

        CachedFeatureFlags.resetFlagsForTesting();
        assertFalse(TabUiFeatureUtilities.isGridTabSwitcherEnabled(
                ContextUtils.getApplicationContext()));
        assertFalse(TabUiFeatureUtilities.isTabGroupsAndroidEnabled(
                ContextUtils.getApplicationContext()));
        assertFalse(TabUiFeatureUtilities.isTabGroupsAndroidContinuationEnabled(
                ContextUtils.getApplicationContext()));
        assertFalse(TabUiFeatureUtilities.shouldUseListMode(ContextUtils.getApplicationContext()));
        assertTrue(TabUiFeatureUtilities.isTabToGtsAnimationEnabled(
                ContextUtils.getApplicationContext()));

        setAccessibilityEnabledForTesting(true);
        DeviceClassManager.resetForTesting();
        cacheFeatureFlags();

        CachedFeatureFlags.resetFlagsForTesting();
        assertFalse(TabUiFeatureUtilities.isGridTabSwitcherEnabled(
                ContextUtils.getApplicationContext()));
        assertFalse(TabUiFeatureUtilities.isTabGroupsAndroidEnabled(
                ContextUtils.getApplicationContext()));
        assertFalse(TabUiFeatureUtilities.isTabGroupsAndroidContinuationEnabled(
                ContextUtils.getApplicationContext()));
        assertFalse(TabUiFeatureUtilities.shouldUseListMode(ContextUtils.getApplicationContext()));
        assertTrue(TabUiFeatureUtilities.isTabToGtsAnimationEnabled(
                ContextUtils.getApplicationContext()));
    }

    @Test
    // clang-format off
    @Features.EnableFeatures({ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID,
                                ChromeFeatureList.TAB_GROUPS_ANDROID})
    @Features.DisableFeatures({ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID})
    @CommandLineFlags.Add({BaseSwitches.DISABLE_LOW_END_DEVICE_MODE})
    public void testCacheGridTabSwitcher_HighEnd_LayoutGroup() {
        // clang-format on
        cacheFeatureFlags();

        CachedFeatureFlags.resetFlagsForTesting();
        assertTrue(TabUiFeatureUtilities.isGridTabSwitcherEnabled(
                ContextUtils.getApplicationContext()));
        assertTrue(TabUiFeatureUtilities.isTabGroupsAndroidEnabled(
                ContextUtils.getApplicationContext()));
        assertFalse(TabUiFeatureUtilities.isTabGroupsAndroidContinuationEnabled(
                ContextUtils.getApplicationContext()));
        assertFalse(TabUiFeatureUtilities.shouldUseListMode(ContextUtils.getApplicationContext()));
        assertTrue(TabUiFeatureUtilities.isTabToGtsAnimationEnabled(
                ContextUtils.getApplicationContext()));

        setAccessibilityEnabledForTesting(true);
        DeviceClassManager.resetForTesting();
        cacheFeatureFlags();

        CachedFeatureFlags.resetFlagsForTesting();
        assertFalse(TabUiFeatureUtilities.isGridTabSwitcherEnabled(
                ContextUtils.getApplicationContext()));
        assertFalse(TabUiFeatureUtilities.isTabGroupsAndroidEnabled(
                ContextUtils.getApplicationContext()));
        assertFalse(TabUiFeatureUtilities.isTabGroupsAndroidContinuationEnabled(
                ContextUtils.getApplicationContext()));
        assertFalse(TabUiFeatureUtilities.shouldUseListMode(ContextUtils.getApplicationContext()));
        assertTrue(TabUiFeatureUtilities.isTabToGtsAnimationEnabled(
                ContextUtils.getApplicationContext()));
    }

    @Test
    // clang-format off
    @Features.EnableFeatures({ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID,
                                ChromeFeatureList.TAB_GROUPS_ANDROID})
    @Features.DisableFeatures({ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID})
    @CommandLineFlags.Add({BaseSwitches.ENABLE_LOW_END_DEVICE_MODE})
    public void testCacheGridTabSwitcher_LowEnd_LayoutGroup() {
        // clang-format on
        cacheFeatureFlags();

        CachedFeatureFlags.resetFlagsForTesting();
        assertFalse(TabUiFeatureUtilities.isGridTabSwitcherEnabled(
                ContextUtils.getApplicationContext()));
        assertFalse(TabUiFeatureUtilities.isTabGroupsAndroidEnabled(
                ContextUtils.getApplicationContext()));
        assertFalse(TabUiFeatureUtilities.isTabGroupsAndroidContinuationEnabled(
                ContextUtils.getApplicationContext()));
        assertFalse(TabUiFeatureUtilities.shouldUseListMode(ContextUtils.getApplicationContext()));
        assertTrue(TabUiFeatureUtilities.isTabToGtsAnimationEnabled(
                ContextUtils.getApplicationContext()));

        setAccessibilityEnabledForTesting(true);
        DeviceClassManager.resetForTesting();
        cacheFeatureFlags();

        assertFalse(TabUiFeatureUtilities.isGridTabSwitcherEnabled(
                ContextUtils.getApplicationContext()));
        assertFalse(TabUiFeatureUtilities.isTabGroupsAndroidEnabled(
                ContextUtils.getApplicationContext()));
        assertFalse(TabUiFeatureUtilities.isTabGroupsAndroidContinuationEnabled(
                ContextUtils.getApplicationContext()));
        assertFalse(TabUiFeatureUtilities.shouldUseListMode(ContextUtils.getApplicationContext()));
        assertTrue(TabUiFeatureUtilities.isTabToGtsAnimationEnabled(
                ContextUtils.getApplicationContext()));
    }

    @Test
    // clang-format off
    @Features.EnableFeatures({ChromeFeatureList.TAB_GROUPS_ANDROID})
    @Features.DisableFeatures({ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID,
                                ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID})
    @CommandLineFlags.Add({BaseSwitches.DISABLE_LOW_END_DEVICE_MODE})
    public void testCacheGridTabSwitcher_HighEnd_Group() {
        // clang-format on
        cacheFeatureFlags();

        CachedFeatureFlags.resetFlagsForTesting();
        assertTrue(TabUiFeatureUtilities.isGridTabSwitcherEnabled(
                ContextUtils.getApplicationContext()));
        assertTrue(TabUiFeatureUtilities.isTabGroupsAndroidEnabled(
                ContextUtils.getApplicationContext()));
        assertFalse(TabUiFeatureUtilities.isTabGroupsAndroidContinuationEnabled(
                ContextUtils.getApplicationContext()));
        assertFalse(TabUiFeatureUtilities.shouldUseListMode(ContextUtils.getApplicationContext()));
        assertTrue(TabUiFeatureUtilities.isTabToGtsAnimationEnabled(
                ContextUtils.getApplicationContext()));

        setAccessibilityEnabledForTesting(true);
        DeviceClassManager.resetForTesting();
        cacheFeatureFlags();

        assertFalse(TabUiFeatureUtilities.isGridTabSwitcherEnabled(
                ContextUtils.getApplicationContext()));
        assertFalse(TabUiFeatureUtilities.isTabGroupsAndroidEnabled(
                ContextUtils.getApplicationContext()));
        assertFalse(TabUiFeatureUtilities.isTabGroupsAndroidContinuationEnabled(
                ContextUtils.getApplicationContext()));
        assertFalse(TabUiFeatureUtilities.shouldUseListMode(ContextUtils.getApplicationContext()));
        assertTrue(TabUiFeatureUtilities.isTabToGtsAnimationEnabled(
                ContextUtils.getApplicationContext()));
    }

    @Test
    // clang-format off
    @Features.EnableFeatures({ChromeFeatureList.TAB_GROUPS_ANDROID})
    @Features.DisableFeatures({ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID,
                                ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID})
    @CommandLineFlags.Add({BaseSwitches.ENABLE_LOW_END_DEVICE_MODE})
    public void testCacheGridTabSwitcher_LowEnd_Group() {
        // clang-format on
        cacheFeatureFlags();

        CachedFeatureFlags.resetFlagsForTesting();
        assertFalse(TabUiFeatureUtilities.isGridTabSwitcherEnabled(
                ContextUtils.getApplicationContext()));
        assertFalse(TabUiFeatureUtilities.isTabGroupsAndroidEnabled(
                ContextUtils.getApplicationContext()));
        assertFalse(TabUiFeatureUtilities.isTabGroupsAndroidContinuationEnabled(
                ContextUtils.getApplicationContext()));
        assertFalse(TabUiFeatureUtilities.shouldUseListMode(ContextUtils.getApplicationContext()));
        assertTrue(TabUiFeatureUtilities.isTabToGtsAnimationEnabled(
                ContextUtils.getApplicationContext()));

        setAccessibilityEnabledForTesting(true);
        DeviceClassManager.resetForTesting();
        cacheFeatureFlags();

        assertFalse(TabUiFeatureUtilities.isGridTabSwitcherEnabled(
                ContextUtils.getApplicationContext()));
        assertFalse(TabUiFeatureUtilities.isTabGroupsAndroidEnabled(
                ContextUtils.getApplicationContext()));
        assertFalse(TabUiFeatureUtilities.isTabGroupsAndroidContinuationEnabled(
                ContextUtils.getApplicationContext()));
        assertFalse(TabUiFeatureUtilities.shouldUseListMode(ContextUtils.getApplicationContext()));
        assertTrue(TabUiFeatureUtilities.isTabToGtsAnimationEnabled(
                ContextUtils.getApplicationContext()));
    }

    @Test
    // clang-format off
    @Features.EnableFeatures({ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID})
    @Features.DisableFeatures({ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID,
                                ChromeFeatureList.TAB_GROUPS_ANDROID})
    @CommandLineFlags.Add({BaseSwitches.ENABLE_LOW_END_DEVICE_MODE})
    public void testCacheGridTabSwitcher_LowEnd_Continuation() {
        // clang-format on
        cacheFeatureFlags();

        CachedFeatureFlags.resetFlagsForTesting();
        DeviceClassManager.GTS_LOW_END_SUPPORT.setForTesting(true);
        DeviceClassManager.GTS_ACCESSIBILITY_SUPPORT.setForTesting(true);
        TabUiFeatureUtilities.GTS_ACCESSIBILITY_LIST_MODE.setForTesting(false);

        assertFalse(TabUiFeatureUtilities.isGridTabSwitcherEnabled(
                ContextUtils.getApplicationContext()));
        assertFalse(TabUiFeatureUtilities.isTabGroupsAndroidEnabled(
                ContextUtils.getApplicationContext()));
        assertFalse(TabUiFeatureUtilities.isTabGroupsAndroidContinuationEnabled(
                ContextUtils.getApplicationContext()));
        assertFalse(TabUiFeatureUtilities.shouldUseListMode(ContextUtils.getApplicationContext()));
        assertTrue(TabUiFeatureUtilities.isTabToGtsAnimationEnabled(
                ContextUtils.getApplicationContext()));

        setAccessibilityEnabledForTesting(true);
        DeviceClassManager.resetForTesting();
        cacheFeatureFlags();

        assertFalse(TabUiFeatureUtilities.isGridTabSwitcherEnabled(
                ContextUtils.getApplicationContext()));
        assertFalse(TabUiFeatureUtilities.isTabGroupsAndroidEnabled(
                ContextUtils.getApplicationContext()));
        assertFalse(TabUiFeatureUtilities.isTabGroupsAndroidContinuationEnabled(
                ContextUtils.getApplicationContext()));
        assertFalse(TabUiFeatureUtilities.shouldUseListMode(ContextUtils.getApplicationContext()));
        assertTrue(TabUiFeatureUtilities.isTabToGtsAnimationEnabled(
                ContextUtils.getApplicationContext()));
    }

    @Test
    // clang-format off
    @Features.EnableFeatures({ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID,
                                ChromeFeatureList.TAB_GROUPS_ANDROID,
                                ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID})
    @CommandLineFlags.Add({BaseSwitches.DISABLE_LOW_END_DEVICE_MODE})
    public void testCacheGridTabSwitcher_HighEnd_AllFlags() {
        // clang-format on
        cacheFeatureFlags();

        CachedFeatureFlags.resetFlagsForTesting();
        DeviceClassManager.GTS_LOW_END_SUPPORT.setForTesting(true);
        DeviceClassManager.GTS_ACCESSIBILITY_SUPPORT.setForTesting(true);
        TabUiFeatureUtilities.GTS_ACCESSIBILITY_LIST_MODE.setForTesting(false);

        assertTrue(TabUiFeatureUtilities.isGridTabSwitcherEnabled(
                ContextUtils.getApplicationContext()));
        assertTrue(TabUiFeatureUtilities.isTabGroupsAndroidEnabled(
                ContextUtils.getApplicationContext()));
        assertTrue(TabUiFeatureUtilities.isTabGroupsAndroidContinuationEnabled(
                ContextUtils.getApplicationContext()));
        assertFalse(TabUiFeatureUtilities.shouldUseListMode(ContextUtils.getApplicationContext()));
        assertTrue(TabUiFeatureUtilities.isTabToGtsAnimationEnabled(
                ContextUtils.getApplicationContext()));

        setAccessibilityEnabledForTesting(true);
        DeviceClassManager.resetForTesting();
        cacheFeatureFlags();

        assertTrue(TabUiFeatureUtilities.isGridTabSwitcherEnabled(
                ContextUtils.getApplicationContext()));
        assertTrue(TabUiFeatureUtilities.isTabGroupsAndroidEnabled(
                ContextUtils.getApplicationContext()));
        assertTrue(TabUiFeatureUtilities.isTabGroupsAndroidContinuationEnabled(
                ContextUtils.getApplicationContext()));
        assertFalse(TabUiFeatureUtilities.shouldUseListMode(ContextUtils.getApplicationContext()));
        assertTrue(TabUiFeatureUtilities.isTabToGtsAnimationEnabled(
                ContextUtils.getApplicationContext()));
    }

    @Test
    // clang-format off
    @Features.EnableFeatures({ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID,
                                ChromeFeatureList.TAB_GROUPS_ANDROID,
                                ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID})
    @CommandLineFlags.Add({BaseSwitches.DISABLE_LOW_END_DEVICE_MODE})
    public void testCacheGridTabSwitcher_HighEnd_AllFlags_AccessibilityListMode() {
        // clang-format on
        cacheFeatureFlags();

        CachedFeatureFlags.resetFlagsForTesting();
        DeviceClassManager.GTS_LOW_END_SUPPORT.setForTesting(true);
        DeviceClassManager.GTS_ACCESSIBILITY_SUPPORT.setForTesting(true);
        TabUiFeatureUtilities.GTS_ACCESSIBILITY_LIST_MODE.setForTesting(true);

        assertTrue(TabUiFeatureUtilities.isGridTabSwitcherEnabled(
                ContextUtils.getApplicationContext()));
        assertTrue(TabUiFeatureUtilities.isTabGroupsAndroidEnabled(
                ContextUtils.getApplicationContext()));
        assertTrue(TabUiFeatureUtilities.isTabGroupsAndroidContinuationEnabled(
                ContextUtils.getApplicationContext()));
        assertFalse(TabUiFeatureUtilities.shouldUseListMode(ContextUtils.getApplicationContext()));
        assertTrue(TabUiFeatureUtilities.isTabToGtsAnimationEnabled(
                ContextUtils.getApplicationContext()));

        setAccessibilityEnabledForTesting(true);
        DeviceClassManager.resetForTesting();
        cacheFeatureFlags();

        assertTrue(TabUiFeatureUtilities.isGridTabSwitcherEnabled(
                ContextUtils.getApplicationContext()));
        assertTrue(TabUiFeatureUtilities.isTabGroupsAndroidEnabled(
                ContextUtils.getApplicationContext()));
        assertTrue(TabUiFeatureUtilities.isTabGroupsAndroidContinuationEnabled(
                ContextUtils.getApplicationContext()));
        assertTrue(TabUiFeatureUtilities.shouldUseListMode(ContextUtils.getApplicationContext()));
        assertFalse(TabUiFeatureUtilities.isTabToGtsAnimationEnabled(
                ContextUtils.getApplicationContext()));
    }

    @Test
    // clang-format off
    @Features.EnableFeatures({ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID,
                                ChromeFeatureList.TAB_GROUPS_ANDROID,
                                ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID})
    @CommandLineFlags.Add({BaseSwitches.ENABLE_LOW_END_DEVICE_MODE})
    public void testCacheGridTabSwitcher_LowEnd_AllFlags() {
        // clang-format on
        cacheFeatureFlags();

        CachedFeatureFlags.resetFlagsForTesting();
        DeviceClassManager.GTS_LOW_END_SUPPORT.setForTesting(true);
        DeviceClassManager.GTS_ACCESSIBILITY_SUPPORT.setForTesting(true);
        TabUiFeatureUtilities.GTS_ACCESSIBILITY_LIST_MODE.setForTesting(true);

        assertTrue(TabUiFeatureUtilities.isGridTabSwitcherEnabled(
                ContextUtils.getApplicationContext()));
        assertTrue(TabUiFeatureUtilities.isTabGroupsAndroidEnabled(
                ContextUtils.getApplicationContext()));
        assertTrue(TabUiFeatureUtilities.isTabGroupsAndroidContinuationEnabled(
                ContextUtils.getApplicationContext()));
        assertTrue(TabUiFeatureUtilities.shouldUseListMode(ContextUtils.getApplicationContext()));
        assertFalse(TabUiFeatureUtilities.isTabToGtsAnimationEnabled(
                ContextUtils.getApplicationContext()));

        setAccessibilityEnabledForTesting(true);
        DeviceClassManager.resetForTesting();
        cacheFeatureFlags();

        assertTrue(TabUiFeatureUtilities.isGridTabSwitcherEnabled(
                ContextUtils.getApplicationContext()));
        assertTrue(TabUiFeatureUtilities.isTabGroupsAndroidEnabled(
                ContextUtils.getApplicationContext()));
        assertTrue(TabUiFeatureUtilities.isTabGroupsAndroidContinuationEnabled(
                ContextUtils.getApplicationContext()));
        assertTrue(TabUiFeatureUtilities.shouldUseListMode(ContextUtils.getApplicationContext()));
        assertFalse(TabUiFeatureUtilities.isTabToGtsAnimationEnabled(
                ContextUtils.getApplicationContext()));
    }

    @Test
    // clang-format off
    @Features.EnableFeatures({ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID,
                                ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID})
    @Features.DisableFeatures({ChromeFeatureList.TAB_GROUPS_ANDROID})
    @CommandLineFlags.Add({BaseSwitches.DISABLE_LOW_END_DEVICE_MODE})
    public void testCacheGridTabSwitcher_HighEnd_LayoutContinuation() {
        // clang-format on
        cacheFeatureFlags();

        CachedFeatureFlags.resetFlagsForTesting();
        DeviceClassManager.GTS_LOW_END_SUPPORT.setForTesting(true);
        DeviceClassManager.GTS_ACCESSIBILITY_SUPPORT.setForTesting(true);
        TabUiFeatureUtilities.GTS_ACCESSIBILITY_LIST_MODE.setForTesting(true);

        assertTrue(TabUiFeatureUtilities.isGridTabSwitcherEnabled(
                ContextUtils.getApplicationContext()));
        assertFalse(TabUiFeatureUtilities.isTabGroupsAndroidEnabled(
                ContextUtils.getApplicationContext()));
        assertFalse(TabUiFeatureUtilities.isTabGroupsAndroidContinuationEnabled(
                ContextUtils.getApplicationContext()));
        assertFalse(TabUiFeatureUtilities.shouldUseListMode(ContextUtils.getApplicationContext()));
        assertTrue(TabUiFeatureUtilities.isTabToGtsAnimationEnabled(
                ContextUtils.getApplicationContext()));

        setAccessibilityEnabledForTesting(true);
        DeviceClassManager.resetForTesting();
        cacheFeatureFlags();

        assertFalse(TabUiFeatureUtilities.isGridTabSwitcherEnabled(
                ContextUtils.getApplicationContext()));
        assertFalse(TabUiFeatureUtilities.isTabGroupsAndroidEnabled(
                ContextUtils.getApplicationContext()));
        assertFalse(TabUiFeatureUtilities.isTabGroupsAndroidContinuationEnabled(
                ContextUtils.getApplicationContext()));
        assertFalse(TabUiFeatureUtilities.shouldUseListMode(ContextUtils.getApplicationContext()));
        assertTrue(TabUiFeatureUtilities.isTabToGtsAnimationEnabled(
                ContextUtils.getApplicationContext()));
    }

    @Test
    // clang-format off
    @Features.EnableFeatures({ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID,
                                ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID})
    @Features.DisableFeatures({ChromeFeatureList.TAB_GROUPS_ANDROID})
    @CommandLineFlags.Add({BaseSwitches.ENABLE_LOW_END_DEVICE_MODE})
    public void testCacheGridTabSwitcher_LowEnd_LayoutContinuation() {
        // clang-format on
        cacheFeatureFlags();

        CachedFeatureFlags.resetFlagsForTesting();
        DeviceClassManager.GTS_LOW_END_SUPPORT.setForTesting(true);
        DeviceClassManager.GTS_ACCESSIBILITY_SUPPORT.setForTesting(true);
        TabUiFeatureUtilities.GTS_ACCESSIBILITY_LIST_MODE.setForTesting(false);

        assertFalse(TabUiFeatureUtilities.isGridTabSwitcherEnabled(
                ContextUtils.getApplicationContext()));
        assertFalse(TabUiFeatureUtilities.isTabGroupsAndroidEnabled(
                ContextUtils.getApplicationContext()));
        assertFalse(TabUiFeatureUtilities.isTabGroupsAndroidContinuationEnabled(
                ContextUtils.getApplicationContext()));
        assertFalse(TabUiFeatureUtilities.shouldUseListMode(ContextUtils.getApplicationContext()));
        assertTrue(TabUiFeatureUtilities.isTabToGtsAnimationEnabled(
                ContextUtils.getApplicationContext()));

        setAccessibilityEnabledForTesting(true);
        DeviceClassManager.resetForTesting();
        cacheFeatureFlags();

        assertFalse(TabUiFeatureUtilities.isGridTabSwitcherEnabled(
                ContextUtils.getApplicationContext()));
        assertFalse(TabUiFeatureUtilities.isTabGroupsAndroidEnabled(
                ContextUtils.getApplicationContext()));
        assertFalse(TabUiFeatureUtilities.isTabGroupsAndroidContinuationEnabled(
                ContextUtils.getApplicationContext()));
        assertFalse(TabUiFeatureUtilities.shouldUseListMode(ContextUtils.getApplicationContext()));
        assertTrue(TabUiFeatureUtilities.isTabToGtsAnimationEnabled(
                ContextUtils.getApplicationContext()));
    }

    @Test
    // clang-format off
    @Features.EnableFeatures({ChromeFeatureList.TAB_GROUPS_ANDROID,
                                ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID})
    @Features.DisableFeatures({ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID})
    @CommandLineFlags.Add({BaseSwitches.DISABLE_LOW_END_DEVICE_MODE})
    public void testCacheGridTabSwitcher_HighEnd_GroupContinuation() {
        // clang-format on
        cacheFeatureFlags();

        CachedFeatureFlags.resetFlagsForTesting();
        DeviceClassManager.GTS_LOW_END_SUPPORT.setForTesting(true);
        DeviceClassManager.GTS_ACCESSIBILITY_SUPPORT.setForTesting(true);
        TabUiFeatureUtilities.GTS_ACCESSIBILITY_LIST_MODE.setForTesting(false);

        assertTrue(TabUiFeatureUtilities.isGridTabSwitcherEnabled(
                ContextUtils.getApplicationContext()));
        assertTrue(TabUiFeatureUtilities.isTabGroupsAndroidEnabled(
                ContextUtils.getApplicationContext()));
        assertTrue(TabUiFeatureUtilities.isTabGroupsAndroidContinuationEnabled(
                ContextUtils.getApplicationContext()));
        assertFalse(TabUiFeatureUtilities.shouldUseListMode(ContextUtils.getApplicationContext()));
        assertTrue(TabUiFeatureUtilities.isTabToGtsAnimationEnabled(
                ContextUtils.getApplicationContext()));

        setAccessibilityEnabledForTesting(true);
        DeviceClassManager.resetForTesting();
        cacheFeatureFlags();

        assertTrue(TabUiFeatureUtilities.isGridTabSwitcherEnabled(
                ContextUtils.getApplicationContext()));
        assertTrue(TabUiFeatureUtilities.isTabGroupsAndroidEnabled(
                ContextUtils.getApplicationContext()));
        assertTrue(TabUiFeatureUtilities.isTabGroupsAndroidContinuationEnabled(
                ContextUtils.getApplicationContext()));
        assertFalse(TabUiFeatureUtilities.shouldUseListMode(ContextUtils.getApplicationContext()));
        assertTrue(TabUiFeatureUtilities.isTabToGtsAnimationEnabled(
                ContextUtils.getApplicationContext()));
    }

    @Test
    // clang-format off
    @Features.EnableFeatures({ChromeFeatureList.TAB_GROUPS_ANDROID,
                                ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID})
    @Features.DisableFeatures({ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID})
    @CommandLineFlags.Add({BaseSwitches.DISABLE_LOW_END_DEVICE_MODE})
    public void testCacheGridTabSwitcher_HighEnd_GroupContinuation_NoAccessibility() {
        // clang-format on
        cacheFeatureFlags();

        CachedFeatureFlags.resetFlagsForTesting();
        DeviceClassManager.GTS_LOW_END_SUPPORT.setForTesting(true);
        DeviceClassManager.GTS_ACCESSIBILITY_SUPPORT.setForTesting(false);
        TabUiFeatureUtilities.GTS_ACCESSIBILITY_LIST_MODE.setForTesting(true);

        assertTrue(TabUiFeatureUtilities.isGridTabSwitcherEnabled(
                ContextUtils.getApplicationContext()));
        assertTrue(TabUiFeatureUtilities.isTabGroupsAndroidEnabled(
                ContextUtils.getApplicationContext()));
        assertTrue(TabUiFeatureUtilities.isTabGroupsAndroidContinuationEnabled(
                ContextUtils.getApplicationContext()));
        assertFalse(TabUiFeatureUtilities.shouldUseListMode(ContextUtils.getApplicationContext()));
        assertTrue(TabUiFeatureUtilities.isTabToGtsAnimationEnabled(
                ContextUtils.getApplicationContext()));

        setAccessibilityEnabledForTesting(true);
        DeviceClassManager.resetForTesting();
        cacheFeatureFlags();

        assertFalse(TabUiFeatureUtilities.isGridTabSwitcherEnabled(
                ContextUtils.getApplicationContext()));
        assertFalse(TabUiFeatureUtilities.isTabGroupsAndroidEnabled(
                ContextUtils.getApplicationContext()));
        assertFalse(TabUiFeatureUtilities.isTabGroupsAndroidContinuationEnabled(
                ContextUtils.getApplicationContext()));
        assertFalse(TabUiFeatureUtilities.shouldUseListMode(ContextUtils.getApplicationContext()));
        assertTrue(TabUiFeatureUtilities.isTabToGtsAnimationEnabled(
                ContextUtils.getApplicationContext()));
    }

    @Test
    // clang-format off
    @Features.EnableFeatures({ChromeFeatureList.TAB_GROUPS_ANDROID,
                                ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID})
    @Features.DisableFeatures({ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID})
    @CommandLineFlags.Add({BaseSwitches.ENABLE_LOW_END_DEVICE_MODE})
    public void testCacheGridTabSwitcher_LowEnd_GroupContinuation() {
        // clang-format on
        cacheFeatureFlags();

        CachedFeatureFlags.resetFlagsForTesting();
        DeviceClassManager.GTS_LOW_END_SUPPORT.setForTesting(true);
        DeviceClassManager.GTS_ACCESSIBILITY_SUPPORT.setForTesting(true);
        TabUiFeatureUtilities.GTS_ACCESSIBILITY_LIST_MODE.setForTesting(false);

        assertTrue(TabUiFeatureUtilities.isGridTabSwitcherEnabled(
                ContextUtils.getApplicationContext()));
        assertTrue(TabUiFeatureUtilities.isTabGroupsAndroidEnabled(
                ContextUtils.getApplicationContext()));
        assertTrue(TabUiFeatureUtilities.isTabGroupsAndroidContinuationEnabled(
                ContextUtils.getApplicationContext()));
        assertTrue(TabUiFeatureUtilities.shouldUseListMode(ContextUtils.getApplicationContext()));
        assertFalse(TabUiFeatureUtilities.isTabToGtsAnimationEnabled(
                ContextUtils.getApplicationContext()));

        setAccessibilityEnabledForTesting(true);
        DeviceClassManager.resetForTesting();
        cacheFeatureFlags();

        assertTrue(TabUiFeatureUtilities.isGridTabSwitcherEnabled(
                ContextUtils.getApplicationContext()));
        assertTrue(TabUiFeatureUtilities.isTabGroupsAndroidEnabled(
                ContextUtils.getApplicationContext()));
        assertTrue(TabUiFeatureUtilities.isTabGroupsAndroidContinuationEnabled(
                ContextUtils.getApplicationContext()));
        assertTrue(TabUiFeatureUtilities.shouldUseListMode(ContextUtils.getApplicationContext()));
        assertFalse(TabUiFeatureUtilities.isTabToGtsAnimationEnabled(
                ContextUtils.getApplicationContext()));
    }

    @Test
    // clang-format off
    @Features.EnableFeatures({ChromeFeatureList.TAB_GROUPS_ANDROID,
                                ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID})
    @Features.DisableFeatures({ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID})
    @CommandLineFlags.Add({BaseSwitches.ENABLE_LOW_END_DEVICE_MODE})
    public void testCacheGridTabSwitcher_LowEnd_GroupContinuation_NoLowEnd() {
        // clang-format on
        cacheFeatureFlags();

        CachedFeatureFlags.resetFlagsForTesting();
        DeviceClassManager.GTS_LOW_END_SUPPORT.setForTesting(false);
        DeviceClassManager.GTS_ACCESSIBILITY_SUPPORT.setForTesting(false);
        TabUiFeatureUtilities.GTS_ACCESSIBILITY_LIST_MODE.setForTesting(false);

        assertFalse(TabUiFeatureUtilities.isGridTabSwitcherEnabled(
                ContextUtils.getApplicationContext()));
        assertFalse(TabUiFeatureUtilities.isTabGroupsAndroidEnabled(
                ContextUtils.getApplicationContext()));
        assertFalse(TabUiFeatureUtilities.isTabGroupsAndroidContinuationEnabled(
                ContextUtils.getApplicationContext()));
        assertFalse(TabUiFeatureUtilities.shouldUseListMode(ContextUtils.getApplicationContext()));
        assertTrue(TabUiFeatureUtilities.isTabToGtsAnimationEnabled(
                ContextUtils.getApplicationContext()));

        setAccessibilityEnabledForTesting(true);
        DeviceClassManager.resetForTesting();
        cacheFeatureFlags();

        assertFalse(TabUiFeatureUtilities.isGridTabSwitcherEnabled(
                ContextUtils.getApplicationContext()));
        assertFalse(TabUiFeatureUtilities.isTabGroupsAndroidEnabled(
                ContextUtils.getApplicationContext()));
        assertFalse(TabUiFeatureUtilities.isTabGroupsAndroidContinuationEnabled(
                ContextUtils.getApplicationContext()));
        assertFalse(TabUiFeatureUtilities.shouldUseListMode(ContextUtils.getApplicationContext()));
        assertTrue(TabUiFeatureUtilities.isTabToGtsAnimationEnabled(
                ContextUtils.getApplicationContext()));
    }

    @Test
    // clang-format off
    @Features.EnableFeatures({ChromeFeatureList.TAB_GROUPS_ANDROID,
            ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID})
    @Features.DisableFeatures({ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID})
    @CommandLineFlags.Add({BaseSwitches.ENABLE_LOW_END_DEVICE_MODE})
    public void testCacheGridAndGroup_LowEnd_enabledThenDisabled_withContinuationFlag() {
        // clang-format on
        cacheFeatureFlags();

        CachedFeatureFlags.resetFlagsForTesting();
        DeviceClassManager.GTS_LOW_END_SUPPORT.setForTesting(true);
        DeviceClassManager.GTS_ACCESSIBILITY_SUPPORT.setForTesting(true);
        TabUiFeatureUtilities.GTS_ACCESSIBILITY_LIST_MODE.setForTesting(false);

        assertTrue(TabUiFeatureUtilities.isGridTabSwitcherEnabled(
                ContextUtils.getApplicationContext()));
        assertTrue(TabUiFeatureUtilities.isTabGroupsAndroidEnabled(
                ContextUtils.getApplicationContext()));
        assertTrue(TabUiFeatureUtilities.isTabGroupsAndroidContinuationEnabled(
                ContextUtils.getApplicationContext()));
        assertTrue(TabUiFeatureUtilities.shouldUseListMode(ContextUtils.getApplicationContext()));
        assertFalse(TabUiFeatureUtilities.isTabToGtsAnimationEnabled(
                ContextUtils.getApplicationContext()));

        CachedFeatureFlags.resetFlagsForTesting();
        // Pretend that we've flipped the continuation flag.
        ChromeFeatureList.sTabGroupsContinuationAndroid.setForTesting(false);
        assertFalse(TabUiFeatureUtilities.isGridTabSwitcherEnabled(
                ContextUtils.getApplicationContext()));
        assertFalse(TabUiFeatureUtilities.isTabGroupsAndroidEnabled(
                ContextUtils.getApplicationContext()));
        assertFalse(TabUiFeatureUtilities.isTabGroupsAndroidContinuationEnabled(
                ContextUtils.getApplicationContext()));
        assertFalse(TabUiFeatureUtilities.shouldUseListMode(ContextUtils.getApplicationContext()));
        assertTrue(TabUiFeatureUtilities.isTabToGtsAnimationEnabled(
                ContextUtils.getApplicationContext()));
    }
}
