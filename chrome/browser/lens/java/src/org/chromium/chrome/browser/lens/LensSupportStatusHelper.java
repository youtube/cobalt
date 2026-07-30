// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.lens;

import android.text.TextUtils;

import org.jni_zero.CalledByNative;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.gsa.GSAUtils;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.components.search_engines.TemplateUrlService;

/** Helper class to determine the support status of Google Lens. */
@NullMarked
public class LensSupportStatusHelper {
    private static final String MIN_AGSA_VERSION_NAME_FOR_LENS_POSTCAPTURE = "10.65";

    /**
     * Returns the Lens support status based on a set of application compatibility checks.
     *
     * @param profile The profile to use for checking the default search engine.
     * @param isIncognito Whether the user is incognito.
     * @return A @LensSupportStatus Integer if compatibility checks were performed, or null if the
     *     feature is explicitly disabled (e.g., in Incognito) and no metrics should be recorded.
     */
    public static @Nullable @LensMetrics.LensSupportStatus Integer getLensSupportStatus(
            @Nullable Profile profile, boolean isIncognito) {
        if (isIncognito || profile == null) {
            return null;
        }

        final TemplateUrlService templateUrlServiceInstance =
                TemplateUrlServiceFactory.getForProfile(profile);
        if (!templateUrlServiceInstance.isDefaultSearchEngineGoogle()) {
            return LensMetrics.LensSupportStatus.NON_GOOGLE_SEARCH_ENGINE;
        }

        String versionName = GSAUtils.getAgsaVersionName();
        if (TextUtils.isEmpty(versionName)) {
            return LensMetrics.LensSupportStatus.ACTIVITY_NOT_ACCESSIBLE;
        }
        if (GSAUtils.isAgsaVersionBelowMinimum(
                versionName, MIN_AGSA_VERSION_NAME_FOR_LENS_POSTCAPTURE)) {
            return LensMetrics.LensSupportStatus.OUT_OF_DATE;
        }

        if (!GSAUtils.isValidAgsaPackage()) {
            return LensMetrics.LensSupportStatus.INVALID_PACKAGE;
        }

        return LensMetrics.LensSupportStatus.LENS_SEARCH_SUPPORTED;
    }

    /**
     * @return Whether Lens search is currently supported.
     */
    @CalledByNative
    public static boolean isLensSearchSupported(@Nullable Profile profile, boolean isIncognito) {
        Integer supportStatus = getLensSupportStatus(profile, isIncognito);
        return supportStatus != null
                && supportStatus == LensMetrics.LensSupportStatus.LENS_SEARCH_SUPPORTED;
    }
}
