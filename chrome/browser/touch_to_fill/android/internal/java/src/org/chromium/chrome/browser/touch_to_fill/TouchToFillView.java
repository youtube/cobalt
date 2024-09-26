// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill;

import static org.chromium.chrome.browser.password_manager.PasswordManagerHelper.usesUnifiedPasswordManagerBranding;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.RelativeLayout;

import androidx.annotation.Px;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.ItemType;
import org.chromium.chrome.browser.touch_to_fill.common.ItemDividerBase;
import org.chromium.chrome.browser.touch_to_fill.common.TouchToFillViewBase;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;

/**
 * This class is responsible for rendering the bottom sheet which displays the touch to fill
 * credentials. It is a View in this Model-View-Controller component and doesn't inherit but holds
 * Android Views.
 */
class TouchToFillView extends TouchToFillViewBase {
    private static class HorizontalDividerItemDecoration extends ItemDividerBase {
        HorizontalDividerItemDecoration(Context context) {
            super(context);
        }

        @Override
        protected int selectBackgroundDrawable(
                int position, boolean containsFillButton, int itemCount) {
            if (!usesUnifiedPasswordManagerBranding()) {
                return R.drawable.touch_to_fill_credential_background;
            }
            return super.selectBackgroundDrawable(position, containsFillButton, itemCount);
        }

        @Override
        protected boolean shouldSkipItemType(@ItemType int type) {
            switch (type) {
                case ItemType.HEADER: // Fallthrough.
                case ItemType.FILL_BUTTON:
                case ItemType.FOOTER:
                    return true;
                case ItemType.CREDENTIAL: // Fallthrough.
                case ItemType.WEBAUTHN_CREDENTIAL:
                    return false;
            }
            assert false : "Undefined whether to skip setting background for item of type: " + type;
            return true; // Should never be reached. But if, skip to not change anything.
        }

        @Override
        protected boolean containsFillButton(RecyclerView parent) {
            int itemCount = parent.getAdapter().getItemCount();
            // The button will be above the footer if it's present.
            return itemCount > 1
                    && parent.getAdapter().getItemViewType(itemCount - 2) == ItemType.FILL_BUTTON;
        }
    }

    /**
     * Constructs a TouchToFillView which creates, modifies, and shows the bottom sheet.
     *
     * @param context A {@link Context} used to load resources and inflate the sheet.
     * @param bottomSheetController The {@link BottomSheetController} used to show/hide the sheet.
     */
    TouchToFillView(Context context, BottomSheetController bottomSheetController) {
        super(bottomSheetController,
                (RelativeLayout) LayoutInflater.from(context).inflate(
                        R.layout.touch_to_fill_sheet, null));

        if (usesUnifiedPasswordManagerBranding()) {
            getSheetItemListView().addItemDecoration(new HorizontalDividerItemDecoration(context));
        }
    }

    @Override
    public int getVerticalScrollOffset() {
        return getSheetItemListView().computeVerticalScrollOffset();
    }

    @Override
    public int getSheetContentDescriptionStringId() {
        return R.string.touch_to_fill_content_description;
    }

    @Override
    public int getSheetHalfHeightAccessibilityStringId() {
        return R.string.touch_to_fill_sheet_half_height;
    }

    @Override
    public int getSheetFullHeightAccessibilityStringId() {
        return R.string.touch_to_fill_sheet_full_height;
    }

    @Override
    public int getSheetClosedAccessibilityStringId() {
        return R.string.touch_to_fill_sheet_closed;
    }

    @Override
    protected View getHandlebar() {
        return getContentView().findViewById(R.id.drag_handlebar);
    }

    @Override
    protected @Px int getConclusiveMarginHeightPx() {
        return getContentView().getResources().getDimensionPixelSize(
                usesUnifiedPasswordManagerBranding()
                        ? R.dimen.touch_to_fill_sheet_bottom_padding_button_modern
                        : R.dimen.touch_to_fill_sheet_bottom_padding_button);
    }

    @Override
    protected @Px int getSideMarginPx() {
        return getContentView().getResources().getDimensionPixelSize(
                usesUnifiedPasswordManagerBranding() ? R.dimen.touch_to_fill_sheet_margin_modern
                                                     : R.dimen.touch_to_fill_sheet_margin);
    }

    @Override
    protected int listedItemType() {
        return TouchToFillProperties.ItemType.CREDENTIAL;
    }

    @Override
    protected int footerItemType() {
        return TouchToFillProperties.ItemType.FOOTER;
    }
}
