// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.autofill;

import static org.chromium.chrome.browser.ui.autofill.AtMemoryBottomSheetCoordinator.ITEM_TYPE_SEARCH_TILE;
import static org.chromium.chrome.browser.ui.autofill.AtMemoryBottomSheetCoordinator.ITEM_TYPE_SUGGESTION;
import static org.chromium.chrome.browser.ui.autofill.AtMemoryBottomSheetCoordinator.ITEM_TYPE_ZERO_STATE;
import static org.chromium.chrome.browser.ui.autofill.AtMemoryBottomSheetProperties.FLYOUT_SUGGESTIONS;
import static org.chromium.chrome.browser.ui.autofill.AtMemoryBottomSheetProperties.IS_LOADING;
import static org.chromium.chrome.browser.ui.autofill.AtMemoryBottomSheetProperties.IS_NOTICE_VISIBLE;
import static org.chromium.chrome.browser.ui.autofill.AtMemoryBottomSheetProperties.NOTICE_OK_CLICK_LISTENER;
import static org.chromium.chrome.browser.ui.autofill.AtMemoryBottomSheetProperties.ON_QUERY_SUBMITTED_CALLBACK;
import static org.chromium.chrome.browser.ui.autofill.AtMemoryBottomSheetProperties.ON_QUERY_TEXT_CHANGED_CALLBACK;
import static org.chromium.chrome.browser.ui.autofill.AtMemoryBottomSheetProperties.SHOW_SUGGESTIONS_BACKGROUND;
import static org.chromium.chrome.browser.ui.autofill.AtMemoryBottomSheetProperties.VISIBLE;
import static org.chromium.chrome.browser.ui.autofill.AtMemoryBottomSheetSearchTileProperties.ON_TILE_CLICKED;
import static org.chromium.chrome.browser.ui.autofill.AtMemoryBottomSheetSearchTileProperties.TILE_DETAILS;
import static org.chromium.chrome.browser.ui.autofill.AtMemoryBottomSheetSearchTileProperties.TILE_ICON;
import static org.chromium.chrome.browser.ui.autofill.AtMemoryBottomSheetSearchTileProperties.TILE_TITLE;
import static org.chromium.chrome.browser.ui.autofill.AtMemoryBottomSheetSuggestionProperties.DETAILS;
import static org.chromium.chrome.browser.ui.autofill.AtMemoryBottomSheetSuggestionProperties.ICON;
import static org.chromium.chrome.browser.ui.autofill.AtMemoryBottomSheetSuggestionProperties.ON_FLYOUT_CLICKED;
import static org.chromium.chrome.browser.ui.autofill.AtMemoryBottomSheetSuggestionProperties.ON_SUGGESTION_CLICKED;
import static org.chromium.chrome.browser.ui.autofill.AtMemoryBottomSheetSuggestionProperties.TITLE;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.personal_context.first_run.PersonalContextFirstRunService;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.autofill.AutofillSuggestion;
import org.chromium.components.autofill.SuggestionType;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;

/** Contains the business logic for the AtMemoryBottomSheet. */
@NullMarked
class AtMemoryBottomSheetMediator {
    private final PropertyModel mModel;
    private final ModelList mModelList;
    private final AtMemoryBottomSheetCoordinator.Delegate mDelegate;
    private final Profile mProfile;
    private final Runnable mHideKeyboardCallback;

    AtMemoryBottomSheetMediator(
            Profile profile,
            AtMemoryBottomSheetCoordinator.Delegate delegate,
            ModelList modelList,
            Runnable hideKeyboardCallback) {
        mProfile = profile;
        mModelList = modelList;
        mDelegate = delegate;
        mHideKeyboardCallback = hideKeyboardCallback;

        boolean shouldShowNotice = PersonalContextFirstRunService.shouldShowNotice(mProfile);

        mModel =
                new PropertyModel.Builder(AtMemoryBottomSheetProperties.ALL_KEYS)
                        .with(VISIBLE, false)
                        .with(SHOW_SUGGESTIONS_BACKGROUND, false)
                        .with(ON_QUERY_SUBMITTED_CALLBACK, this::onQuerySubmitted)
                        .with(ON_QUERY_TEXT_CHANGED_CALLBACK, mDelegate::onQueryTextChanged)
                        .with(IS_NOTICE_VISIBLE, shouldShowNotice)
                        .with(NOTICE_OK_CLICK_LISTENER, this::onNoticeAcknowledged)
                        .build();
    }

    PropertyModel getModel() {
        return mModel;
    }

    void show(List<AutofillSuggestion> suggestions) {
        applyScreenState(getScreenState(suggestions), suggestions);
        mModel.set(VISIBLE, true);
    }

