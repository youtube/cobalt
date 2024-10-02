// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.flags;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.FeatureList;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;

/**
 * Tests the behavior of {@link ChromeFeatureList} in Robolectric unit tests when the rule
 * Features.JUnitProcessor is present.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ChromeFeatureListWithProcessorUnitTest {
    @Rule
    public TestRule mFeaturesProcessorRule = new Features.JUnitProcessor();

    /**
     * In unit tests, all flags checked must have their value specified.
     */
    @Test(expected = IllegalArgumentException.class)
    public void testNoOverridesDefaultDisabled_throws() {
        ChromeFeatureList.isEnabled(ChromeFeatureList.TEST_DEFAULT_DISABLED);
    }

    /**
     * In unit tests, all flags checked must have their value specified.
     */
    @Test(expected = IllegalArgumentException.class)
    public void testNoOverridesDefaultEnabled_throws() {
        ChromeFeatureList.isEnabled(ChromeFeatureList.TEST_DEFAULT_ENABLED);
    }

    /**
     * In unit tests, flags may have their value specified by the EnableFeatures annotation.
     */
    @Test
    @EnableFeatures(ChromeFeatureList.TEST_DEFAULT_DISABLED)
    public void testAnnotationEnabled_returnsEnabled() {
        assertTrue(ChromeFeatureList.isEnabled(ChromeFeatureList.TEST_DEFAULT_DISABLED));
    }

    /**
     * In unit tests, flags may have their value specified by the DisableFeatures annotation.
     */
    @Test
    @DisableFeatures(ChromeFeatureList.TEST_DEFAULT_ENABLED)
    public void testAnnotationDisabled_returnsDisabled() {
        assertFalse(ChromeFeatureList.isEnabled(ChromeFeatureList.TEST_DEFAULT_ENABLED));
    }

    /**
     * In unit tests, flags may have their value specified by calling
     * {@link FeatureList#setTestFeatures(java.util.Map)}.
     */
    @Test
    @EnableFeatures(ChromeFeatureList.TEST_DEFAULT_DISABLED)
    public void testSetTestFeaturesEnabled_returnsEnabled() {
        assertTrue(ChromeFeatureList.isEnabled(ChromeFeatureList.TEST_DEFAULT_DISABLED));
    }

    /**
     * In unit tests, flags may have their value specified by calling
     * {@link FeatureList#setTestFeatures(java.util.Map)}.
     */
    @Test
    @DisableFeatures(ChromeFeatureList.TEST_DEFAULT_ENABLED)
    public void testSetTestFeaturesDisabled_returnsDisabled() {
        assertFalse(ChromeFeatureList.isEnabled(ChromeFeatureList.TEST_DEFAULT_ENABLED));
    }
}
