// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.hub;

import static androidx.test.espresso.action.ViewActions.replaceText;
import static androidx.test.espresso.matcher.ViewMatchers.isAssignableFrom;
import static androidx.test.espresso.matcher.ViewMatchers.withContentDescription;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.CoreMatchers.allOf;
import static org.hamcrest.CoreMatchers.startsWith;

import static org.chromium.base.test.transit.ViewSpec.viewSpec;

import android.content.Context;
import android.view.View;
import android.widget.EditText;

import androidx.annotation.Nullable;
import androidx.test.espresso.Espresso;

import org.hamcrest.Matcher;

import org.chromium.base.test.transit.Elements;
import org.chromium.base.test.transit.Facility;
import org.chromium.base.test.transit.Transition;
import org.chromium.base.test.transit.ViewElement;
import org.chromium.base.test.transit.ViewSpec;
import org.chromium.chrome.browser.tabmodel.TabGroupColorUtils;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tasks.tab_management.ColorPickerUtils;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.transit.SoftKeyboardFacility;
import org.chromium.chrome.test.transit.tabmodel.TabGroupUtil;
import org.chromium.chrome.test.util.TabBinningUtil;
import org.chromium.components.tab_groups.TabGroupColorId;

import java.util.List;

/** Dialog that appears when a new tab group is created to name the group and pick a color. */
// TODO(crbug.com/374366760): Change to generic Facility<HostStationT>.
public class NewTabGroupDialogFacility extends Facility<TabSwitcherStation> {

    public static final ViewSpec DIALOG = viewSpec(withId(R.id.visual_data_dialog_layout));
    public static final ViewSpec DIALOG_TITLE =
            viewSpec(allOf(withId(R.id.visual_data_dialog_title), withText("New tab group")));

    public static final Matcher<View> TITLE_INPUT_MATCHER =
            allOf(withId(R.id.title_input_text), isAssignableFrom(EditText.class));
    public static final ViewSpec COLOR_PICKER_CONTAINER =
            viewSpec(withId(R.id.color_picker_container));

    public static final ViewSpec DONE_BUTTON = viewSpec(withId(R.id.positive_button));

    private final List<Integer> mTabIdsToGroup;
    private final String mTitle;
    private final @Nullable @TabGroupColorId Integer mSelectedColor;
    private final SoftKeyboardFacility mSoftKeyboard;
    private ViewSpec mTitleInputSpec;
    private ViewElement mDialog;

    /** Constructor. Expects no particular title or selected color. */
    public NewTabGroupDialogFacility(
            List<Integer> tabIdsToGroup, SoftKeyboardFacility softKeyboard) {
        this(
                tabIdsToGroup,
                TabGroupUtil.getNumberOfTabsString(tabIdsToGroup.size()),
                /* selectedColor= */ null,
                softKeyboard);
    }

    /** Constructor. Expects a specific title and selected color. */
    public NewTabGroupDialogFacility(
            List<Integer> tabIdsToGroup,
            String title,
            @Nullable @TabGroupColorId Integer selectedColor,
            SoftKeyboardFacility softKeyboard) {
        mTabIdsToGroup = tabIdsToGroup;
        mTitle = title;
        mSelectedColor = selectedColor;
        mSoftKeyboard = softKeyboard;
    }

    @Override
    public void declareElements(Elements.Builder elements) {
        mDialog = elements.declareView(DIALOG, ViewElement.displayingAtLeastOption(80));
        elements.declareView(DIALOG_TITLE);

        String inputElementId = "Tab group title input showing " + mTitle;
        mTitleInputSpec = viewSpec(allOf(TITLE_INPUT_MATCHER, withText(mTitle)));
        elements.declareView(mTitleInputSpec, ViewElement.elementIdOption(inputElementId));

        // TODO(crbug.com/346377124): Partially cut off in android_30_google_apis_x86.textpb
        elements.declareView(COLOR_PICKER_CONTAINER, ViewElement.displayingAtLeastOption(50));
        @TabGroupColorId List<Integer> colors = TabGroupColorUtils.getTabGroupColorIdList();
        // Only the first 5 colors are displayed reliably when the soft keyboard opens.
        for (int i = 0; i < 5; i++) {
            @TabGroupColorId Integer color = colors.get(i);
            if (mSelectedColor != null) {
                elements.declareView(
                        colorPickerIconSpec(color, color.equals(mSelectedColor)),
                        ViewElement.newOptions().unscoped().displayingAtLeast(60).build());
            } else {
                elements.declareView(
                        colorPickerIconSpec(color, /* selected= */ null),
                        ViewElement.newOptions().unscoped().displayingAtLeast(60).build());
            }
        }

        elements.declareView(DONE_BUTTON);
    }