    void onDismissed() {
        applyScreenState(AtMemoryScreenState.HIDDEN, List.of());
        mModel.set(VISIBLE, false);
        mDelegate.onDismissed();
    }

    private void onNoticeAcknowledged() {
        mModel.set(IS_NOTICE_VISIBLE, false);
        PersonalContextFirstRunService.noticeAcknowledged(mProfile);
    }

    private AtMemoryScreenState getScreenState(List<AutofillSuggestion> suggestions) {
        if (isSearchAffordance(suggestions)) {
            return AtMemoryScreenState.SEARCH_AFFORDANCE;
        }
        if (suggestions.isEmpty()) {
            return mDelegate.isSearching()
                    ? AtMemoryScreenState.LOADING
                    : AtMemoryScreenState.ZERO_STATE;
        }
        return AtMemoryScreenState.SUGGESTIONS;
    }

    private void applyScreenState(
            AtMemoryScreenState screenState, List<AutofillSuggestion> suggestions) {
        mModel.set(IS_LOADING, screenState.isLoading);
        mModel.set(SHOW_SUGGESTIONS_BACKGROUND, screenState.showSuggestionsBackground);

        if (screenState.showZeroState) {
            applyZeroState();
        }
        if (screenState.showSearchAffordance) {
            applySearchAffordance(suggestions.get(0));
        }
        if (screenState.showAtMemorySuggestions) {
            applySuggestions(suggestions);
        }
        if (screenState == AtMemoryScreenState.HIDDEN) {
            mModelList.clear();
        }
    }

    private void applyZeroState() {
        if (mModelList.size() == 1 && mModelList.get(0).type == ITEM_TYPE_ZERO_STATE) {
            return;
        }
        mModelList.clear();
        mModelList.add(new ListItem(ITEM_TYPE_ZERO_STATE, new PropertyModel()));
    }

    private void applySearchAffordance(AutofillSuggestion affordance) {
        if (!mModelList.isEmpty() && mModelList.get(0).type == ITEM_TYPE_SEARCH_TILE) {
            mModelList.get(0).model.set(TILE_TITLE, affordance.getLabel());
            if (mModelList.size() > 1) {
                mModelList.removeRange(1, mModelList.size() - 1);
            }
            return;
        }
        mModelList.clear();
        PropertyModel itemModel =
                new PropertyModel.Builder(AtMemoryBottomSheetSearchTileProperties.ALL_KEYS)
                        .with(TILE_ICON, affordance.getIconId())
                        .with(TILE_TITLE, affordance.getLabel())
                        .with(TILE_DETAILS, affordance.getSublabel())
                        .with(ON_TILE_CLICKED, this::onSearchTileClicked)
                        .build();
        mModelList.add(new ListItem(ITEM_TYPE_SEARCH_TILE, itemModel));
    }

    private void applySuggestions(List<AutofillSuggestion> suggestions) {
        mModelList.clear();
        for (int i = 0; i < suggestions.size(); i++) {
            AutofillSuggestion suggestion = suggestions.get(i);
            int position = i;
            PropertyModel itemModel =
                    new PropertyModel.Builder(AtMemoryBottomSheetSuggestionProperties.ALL_KEYS)
                            .with(ICON, suggestion.getIconId())
                            .with(TITLE, suggestion.getLabel())
                            .with(DETAILS, suggestion.getSublabel())
                            .with(ON_SUGGESTION_CLICKED, () -> onSuggestionClicked(position))
                            .with(ON_FLYOUT_CLICKED, () -> onFlyoutClicked(suggestion))
                            .build();
            mModelList.add(new ListItem(ITEM_TYPE_SUGGESTION, itemModel));
        }
    }

    private void onSuggestionClicked(int position) {
        mDelegate.onSuggestionClicked(position);
    }

    private void onFlyoutClicked(AutofillSuggestion suggestion) {
        // TODO(crbug.com/505255929): Once AutofillSuggestion supports child/sub-suggestions,
        // set the title, source, and suggestions directly. For now, pass placeholder values for
        // testing.
        mModel.set(FLYOUT_SUGGESTIONS, List.of(suggestion));
    }

    void onQuerySubmitted(String query) {
        mDelegate.onQuerySubmitted(query);
    }

    private void onSearchTileClicked() {
        if (mModelList.isEmpty()) return;

        String query = mModelList.get(0).model.get(TILE_TITLE);
        if (query == null) return;

        mHideKeyboardCallback.run();
        onQuerySubmitted(query);
    }

    private boolean isSearchAffordance(List<AutofillSuggestion> suggestions) {
        return suggestions.size() == 1
                && suggestions.get(0).getSuggestionType()
                        == SuggestionType.AT_MEMORY_SEARCH_AFFORDANCE;
    }
}
