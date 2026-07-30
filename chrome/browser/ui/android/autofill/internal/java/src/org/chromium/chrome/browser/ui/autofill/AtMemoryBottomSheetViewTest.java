// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.autofill;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import android.content.Context;
import android.view.ContextThemeWrapper;
import android.view.View;

import androidx.appcompat.widget.SearchView;
import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.ui.autofill.internal.R;
import org.chromium.ui.modelutil.PropertyModel;

/** Unit tests for {@link AtMemoryBottomSheetView}. */
@RunWith(BaseRobolectricTestRunner.class)
@NullMarked
public class AtMemoryBottomSheetViewTest {

    private Context mContext;
    private AtMemoryBottomSheetView mView;

    @Before
    public void setUp() {
        mContext =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        R.style.Theme_BrowserUI_DayNight);
        mView = new AtMemoryBottomSheetView(mContext);
    }

    @Test
    public void testSearchAreaGainsFocusWhenVisible() {
        PropertyModel model =
                new PropertyModel.Builder(AtMemoryBottomSheetProperties.ALL_KEYS)
                        .with(AtMemoryBottomSheetProperties.VISIBLE, true)
                        .build();
        AtMemoryBottomSheetViewBinder.bind(model, mView, AtMemoryBottomSheetProperties.VISIBLE);

        View contentView = mView.getContentView();
        View searchView = contentView.findViewById(R.id.search_query_input);
        assertNotNull(searchView);
        assertTrue(searchView.hasFocus());
    }

    @Test
    public void testSearchTextIsClearedWhenVisible() {
        View contentView = mView.getContentView();
        SearchView searchView = contentView.findViewById(R.id.search_query_input);
        assertNotNull(searchView);
        searchView.setQuery("some text", false);

        PropertyModel model =
                new PropertyModel.Builder(AtMemoryBottomSheetProperties.ALL_KEYS)
                        .with(AtMemoryBottomSheetProperties.VISIBLE, true)
                        .build();
        AtMemoryBottomSheetViewBinder.bind(model, mView, AtMemoryBottomSheetProperties.VISIBLE);

        assertEquals("", searchView.getQuery().toString());
    }
}
