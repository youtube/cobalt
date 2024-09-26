// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.bar_component;

import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.ANIMATION_LISTENER;
import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.BAR_ITEMS;
import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.BOTTOM_OFFSET_PX;
import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.DISABLE_ANIMATIONS_FOR_TESTING;
import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.HAS_SUGGESTIONS;
import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.OBFUSCATED_CHILD_AT_CALLBACK;
import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.SHEET_OPENER_ITEM;
import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.SHOW_SWIPING_IPH;
import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.SKIP_CLOSING_ANIMATION;
import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.VISIBLE;

import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import androidx.annotation.LayoutRes;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.chrome.browser.keyboard_accessory.R;
import org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.BarItem;
import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Observes {@link KeyboardAccessoryProperties} changes (like a newly available tab) and triggers
 * the {@link KeyboardAccessoryViewBinder} which will modify the view accordingly.
 */
class KeyboardAccessoryViewBinder {
    static BarItemViewHolder create(ViewGroup parent, @BarItem.Type int viewType) {
        switch (viewType) {
            case BarItem.Type.ACTION_BUTTON:
                return new BarItemTextViewHolder(parent, R.layout.keyboard_accessory_action);
            case BarItem.Type.SUGGESTION:
                return new BarItemTextViewHolder(parent, R.layout.keyboard_accessory_chip);
            case BarItem.Type.TAB_LAYOUT: // Intentional fallthrough. Not supported.
                assert false : "Type " + viewType + " is not a valid accessory bar action!";
        }
        assert false : "Action type " + viewType + " was not handled!";
        return null;
    }

    abstract static class BarItemViewHolder<T extends BarItem, V extends View>
            extends RecyclerView.ViewHolder {
        BarItemViewHolder(ViewGroup parent, @LayoutRes int layout) {
            super(LayoutInflater.from(parent.getContext()).inflate(layout, parent, false));
        }

        @SuppressWarnings("unchecked")
        void bind(BarItem barItem) {
            bind((T) barItem, (V) itemView);
        }

        /**
         * Called when the ViewHolder is bound.
         * @param item The {@link BarItem} that this ViewHolder represents.
         * @param view The {@link View} that this ViewHolder binds the bar item to.
         */
        protected abstract void bind(T item, V view);

        /**
         * The opposite of {@link #bind}. Use this to free expensive resources or reset observers.
         */
        protected void recycle() {}
    }

    static class BarItemTextViewHolder extends BarItemViewHolder<BarItem, TextView> {
        BarItemTextViewHolder(ViewGroup parent, @LayoutRes int layout) {
            super(parent, layout);
        }

        @Override
        public void bind(BarItem barItem, TextView textView) {
            KeyboardAccessoryData.Action action = barItem.getAction();
            assert action != null : "Tried to bind item without action. Chose a wrong ViewHolder?";
            textView.setText(action.getCaption());
            textView.setOnClickListener(view -> action.getCallback().onResult(action));
        }
    }

    static void bind(PropertyModel model, KeyboardAccessoryView view, PropertyKey propertyKey) {
        boolean wasBound = bindInternal(model, view, propertyKey);
        assert wasBound : "Every possible property update needs to be handled!";
    }

    /**
     * Tries to bind the given property to the given view by using the value in the given model.
     * @param model       A {@link PropertyModel}.
     * @param view        A {@link KeyboardAccessoryView}.
     * @param propertyKey A {@link PropertyKey}.
     * @return True if the given propertyKey was bound to the given view.
     */
    protected static boolean bindInternal(
            PropertyModel model, KeyboardAccessoryView view, PropertyKey propertyKey) {
        if (propertyKey == BAR_ITEMS) {
            view.setBarItemsAdapter(
                    KeyboardAccessoryCoordinator.createBarItemsAdapter(model.get(BAR_ITEMS)));
        } else if (propertyKey == DISABLE_ANIMATIONS_FOR_TESTING) {
            if (model.get(DISABLE_ANIMATIONS_FOR_TESTING)) view.disableAnimationsForTesting();
        } else if (propertyKey == VISIBLE) {
            view.setVisible(model.get(VISIBLE));
        } else if (propertyKey == SKIP_CLOSING_ANIMATION) {
            view.setSkipClosingAnimation(model.get(SKIP_CLOSING_ANIMATION));
            if (!model.get(VISIBLE)) view.setVisible(false); // Update to cancel any animation.
        } else if (propertyKey == BOTTOM_OFFSET_PX) {
            view.setBottomOffset(model.get(BOTTOM_OFFSET_PX));
        } else if (propertyKey == ANIMATION_LISTENER) {
            view.setAnimationListener(model.get(ANIMATION_LISTENER));
        } else if (propertyKey == SHEET_OPENER_ITEM || propertyKey == OBFUSCATED_CHILD_AT_CALLBACK
                || propertyKey == SHOW_SWIPING_IPH || propertyKey == HAS_SUGGESTIONS) {
            // No binding required.
        } else {
            return false;
        }
        return true;
    }
}
