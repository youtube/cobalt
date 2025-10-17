// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.edge_to_edge;

import static org.chromium.base.test.transit.Condition.whether;

import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.transit.ConditionStatus;
import org.chromium.base.test.transit.LogicalElement;
import org.chromium.chrome.browser.browser_controls.BottomControlsStacker;
import org.chromium.chrome.browser.browser_controls.BottomControlsStacker.LayerType;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeUtils;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.transit.edge_to_edge.EdgeToEdgeConditions.BottomControlsStackerCondition;
import org.chromium.chrome.test.transit.edge_to_edge.EdgeToEdgeConditions.EdgeToEdgeControllerCondition;
import org.chromium.chrome.test.transit.page.PageStation;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.content_public.browser.test.transit.HtmlElement;
import org.chromium.content_public.browser.test.transit.HtmlElementSpec;

/**
 * Station represent an page that has viewport-fit=cover. It expects an {@link
 * org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeController} to be ready when the page
 * loads, so it requires the test to have corresponding test flag setup to make {@link
 * EdgeToEdgeUtils#isChromeEdgeToEdgeFeatureEnabled()}
 */
public class ViewportFitCoverPageStation extends WebPageStation {
    /** Page opt-in edge-to-edge with sub frames. */
    public static final String PATH_VIEWPORT_FIT_COVER_SUB_FRAMES =
            "/chrome/test/data/android/edge_to_edge/viewport-fit-cover-sub-frames-main.html";

    public static final HtmlElementSpec AVOID_BOTTOM_DIV = new HtmlElementSpec("positioned-bottom");
    public static final HtmlElementSpec FULLSCREEN_MAIN_BUTTON =
            new HtmlElementSpec("fullscreen-main");

    private final HtmlElement mAvoidBottomElement;
    private final HtmlElement mFullScreenButtonElement;

    protected <T extends ViewportFitCoverPageStation> ViewportFitCoverPageStation(
            Builder<T> builder) {
        super(builder);

        // Ensure the web page elements are drawn and visible.
        mAvoidBottomElement = declareElement(new HtmlElement(AVOID_BOTTOM_DIV, webContentsElement));
        mFullScreenButtonElement =
                declareElement(new HtmlElement(FULLSCREEN_MAIN_BUTTON, webContentsElement));

        // Declare requiring EdgeToEdgeController, meaning #setDecorFitsSystemWindows(false)
        declareEnterCondition(new EdgeToEdgeControllerCondition(mActivityElement));

        // Ensure the bottom chin is on display.
        Supplier<BottomControlsStacker> bottomControlsStacker =
                declareEnterConditionAsElement(
                        new BottomControlsStackerCondition(mActivityElement));
        declareElement(
                LogicalElement.uiThreadLogicalElement(
                        "Bottom chin is not on display",
                        this::isBottomChinShowing,
                        bottomControlsStacker));
    }

    /** Load the test page viewport-fit-cover-sub-frames-main.html. */
    public static ViewportFitCoverPageStation loadViewportFitCoverPage(
            ChromeTabbedActivityTestRule activityTestRule, PageStation currentPageStation) {
        String url = activityTestRule.getTestServer().getURL(PATH_VIEWPORT_FIT_COVER_SUB_FRAMES);
        return currentPageStation.loadPageProgrammatically(
                url, new Builder<>(ViewportFitCoverPageStation::new));
    }

    private ConditionStatus isBottomChinShowing(BottomControlsStacker bottomControlsStacker) {
        var bottomChinLayer = bottomControlsStacker.getLayerForTesting(LayerType.BOTTOM_CHIN);
        return whether(bottomChinLayer != null && bottomChinLayer.getHeight() > 0);
    }
}
