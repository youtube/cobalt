// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.lens;

import org.chromium.base.UserData;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.lens.LensMetrics;
import org.chromium.chrome.browser.lens.LensSupportStatusHelper;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.url.GURL;

/**
 * A lightweight tab helper that tracks the showing state of the Lens Overlay. This allows
 * persistent UI elements (like the Omnibox) to observe the overlay state without requiring the full
 * LensOverlayCoordinator to be instantiated.
 */
@NullMarked
public class LensOverlayTabHelper implements UserData {
    private final SettableNonNullObservableSupplier<Boolean> mIsShowingSupplier =
            ObservableSuppliers.createNonNull(false);

    /**
     * @param tab The tab to check.
     * @return Whether the Lens Overlay is eligible to be shown.
     */
    public static boolean shouldShowLensOverlay(@Nullable Tab tab) {
        if (tab == null || tab.getWebContents() == null) {
            return false;
        }

        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.LENS_OVERLAY_ANDROID)) {
            return false;
        }

        GURL url = tab.getUrl();
        // This also filters out NTPs and internal pages.
        if (url == null || !UrlUtilities.isHttpOrHttps(url)) {
            return false;
        }

        @Nullable
        @LensMetrics.LensSupportStatus
        Integer supportStatus =
                LensSupportStatusHelper.getLensSupportStatus(tab.getProfile(), tab.isIncognito());
        if (supportStatus != null) {
            LensMetrics.recordPageSearchSupportStatus(supportStatus);
        }

        return supportStatus != null
                && supportStatus == LensMetrics.LensSupportStatus.LENS_SEARCH_SUPPORTED;
    }

    /**
     * @param tab The tab to check.
     * @return Whether the Lens Overlay is currently showing on the tab.
     */
    public static boolean isOverlayShowing(@Nullable Tab tab) {
        if (tab == null) {
            return false;
        }

        // Optimization: If helper doesn't exist, just return false and don't create.
        LensOverlayTabHelper helper = get(tab, false);
        return helper != null && helper.mIsShowingSupplier.get();
    }

    /**
     * Sets the showing state of the Lens Overlay for the given tab.
     *
     * @param tab The tab to update.
     * @param isShowing Whether the overlay is showing.
     */
    public static void setOverlayShowing(Tab tab, boolean isShowing) {
        // Optimization: If `isShowing` is false, don't bother creating an instance.
        LensOverlayTabHelper helper = get(tab, isShowing);
        if (helper != null) {
            helper.mIsShowingSupplier.set(isShowing);
        }
    }

    private static @Nullable LensOverlayTabHelper get(Tab tab, boolean createIfNeeded) {
        LensOverlayTabHelper helper = tab.getUserDataHost().getUserData(LensOverlayTabHelper.class);
        if (helper == null && createIfNeeded) {
            helper = new LensOverlayTabHelper();
            tab.getUserDataHost().setUserData(LensOverlayTabHelper.class, helper);
        }
        return helper;
    }

    private LensOverlayTabHelper() {}

    @Override
    public void destroy() {
        mIsShowingSupplier.destroy();
    }
}
