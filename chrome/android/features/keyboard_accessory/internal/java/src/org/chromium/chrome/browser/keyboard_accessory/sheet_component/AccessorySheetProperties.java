// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.sheet_component;

import androidx.viewpager.widget.ViewPager;

import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData.Tab;
import org.chromium.ui.modelutil.ListModel;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/**
 * This model holds all view state of the accessory sheet.
 * It is updated by the {@link AccessorySheetMediator} and emits notification on which observers
 * like the view binder react.
 */
class AccessorySheetProperties {
    static final ReadableObjectPropertyKey<ListModel<Tab>> TABS = new ReadableObjectPropertyKey<>();
    static final WritableIntPropertyKey ACTIVE_TAB_INDEX =
            new WritableIntPropertyKey("active_tab_index");
    static final WritableBooleanPropertyKey VISIBLE = new WritableBooleanPropertyKey("visible");
    static final WritableIntPropertyKey HEIGHT = new WritableIntPropertyKey("height");
    static final WritableBooleanPropertyKey TOP_SHADOW_VISIBLE =
            new WritableBooleanPropertyKey("top_shadow_visible");
    static final WritableObjectPropertyKey<ViewPager.OnPageChangeListener> PAGE_CHANGE_LISTENER =
            new WritableObjectPropertyKey<>("page_change_listener");
    static final WritableObjectPropertyKey<Runnable> SHOW_KEYBOARD_CALLBACK =
            new WritableObjectPropertyKey<>("keyboard_callback");

    static final int NO_ACTIVE_TAB = -1;

    static PropertyModel.Builder defaultPropertyModel() {
        return new PropertyModel
                .Builder(TABS, ACTIVE_TAB_INDEX, VISIBLE, HEIGHT, TOP_SHADOW_VISIBLE,
                        PAGE_CHANGE_LISTENER, SHOW_KEYBOARD_CALLBACK)
                .with(TABS, new ListModel<>())
                .with(ACTIVE_TAB_INDEX, NO_ACTIVE_TAB)
                .with(VISIBLE, false)
                .with(TOP_SHADOW_VISIBLE, false);
    }

    private AccessorySheetProperties() {}
}
