// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.sheet_tabs;

import static org.chromium.ui.base.LocalizationUtils.isLayoutRtl;

import android.content.res.TypedArray;
import android.graphics.Canvas;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;
import android.text.method.PasswordTransformationMethod;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.Nullable;
import androidx.core.view.ViewCompat;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.chrome.browser.keyboard_accessory.R;
import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData;
import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData.FooterCommand;
import org.chromium.chrome.browser.keyboard_accessory.data.UserInfoField;
import org.chromium.chrome.browser.keyboard_accessory.helper.FaviconHelper;
import org.chromium.chrome.browser.keyboard_accessory.sheet_tabs.AccessorySheetTabItemsModel.AccessorySheetDataPiece;
import org.chromium.chrome.browser.keyboard_accessory.sheet_tabs.AccessorySheetTabViewBinder.ElementViewHolder;
import org.chromium.ui.HorizontalListDividerDrawable;
import org.chromium.ui.modelutil.ListModel;

/**
 * This stateless class provides methods to bind the items in a {@link ListModel <Item>}
 * to the {@link RecyclerView} used as view of the Password accessory sheet component.
 */
class PasswordAccessorySheetViewBinder {
    static ElementViewHolder create(ViewGroup parent, @AccessorySheetDataPiece.Type int viewType) {
        switch (viewType) {
            case AccessorySheetDataPiece.Type.TITLE:
                return new PasswordsTitleViewHolder(parent);
            case AccessorySheetDataPiece.Type.PASSWORD_INFO:
                return new PasswordsInfoViewHolder(parent);
            case AccessorySheetDataPiece.Type.FOOTER_COMMAND:
                return new FooterCommandViewHolder(parent);
        }
        assert false : "Unhandled type of data piece: " + viewType;
        return null;
    }

    /**
     * Holds a the TextView with the title of the sheet and a divider for the accessory bar.
     */
    static class PasswordsTitleViewHolder extends ElementViewHolder<String, LinearLayout> {
        PasswordsTitleViewHolder(ViewGroup parent) {
            super(parent, R.layout.password_accessory_sheet_label);
        }

        @Override
        protected void bind(String displayText, LinearLayout view) {
            TextView titleView = view.findViewById(R.id.tab_title);
            titleView.setText(displayText);
            titleView.setContentDescription(displayText);
        }
    }

    /**
     * Holds a TextView that represents a bottom command and is separated to the top by a divider.
     */
    static class FooterCommandViewHolder extends ElementViewHolder<FooterCommand, LinearLayout> {
        public static class DynamicTopDivider extends RecyclerView.ItemDecoration {
            @Override
            public void getItemOffsets(
                    Rect outRect, View view, RecyclerView parent, RecyclerView.State state) {
                super.getItemOffsets(outRect, view, parent, state);
                if (view.getId() != R.id.footer_command) return;
                int previous = parent.indexOfChild(view) - 1;
                if (previous < 0) return;
                if (parent.getChildAt(previous).getId() == R.id.footer_command) return;
                outRect.top = view.getContext().getResources().getDimensionPixelSize(
                                      R.dimen.keyboard_accessory_suggestion_padding)
                        + view.getContext().getResources().getDimensionPixelSize(
                                R.dimen.divider_height);
            }

            @Override
            public void onDraw(Canvas canvas, RecyclerView parent, RecyclerView.State state) {
                int attatchedChlidCount = parent.getChildCount();
                for (int i = 0; i < attatchedChlidCount - 1; ++i) {
                    View currentView = parent.getChildAt(i);
                    if (currentView.getId() == R.id.footer_command) break;

                    View nextView = parent.getChildAt(i + 1);
                    if (nextView.getId() != R.id.footer_command) continue;

                    Drawable dividerDrawable =
                            HorizontalListDividerDrawable.create(nextView.getContext());
                    int top = currentView.getBottom()
                            + currentView.getContext().getResources().getDimensionPixelOffset(
                                      R.dimen.keyboard_accessory_suggestion_padding)
                                    / 2;
                    int bottom = top + dividerDrawable.getIntrinsicHeight();
                    dividerDrawable.setBounds(parent.getLeft() + parent.getPaddingLeft(), top,
                            parent.getRight() - parent.getPaddingRight(), bottom);

                    dividerDrawable.draw(canvas);
                }
            }
        }

