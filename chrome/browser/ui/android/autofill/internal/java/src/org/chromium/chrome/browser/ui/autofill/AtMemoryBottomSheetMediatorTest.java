// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.autofill;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.ui.autofill.AtMemoryBottomSheetCoordinator.ITEM_TYPE_ZERO_STATE;
import static org.chromium.chrome.browser.ui.autofill.AtMemoryBottomSheetProperties.IS_LOADING;
import static org.chromium.chrome.browser.ui.autofill.AtMemoryBottomSheetProperties.ON_QUERY_TEXT_CHANGED_CALLBACK;
import static org.chromium.chrome.browser.ui.autofill.AtMemoryBottomSheetProperties.VISIBLE;
import static org.chromium.chrome.browser.ui.autofill.AtMemoryBottomSheetSearchTileProperties.ON_TILE_CLICKED;
import static org.chromium.chrome.browser.ui.autofill.AtMemoryBottomSheetSearchTileProperties.TILE_TITLE;
import static org.chromium.chrome.browser.ui.autofill.AtMemoryBottomSheetSuggestionProperties.ON_FLYOUT_CLICKED;
import static org.chromium.chrome.browser.ui.autofill.AtMemoryBottomSheetSuggestionProperties.ON_SUGGESTION_CLICKED;
import static org.chromium.chrome.browser.ui.autofill.AtMemoryBottomSheetSuggestionProperties.TITLE;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.personal_context.first_run.PersonalContextFirstRunService;
import org.chromium.chrome.browser.personal_context.first_run.PersonalContextFirstRunServiceJni;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.autofill.internal.R;
import org.chromium.components.autofill.AutofillSuggestion;
import org.chromium.components.autofill.SuggestionType;
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
    @Mock private Profile mProfile;
    @Mock private PersonalContextFirstRunService.Natives mFirstRunServiceJniMock;

    private PropertyModel mModel;
    private ModelList mModelList;
    private AtMemoryBottomSheetMediator mMediator;

    @Before
    public void setUp() {
        PersonalContextFirstRunServiceJni.setInstanceForTesting(mFirstRunServiceJniMock);
        mModelList = new ModelList();
        mMediator =
                new AtMemoryBottomSheetMediator(
                        mProfile, mDelegate, mModelList, mHideKeyboardCallback);
        mModel = mMediator.getModel();
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
        verify(mDelegate).onQuerySubmitted("flight");

        when(mDelegate.isSearching()).thenReturn(true);
        mMediator.show(List.of());
        assertTrue(mModel.get(IS_LOADING));

        when(mDelegate.isSearching()).thenReturn(false);
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
        mModel.get(ON_QUERY_TEXT_CHANGED_CALLBACK).onResult("flight");
        verify(mDelegate).onQueryTextChanged("flight");

        mMediator.show(List.of(createSearchAffordance("flight")));
        assertEquals(1, mModelList.size());
        assertEquals(AtMemoryBottomSheetCoordinator.ITEM_TYPE_SEARCH_TILE, mModelList.get(0).type);
        assertEquals("flight", mModelList.get(0).model.get(TILE_TITLE));

        mModelList.get(0).model.get(ON_TILE_CLICKED).run();
        verify(mHideKeyboardCallback).run();
        verify(mDelegate).onQuerySubmitted("flight");
    }

    @Test
    public void testOnQueryTextChanged_subsequentKeystrokes() {
        mMediator.show(List.of(createSearchAffordance("f")));
        assertEquals(1, mModelList.size());
        ListItem firstItem = mModelList.get(0);
        assertEquals("f", firstItem.model.get(TILE_TITLE));

        mMediator.show(List.of(createSearchAffordance("fl")));
        assertEquals(1, mModelList.size());
        assertTrue(firstItem == mModelList.get(0));
        assertEquals("fl", firstItem.model.get(TILE_TITLE));
    }

    @Test
    public void testOnQueryTextChanged_emptyQueryShowsZeroState() {
        mMediator.show(List.of(createSearchAffordance("f")));
        assertEquals(1, mModelList.size());

        mMediator.show(List.of());
        assertEquals(1, mModelList.size());
        assertEquals(ITEM_TYPE_ZERO_STATE, mModelList.get(0).type);
    }

    private AutofillSuggestion createSearchAffordance(String query) {
        return new AutofillSuggestion.Builder()
                .setLabel(query)
                .setSubLabel("test details")
                .setIconId(R.drawable.flight)
                .setSuggestionType(SuggestionType.AT_MEMORY_SEARCH_AFFORDANCE)
                .build();
    }

    @Test
    public void testShow_emptySuggestionsShowsZeroState() {
        mMediator.show(List.of());

        assertEquals(1, mModelList.size());
        assertEquals(ITEM_TYPE_ZERO_STATE, mModelList.get(0).type);
    }

    @Test
    public void testNoticeShownAndDismissedAfterClick() {
        when(mFirstRunServiceJniMock.shouldShowNotice(mProfile)).thenReturn(true);

        AtMemoryBottomSheetMediator mediator =
                new AtMemoryBottomSheetMediator(
                        mProfile, mDelegate, mModelList, mHideKeyboardCallback);
        PropertyModel model = mediator.getModel();

        assertTrue(model.get(AtMemoryBottomSheetProperties.IS_NOTICE_VISIBLE));

        Runnable okClickListener =
                model.get(AtMemoryBottomSheetProperties.NOTICE_OK_CLICK_LISTENER);
        assertNotNull(okClickListener);
        okClickListener.run();

        assertFalse(model.get(AtMemoryBottomSheetProperties.IS_NOTICE_VISIBLE));
        verify(mFirstRunServiceJniMock).noticeAcknowledged(mProfile);
    }

    @Test
    public void testNoticeNotShown() {
        when(mFirstRunServiceJniMock.shouldShowNotice(mProfile)).thenReturn(false);

        AtMemoryBottomSheetMediator mediator =
                new AtMemoryBottomSheetMediator(
                        mProfile, mDelegate, mModelList, mHideKeyboardCallback);

        assertFalse(mediator.getModel().get(AtMemoryBottomSheetProperties.IS_NOTICE_VISIBLE));
    }
}
