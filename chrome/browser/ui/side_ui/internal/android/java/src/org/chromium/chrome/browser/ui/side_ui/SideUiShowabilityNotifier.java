// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.side_ui;

import com.google.errorprone.annotations.DoNotMock;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.SideUiShowability;

/** Notifies {@link SideUiObserver}s of {@link SideUiShowability} when it's changed. */
@DoNotMock
@NullMarked
final class SideUiShowabilityNotifier {
    private @Nullable SideUiShowability mLastSideUiShowability;

    void notify(Iterable<SideUiObserver> observers, SideUiShowability showability) {
        if (showability.equals(mLastSideUiShowability)) {
            return;
        }

        mLastSideUiShowability = showability;

        for (var observer : observers) {
            observer.onShowableSideUisUpdated(showability);
        }
    }
}
