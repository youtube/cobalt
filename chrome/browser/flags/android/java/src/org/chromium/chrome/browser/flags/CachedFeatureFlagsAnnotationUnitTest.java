// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.flags;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.test.util.browser.Features;

/**
 * Unit tests to verify @Features.EnableFeatures() and @Features.DisableFeatures() work for
 * {@link CachedFeatureFlags}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Features.EnableFeatures(ChromeFeatureList.TEST_DEFAULT_DISABLED)
@Config(manifest = Config.NONE)
public class CachedFeatureFlagsAnnotationUnitTest {
    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();

    @Test
    public void testDefaultFeatureValue() {
        Assert.assertTrue(ChromeFeatureList.sTestDefaultEnabled.isEnabled());
    }

    @Test
    public void testFeatureAnnotationOnTestSuiteClass() {
        Assert.assertTrue(ChromeFeatureList.sTestDefaultDisabled.isEnabled());
    }

    @Test
    // clang-format off
    @Features.DisableFeatures({ChromeFeatureList.TEST_DEFAULT_DISABLED,
            ChromeFeatureList.TEST_DEFAULT_ENABLED})
    public void testFeatureAnnotationOnMethod() {
        // clang-format on
        Assert.assertFalse(ChromeFeatureList.sTestDefaultDisabled.isEnabled());
        Assert.assertFalse(ChromeFeatureList.sTestDefaultEnabled.isEnabled());
    }
}
