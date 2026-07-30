// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.autofill;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.view.ContextThemeWrapper;
import android.view.View;
import android.view.ViewGroup;
import android.widget.EditText;

import androidx.constraintlayout.helper.widget.Flow;
import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.ui.autofill.internal.R;
import org.chromium.components.autofill.AutofillSuggestion;
import org.chromium.components.browser_ui.widget.chips.ChipView;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;

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
        EditText searchView = contentView.findViewById(R.id.search_query_input);
        assertNotNull(searchView);
        searchView.setText("some text");

        PropertyModel model =
                new PropertyModel.Builder(AtMemoryBottomSheetProperties.ALL_KEYS)
                        .with(AtMemoryBottomSheetProperties.VISIBLE, true)
                        .build();
        AtMemoryBottomSheetViewBinder.bind(model, mView, AtMemoryBottomSheetProperties.VISIBLE);

        assertEquals("", searchView.getText().toString());
    }

    @Test
    public void testSetFlyoutSuggestionsPopulatesChips() {
        List<AutofillSuggestion> suggestions =
                List.of(
                        new AutofillSuggestion.Builder()
                                .setLabel("Label 1")
                                .setSubLabel("Sublabel 1")
                                .build(),
                        new AutofillSuggestion.Builder()
                                .setLabel("Label 2")
                                .setSubLabel("")
                                .build());

        PropertyModel model =
                new PropertyModel.Builder(AtMemoryBottomSheetProperties.ALL_KEYS)
                        .with(AtMemoryBottomSheetProperties.FLYOUT_SUGGESTIONS, suggestions)
                        .build();
        AtMemoryBottomSheetViewBinder.bind(
                model, mView, AtMemoryBottomSheetProperties.FLYOUT_SUGGESTIONS);

        ViewGroup chipsContainer = mView.getContentView().findViewById(R.id.flyout_chips_container);
        assertNotNull(chipsContainer);

        Flow flow = mView.getContentView().findViewById(R.id.chips_flow);
        assertNotNull(flow);
        int[] ids = flow.getReferencedIds();
        assertEquals(2, ids.length);

        ChipView chip1 = mView.getContentView().findViewById(ids[0]);
        ChipView chip2 = mView.getContentView().findViewById(ids[1]);
        assertNotNull(chip1);
        assertNotNull(chip2);

        assertEquals("Label 1", chip1.getPrimaryTextView().getText().toString());
        assertEquals("Sublabel 1", chip1.getSecondaryTextView().getText().toString());
        assertEquals(View.VISIBLE, chip1.getSecondaryTextView().getVisibility());

        assertEquals("Label 2", chip2.getPrimaryTextView().getText().toString());
        assertEquals(View.GONE, chip2.getSecondaryTextView().getVisibility());

        // Adding views to a Flow posts asynchronous layout tasks to the main thread.
        // We must idle the main looper to ensure these tasks complete before verifying the view
        // hierarchy, avoiding flaky test failures.
        ShadowLooper.idleMainLooper();
    }

    @Test
    public void testNoticeVisibleProperty() {
        View contentView = mView.getContentView();
        View noticeContainer = contentView.findViewById(R.id.notice_container);
        assertNotNull(noticeContainer);

        PropertyModel model =
                new PropertyModel.Builder(AtMemoryBottomSheetProperties.ALL_KEYS)
                        .with(AtMemoryBottomSheetProperties.IS_NOTICE_VISIBLE, true)
                        .build();
        AtMemoryBottomSheetViewBinder.bind(
                model, mView, AtMemoryBottomSheetProperties.IS_NOTICE_VISIBLE);

        assertEquals(View.VISIBLE, noticeContainer.getVisibility());

        model.set(AtMemoryBottomSheetProperties.IS_NOTICE_VISIBLE, false);
        AtMemoryBottomSheetViewBinder.bind(
                model, mView, AtMemoryBottomSheetProperties.IS_NOTICE_VISIBLE);

        assertEquals(View.GONE, noticeContainer.getVisibility());
    }

    @Test
    public void testNoticeOkClickListenerProperty() {
        View contentView = mView.getContentView();
        View noticeOkButton = contentView.findViewById(R.id.notice_ok_button);
        assertNotNull(noticeOkButton);

        Runnable clicked = mock(Runnable.class);
        PropertyModel model =
                new PropertyModel.Builder(AtMemoryBottomSheetProperties.ALL_KEYS)
                        .with(AtMemoryBottomSheetProperties.NOTICE_OK_CLICK_LISTENER, clicked)
                        .build();
        AtMemoryBottomSheetViewBinder.bind(
                model, mView, AtMemoryBottomSheetProperties.NOTICE_OK_CLICK_LISTENER);

        noticeOkButton.performClick();
        verify(clicked).run();
    }
}
