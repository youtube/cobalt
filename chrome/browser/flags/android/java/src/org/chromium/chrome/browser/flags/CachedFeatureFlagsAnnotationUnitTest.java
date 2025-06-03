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
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;

/** Unit tests to verify @EnableFeatures() and @DisableFeatures() work for {@link CachedFlag}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures(ChromeFeatureList.TEST_DEFAULT_DISABLED)
@Config(manifest = Config.NONE)
public class CachedFeatureFlagsAnnotationUnitTest {
    @Rule public TestRule mProcessor = new Features.JUnitProcessor();

    @Test
    public void testDefaultFeatureValue() {
        Assert.assertTrue(ChromeFeatureList.sTestDefaultEnabled.isEnabled());
    }

    @Test
    public void testFeatureAnnotationOnTestSuiteClass() {
        Assert.assertTrue(ChromeFeatureList.sTestDefaultDisabled.isEnabled());
    }

    @Test
    @DisableFeatures({
        ChromeFeatureList.TEST_DEFAULT_DISABLED,
        ChromeFeatureList.TEST_DEFAULT_ENABLED
    })
    public void testFeatureAnnotationOnMethod() {
        Assert.assertFalse(ChromeFeatureList.sTestDefaultDisabled.isEnabled());
        Assert.assertFalse(ChromeFeatureList.sTestDefaultEnabled.isEnabled());
    }
}
