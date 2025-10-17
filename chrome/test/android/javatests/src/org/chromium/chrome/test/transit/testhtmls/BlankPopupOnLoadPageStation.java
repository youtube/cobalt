// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.testhtmls;

import android.util.Pair;

import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.transit.page.PageStation;
import org.chromium.chrome.test.transit.page.PopupBlockedMessageFacility;
import org.chromium.chrome.test.transit.page.WebPageStation;

/** PageStation for popup_blank_test.html, which opens a blank pop-up upon loading. */
public class BlankPopupOnLoadPageStation extends WebPageStation {
    public static final String PATH = "/chrome/test/data/android/popup_blank_test.html";

    protected <T extends BlankPopupOnLoadPageStation> BlankPopupOnLoadPageStation(
            Builder<T> builder) {
        super(builder);
    }

    /**
     * Load popup_blank_test.html in current tab and expect the pop-up to be blocked and a pop-up
     * blocked message to be displayed.
     *
     * @return the now active BlankPopupOnLoadPageStation and the entered
     *     PopupBlockedMessageFacility
     */
    public static Pair<BlankPopupOnLoadPageStation, PopupBlockedMessageFacility>
            loadInCurrentTabExpectBlocked(
                    ChromeTabbedActivityTestRule activityTestRule, PageStation currentPageStation) {
        String url = activityTestRule.getTestServer().getURL(PATH);
        PopupBlockedMessageFacility<BlankPopupOnLoadPageStation> popupBlockedMessage =
                new PopupBlockedMessageFacility<>(1);
        BlankPopupOnLoadPageStation newPage =
                currentPageStation.loadPageProgrammatically(
                        url,
                        new Builder<BlankPopupOnLoadPageStation>(BlankPopupOnLoadPageStation::new)
                                .withFacility(popupBlockedMessage));

        return Pair.create(newPage, popupBlockedMessage);
    }
}