    private ViewSpec colorPickerIconSpec(
            @TabGroupColorId Integer color, @Nullable Boolean selected) {
        Context context = mHostStation.getActivity();
        String colorName =
                context.getString(
                        ColorPickerUtils.getTabGroupColorPickerItemColorAccessibilityString(color));
        Matcher<View> contentDescriptionMatcher;
        if (selected != null) {
            contentDescriptionMatcher =
                    withContentDescription(
                            colorName + " " + (selected ? "Selected" : "Not selected"));
        } else {
            contentDescriptionMatcher = withContentDescription(startsWith(colorName));
        }
        return viewSpec(allOf(withId(R.id.color_picker_icon), contentDescriptionMatcher));
    }

    /** Input a new tab group name. */
    public NewTabGroupDialogFacility inputName(String newTabGroupName) {
        // An empty name causes warning text to show up which could push the color picker container
        // out of view for small screen devices, so dismiss the keyboard.
        if (newTabGroupName.isEmpty()) {
            ensureSoftKeyboardClosed();
        }

        return mHostStation.swapFacilitySync(
                this,
                new NewTabGroupDialogFacility(
                        mTabIdsToGroup, newTabGroupName, mSelectedColor, mSoftKeyboard),
                () -> mTitleInputSpec.perform(replaceText(newTabGroupName)));
    }

    /** Select a color. */
    public NewTabGroupDialogFacility pickColor(@TabGroupColorId int newColor) {
        return mHostStation.swapFacilitySync(
                this,
                new NewTabGroupDialogFacility(mTabIdsToGroup, mTitle, newColor, mSoftKeyboard),
                colorPickerIconSpec(newColor, /* selected= */ false)::click);
    }

    /** Press "Done" to confirm the tab group name and color. */
    public TabSwitcherGroupCardFacility pressDone() {
        ensureSoftKeyboardClosed();

        // The reason we can pass an expected card index is because the tab group has already been
        // created.
        TabModel currentModel = mHostStation.getTabModelSelectorSupplier().get().getCurrentModel();
        int expectedCardIndex = TabBinningUtil.getBinIndex(currentModel, mTabIdsToGroup);
        return mHostStation.swapFacilitySync(
                this,
                new TabSwitcherGroupCardFacility(expectedCardIndex, mTabIdsToGroup, mTitle),
                DONE_BUTTON::click);
    }

    /** Press "Done" to confirm the tab group name and color, but no-op from an invalid title. */
    public NewTabGroupDialogFacility pressDoneWithInvalidTitle() {
        ensureSoftKeyboardClosed();

        return mHostStation.swapFacilitySync(
                this,
                new NewTabGroupDialogFacility(
                        mTabIdsToGroup, mTitle, mSelectedColor, mSoftKeyboard),
                Transition.newOptions().withPossiblyAlreadyFulfilled().build(),
                DONE_BUTTON::click);
    }

    /** Press the system backpress to confirm the tab group name and color. */
    public TabSwitcherGroupCardFacility pressBack() {
        ensureSoftKeyboardClosed();

        // The reason we can pass an expected card index is because the tab group has already been
        // created.
        TabModel currentModel = mHostStation.getTabModelSelectorSupplier().get().getCurrentModel();
        int expectedCardIndex = TabBinningUtil.getBinIndex(currentModel, mTabIdsToGroup);
        return mHostStation.swapFacilitySync(
                this,
                new TabSwitcherGroupCardFacility(expectedCardIndex, mTabIdsToGroup, mTitle),
                () -> {
                    Espresso.pressBack();
                });
    }

    private void ensureSoftKeyboardClosed() {
        if (mSoftKeyboard.getPhase() == Phase.ACTIVE) {
            mSoftKeyboard.close(mDialog);
        } else if (mSoftKeyboard.getPhase() == Phase.FINISHED) {
            // Do nothing as the soft keyboard has already been closed
        } else {
            throw new IllegalArgumentException(
                    "SoftKeyboardFacility is in phase " + mSoftKeyboard.getPhase());
        }
    }
}
