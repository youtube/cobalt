// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.autofill;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.ui.autofill.AtMemoryBottomSheetProperties.IS_LOADING;
import static org.chromium.chrome.browser.ui.autofill.AtMemoryBottomSheetProperties.SHOW_SUGGESTIONS_BACKGROUND;
import static org.chromium.chrome.browser.ui.autofill.AtMemoryBottomSheetProperties.VISIBLE;
import static org.chromium.chrome.browser.ui.autofill.AtMemoryBottomSheetSearchTileProperties.ON_TILE_CLICKED;
import static org.chromium.chrome.browser.ui.autofill.AtMemoryBottomSheetSearchTileProperties.TILE_TITLE;
import static org.chromium.chrome.browser.ui.autofill.AtMemoryBottomSheetSuggestionProperties.ON_FLYOUT_CLICKED;
import static org.chromium.chrome.browser.ui.autofill.AtMemoryBottomSheetSuggestionProperties.ON_SUGGESTION_CLICKED;
import static org.chromium.chrome.browser.ui.autofill.AtMemoryBottomSheetSuggestionProperties.TITLE;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.ui.autofill.internal.R;
import org.chromium.components.autofill.AutofillSuggestion;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;

/** Unit tests for {@link AtMemoryBottomSheetMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
@NullMarked
public class AtMemoryBottomSheetMediatorTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private AtMemoryBottomSheetCoordinator.Delegate mDelegate;
    @Mock private Runnable mHideKeyboardCallback;

    private PropertyModel mModel;
    private ModelList mModelList;
    private AtMemoryBottomSheetMediator mMediator;

    @Before
    public void setUp() {
        mModel =
                new PropertyModel.Builder(AtMemoryBottomSheetProperties.ALL_KEYS)
                        .with(VISIBLE, false)
                        .build();
        mModelList = new ModelList();
        mMediator =
                new AtMemoryBottomSheetMediator(
                        ApplicationProvider.getApplicationContext(),
                        mDelegate,
                        mModel,
                        mModelList,
                        mHideKeyboardCallback);
    }

    @Test
    public void testOnSuggestionClicked() {
        List<AutofillSuggestion> suggestions =
                List.of(
                        new AutofillSuggestion.Builder()
                                .setIconId(R.drawable.flight)
                                .setLabel("KLM204")
                                .setSubLabel("Flight ⋅ 15 May ⋅ SEA - MUC")
                                .build(),
                        new AutofillSuggestion.Builder()
                                .setIconId(R.drawable.travel_trip)
                                .setLabel("Hotel Booking")
                                .setSubLabel("Hilton ⋅ 16 May")
                                .build());

        mMediator.show(suggestions);

        assertTrue(mModel.get(VISIBLE));
        assertEquals(2, mModelList.size());

        assertEquals(suggestions.get(0).getLabel(), mModelList.get(0).model.get(TITLE));
        assertEquals(suggestions.get(1).getLabel(), mModelList.get(1).model.get(TITLE));

        PropertyModel itemModel1 = mModelList.get(0).model;
        itemModel1.get(ON_SUGGESTION_CLICKED).run();

        verify(mDelegate).onSuggestionClicked(/* position= */ 0);
    }

    @Test
    public void testOnFlyoutClicked() {
        List<AutofillSuggestion> suggestions =
                List.of(
                        new AutofillSuggestion.Builder()
                                .setIconId(R.drawable.flight)
                                .setLabel("KLM204")
                                .setSubLabel("Flight ⋅ 15 May ⋅ SEA - MUC")
                                .build(),
                        new AutofillSuggestion.Builder()
                                .setIconId(R.drawable.travel_trip)
                                .setLabel("Hotel Booking")
                                .setSubLabel("Hilton ⋅ 16 May")
                                .build());

        mMediator.show(suggestions);

        PropertyModel itemModel2 = mModelList.get(1).model;
        itemModel2.get(ON_FLYOUT_CLICKED).run();

        assertEquals(
                List.of(suggestions.get(1)),
                mModel.get(AtMemoryBottomSheetProperties.FLYOUT_SUGGESTIONS));
    }

    @Test
    public void testOnDismissed() {
        mModel.set(VISIBLE, true);
        mModel.set(IS_LOADING, true);
        mMediator.onDismissed();
        assertFalse(mModel.get(VISIBLE));
        assertFalse(mModel.get(IS_LOADING));
        verify(mDelegate).onDismissed();
    }

    @Test
    public void testOnQuerySubmitted() {
        mMediator.onQuerySubmitted("flight");
        assertTrue(mModel.get(IS_LOADING));
        verify(mDelegate).onQuerySubmitted("flight");

        mMediator.show(List.of());
        assertTrue(mModel.get(IS_LOADING));

        mMediator.show(
                List.of(
                        new AutofillSuggestion.Builder()
                                .setLabel("No data")
                                .setSubLabel("")
                                .build()));
        assertFalse(mModel.get(IS_LOADING));
    }

    @Test
    public void testOnQueryTextChanged() {
        mMediator.onQueryTextChanged("flight");
        assertEquals(1, mModelList.size());
        assertEquals(AtMemoryBottomSheetCoordinator.ITEM_TYPE_SEARCH_TILE, mModelList.get(0).type);
        assertEquals("flight", mModelList.get(0).model.get(TILE_TITLE));
        assertFalse(mModel.get(SHOW_SUGGESTIONS_BACKGROUND));

        mModelList.get(0).model.get(ON_TILE_CLICKED).run();
        verify(mHideKeyboardCallback).run();
        verify(mDelegate).onQuerySubmitted("flight");
    }

    @Test
    public void testOnQueryTextChanged_subsequentKeystrokes() {
        mMediator.onQueryTextChanged("f");
        assertEquals(1, mModelList.size());
        ListItem firstItem = mModelList.get(0);
        assertEquals("f", firstItem.model.get(TILE_TITLE));

        mMediator.onQueryTextChanged("fl");
        assertEquals(1, mModelList.size());
        assertTrue(firstItem == mModelList.get(0));
        assertEquals("fl", firstItem.model.get(TILE_TITLE));
    }

    @Test
    public void testOnQueryTextChanged_emptyQueryClearsList() {
        mMediator.onQueryTextChanged("f");
        assertEquals(1, mModelList.size());

        mMediator.onQueryTextChanged("");
        assertTrue(mModelList.isEmpty());
    }
}
