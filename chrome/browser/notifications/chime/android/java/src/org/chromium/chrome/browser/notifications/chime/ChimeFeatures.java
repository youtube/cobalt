// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications.chime;

import org.chromium.chrome.browser.flags.BooleanCachedFieldTrialParameter;
import org.chromium.chrome.browser.flags.ChromeFeatureList;

/**
 * Chime related features and Finch parameters.
 */
public class ChimeFeatures {
    /**
     * Always register to push notification service.
     */
    public static final BooleanCachedFieldTrialParameter ALWAYS_REGISTER =
            new BooleanCachedFieldTrialParameter(
                    ChromeFeatureList.USE_CHIME_ANDROID_SDK, "always_register", false);
}
