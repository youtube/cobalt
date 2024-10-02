
// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.supervised_user;
import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isCompletelyDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.isRoot;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import org.chromium.components.browser_ui.bottomsheet.BottomSheetTestSupport;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.DOMUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.ViewUtils;

import java.util.concurrent.TimeoutException;

class WebsiteParentApprovalTestUtils {
    private static final String LOCAL_APPROVALS_BUTTON_NODE_ID = "local-approvals-button";

    static void clickAskInPerson(WebContents webContents) {
        try {
            String contents = DOMUtils.getNodeContents(webContents, LOCAL_APPROVALS_BUTTON_NODE_ID);
        } catch (TimeoutException e) {
            throw new RuntimeException("Local approval button not found");
        }
        DOMUtils.clickNodeWithJavaScript(webContents, LOCAL_APPROVALS_BUTTON_NODE_ID);
    }

    private static void checkParentApprovalBottomSheetVisible(
            BottomSheetTestSupport bottomSheetTestSupport) {
        onView(isRoot()).check(ViewUtils.waitForView(
                withId(R.id.local_parent_approval_layout), ViewUtils.VIEW_VISIBLE));
        // Ensure all animations have ended before allowing interaction with the view.
        TestThreadUtils.runOnUiThreadBlocking(() -> { bottomSheetTestSupport.endAllAnimations(); });
    }

    static void clickApprove(BottomSheetTestSupport bottomSheetTestSupport) {
        checkParentApprovalBottomSheetVisible(bottomSheetTestSupport);
        onView(withId(R.id.approve_button))
                .check(matches(isCompletelyDisplayed()))
                .perform(click());
    }

    static void clickDoNotApprove(BottomSheetTestSupport bottomSheetTestSupport) {
        checkParentApprovalBottomSheetVisible(bottomSheetTestSupport);
        onView(withId(R.id.deny_button)).check(matches(isCompletelyDisplayed())).perform(click());
    }
}
