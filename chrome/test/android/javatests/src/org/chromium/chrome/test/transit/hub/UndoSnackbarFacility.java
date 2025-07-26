// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.hub;

import org.chromium.chrome.test.transit.SnackbarFacility;

/** The snackbar that lets the user undo a group or close operation for a short time. */
public class UndoSnackbarFacility extends SnackbarFacility<TabSwitcherStation> {

    public UndoSnackbarFacility(String message) {
        super(message, "Undo");
    }

    /** Press undo to revert the operation. */
    public void pressUndo() {
        mHostStation.exitFacilitySync(this, SNACKBAR_BUTTON::click);
    }
}
