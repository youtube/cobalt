// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.side_panel_container.dev;

import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.side_panel.AndroidSidePanelEnabledFn;
import org.chromium.chrome.browser.ui.side_panel_container.SidePanelContainerCoordinator;
import org.chromium.ui.base.WindowAndroid;

import java.util.function.Supplier;

/** Factory for creating a {@link SidePanelDevFeature}. */
@NullMarked
public final class SidePanelDevFeatureFactory {

    private SidePanelDevFeatureFactory() {}

    @Nullable
    public static SidePanelDevFeature create(
            MonotonicObservableSupplier<Profile> profileSupplier,
            SidePanelContainerCoordinator sidePanelContainerCoordinator,
            WindowAndroid windowAndroid,
            Supplier<Tab> tabSupplier) {
        if (AndroidSidePanelEnabledFn.isPureJavaDevFeatureEnabled()) {
            return new SidePanelDevFeatureImpl(
                    profileSupplier, sidePanelContainerCoordinator, windowAndroid);
        }

        if (AndroidSidePanelEnabledFn.isTabScopedDevFeatureEnabled()) {
            return new SidePanelTabScopedDevFeatureImpl(tabSupplier);
        }

        return null;
    }
}
