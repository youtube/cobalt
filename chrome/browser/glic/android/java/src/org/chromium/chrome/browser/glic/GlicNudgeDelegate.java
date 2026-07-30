// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.glic;

import org.chromium.build.annotations.NullMarked;

/** Java equivalent of the C++ GlicNudgeDelegate to handle tab strip Glic nudges. */
@NullMarked
public interface GlicNudgeDelegate {

    /**
     * Called to trigger/show the Glic nudge UI.
     *
     * @param label The action label. This string appears on the clickable part of the nudge.
     * @param anchoredMessageText The longer description, shown in the anchored message UI.
     * @param promptSuggestion The optional prompt to be filled in to Glic if the nudge is clicked.
     */
    void onTriggerGlicNudgeUi(String label, String anchoredMessageText, String promptSuggestion);

    /** Called to hide/dismiss the Glic nudge UI. */
    void onHideGlicNudgeUi();

    /** Returns whether the Glic nudge UI is currently showing. */
    boolean getIsShowingGlicNudge();
}
