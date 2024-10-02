// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_engines;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.search_engines.TemplateUrlService;

/**
 * This factory links the native TemplateURLService for the current Profile to create and hold a
 * {@link TemplateUrlService} singleton.
 */
public class TemplateUrlServiceFactory {
    private static TemplateUrlService sTemplateUrlServiceForTesting;

    private TemplateUrlServiceFactory() {}

    /**
     * Retrieve the TemplateUrlService for a given profile.
     * @param profile The profile associated with the TemplateUrlService.
     * @return The profile specific TemplateUrlService.
     */
    public static TemplateUrlService getForProfile(Profile profile) {
        ThreadUtils.assertOnUiThread();
        if (sTemplateUrlServiceForTesting != null) return sTemplateUrlServiceForTesting;
        return TemplateUrlServiceFactoryJni.get().getTemplateUrlService(profile);
    }

    @VisibleForTesting
    public static void setInstanceForTesting(TemplateUrlService service) {
        sTemplateUrlServiceForTesting = service;
    }

    // Natives interface is public to allow mocking in tests outside of
    // org.chromium.chrome.browser.search_engines package.
    @NativeMethods
    public interface Natives {
        TemplateUrlService getTemplateUrlService(Profile profile);
    }
}