        FooterCommandViewHolder(ViewGroup parent) {
            super(parent, R.layout.password_accessory_sheet_legacy_option);
        }

        @Override
        protected void bind(FooterCommand footerCommand, LinearLayout layout) {
            TextView view = layout.findViewById(R.id.footer_text);
            view.setText(footerCommand.getDisplayText());
            view.setContentDescription(footerCommand.getDisplayText());
            view.setOnClickListener(v -> footerCommand.execute());
            view.setClickable(true);
        }
    }

    /**
     * Holds a layout for a username and a password with a small icon.
     */
    static class PasswordsInfoViewHolder
            extends ElementViewHolder<KeyboardAccessoryData.UserInfo, LinearLayout> {
        private final int mPadding;
        private final int mIconSize;

        PasswordsInfoViewHolder(ViewGroup parent) {
            super(parent, R.layout.keyboard_accessory_sheet_tab_legacy_password_info);
            mPadding = itemView.getContext().getResources().getDimensionPixelSize(
                    R.dimen.keyboard_accessory_suggestion_padding);
            mIconSize = itemView.getContext().getResources().getDimensionPixelSize(
                    R.dimen.keyboard_accessory_suggestion_icon_size);
        }

        @Override
        protected void bind(KeyboardAccessoryData.UserInfo info, LinearLayout layout) {
            TextView username = layout.findViewById(R.id.suggestion_text);
            TextView password = layout.findViewById(R.id.password_text);
            bindTextView(username, info.getFields().get(0));
            bindTextView(password, info.getFields().get(1));

            // Set the default icon for username, then try to get a better one.
            FaviconHelper faviconHelper = FaviconHelper.create(username.getContext());
            setIconForBitmap(username, faviconHelper.getDefaultIcon(info.getOrigin()));
            faviconHelper.fetchFavicon(info.getOrigin(), icon -> setIconForBitmap(username, icon));

            ViewCompat.setPaddingRelative(username, mPadding, 0, mPadding, 0);
            // Passwords have no icon, so increase the offset.
            ViewCompat.setPaddingRelative(password, 2 * mPadding + mIconSize, 0, mPadding, 0);
        }

        private void bindTextView(TextView text, UserInfoField field) {
            text.setTransformationMethod(
                    field.isObfuscated() ? new PasswordTransformationMethod() : null);
            // With transformation, the character set forces a LTR gravity. Therefore, invert it:
            text.setGravity(Gravity.CENTER_VERTICAL
                    | (isLayoutRtl() && field.isObfuscated() ? Gravity.END : Gravity.START));
            text.setText(field.getDisplayText());
            text.setContentDescription(field.getA11yDescription());
            text.setOnClickListener(!field.isSelectable() ? null : src -> field.triggerSelection());
            text.setClickable(true); // Ensures that "disabled" is announced.
            text.setEnabled(field.isSelectable());
            text.setBackground(getBackgroundDrawable(field.isSelectable()));
        }

        private @Nullable Drawable getBackgroundDrawable(boolean selectable) {
            if (!selectable) return null;
            TypedArray a = itemView.getContext().obtainStyledAttributes(
                    new int[] {R.attr.selectableItemBackground});
            Drawable suggestionBackground = a.getDrawable(0);
            a.recycle();
            return suggestionBackground;
        }

        private void setIconForBitmap(TextView text, @Nullable Drawable icon) {
            if (icon != null) {
                icon.setBounds(0, 0, mIconSize, mIconSize);
            }
            text.setCompoundDrawablePadding(mPadding);
            text.setCompoundDrawablesRelative(icon, null, null, null);
        }
    }

    static void initializeView(RecyclerView view, AccessorySheetTabItemsModel model) {
        view.setAdapter(PasswordAccessorySheetCoordinator.createAdapter(model));
        view.addItemDecoration(new FooterCommandViewHolder.DynamicTopDivider());
    }
}
