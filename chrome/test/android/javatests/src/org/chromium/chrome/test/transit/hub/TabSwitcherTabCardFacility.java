// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.hub;

import androidx.annotation.Nullable;

import org.chromium.base.test.transit.Elements;
import org.chromium.chrome.test.transit.page.PageStation;

/** Represents a non-grouped tab card in the Tab Switcher. */
public class TabSwitcherTabCardFacility extends TabSwitcherCardFacility {
    private final int mTabId;

    public TabSwitcherTabCardFacility(@Nullable Integer cardIndex, int tabId, String title) {
        super(cardIndex, title);
        mTabId = tabId;
    }

    @Override
    public void declareElements(Elements.Builder elements) {
        super.declareElements(elements);
    }

    /** Clicks the tab card to show the page. */
    public <PageStationT extends PageStation> PageStationT clickCard(
            PageStation.Builder<PageStationT> destinationBuilder) {
        boolean isSelecting = mHostStation.getActivity().getActivityTab().getId() == mTabId;
        PageStationT destination =
                destinationBuilder
                        .withIncognito(mHostStation.isIncognito())
                        .withIsOpeningTabs(0)
                        .withIsSelectingTabs(isSelecting ? 1 : 0)
                        .withExpectedTitle(mTitle)
                        .build();
        return mHostStation.travelToSync(destination, clickTitleTrigger());
    }
}
