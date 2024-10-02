// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.bottombar.ephemeraltab;

import android.view.ViewGroup;

import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.url.GURL;

/**
 * An Observer that gets notified on Ephemeral Tab events.
 */
public interface EphemeralTabObserver {
    default void onToolbarCreated(ViewGroup toolbarView){};

    default void onNavigationStarted(GURL clickedUrl, BottomSheetController bottomSheetController,
            EphemeralTabSheetContent ephemeralTabSheetContent){};

    default void onTitleSet(EphemeralTabSheetContent sheetContent, String title){};
}
