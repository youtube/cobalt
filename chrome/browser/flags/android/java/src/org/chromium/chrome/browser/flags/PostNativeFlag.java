// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.flags;

import org.chromium.base.FeatureList;
import org.chromium.base.Flag;

/**
 * Flags of this type assume native is loaded and the value can be retrieved directly from native.
 */
public class PostNativeFlag extends Flag {
    private Boolean mInMemoryCachedValue;
    public PostNativeFlag(String featureName) {
        super(featureName);
    }

    @Override
    public boolean isEnabled() {
        if (mInMemoryCachedValue != null) return mInMemoryCachedValue;

        if (FeatureList.hasTestFeature(mFeatureName)) {
            return ChromeFeatureList.isEnabled(mFeatureName);
        }

        mInMemoryCachedValue = ChromeFeatureList.isEnabled(mFeatureName);
        return mInMemoryCachedValue;
    }

    @Override
    protected void clearInMemoryCachedValueForTesting() {
        mInMemoryCachedValue = null;
    }
}
