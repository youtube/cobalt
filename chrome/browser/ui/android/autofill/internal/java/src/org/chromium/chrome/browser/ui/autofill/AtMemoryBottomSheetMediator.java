// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.autofill;

import static org.chromium.chrome.browser.ui.autofill.AtMemoryBottomSheetCoordinator.ITEM_TYPE_SEARCH_TILE;
import static org.chromium.chrome.browser.ui.autofill.AtMemoryBottomSheetCoordinator.ITEM_TYPE_SUGGESTION;
import static org.chromium.chrome.browser.ui.autofill.AtMemoryBottomSheetProperties.FLYOUT_SUGGESTIONS;
import static org.chromium.chrome.browser.ui.autofill.AtMemoryBottomSheetProperties.IS_LOADING;
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

import android.content.Context;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.ui.autofill.internal.R;
import org.chromium.components.autofill.AutofillSuggestion;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;

/** Contains the business logic for the AtMemoryBottomSheet. */
@NullMarked
class AtMemoryBottomSheetMediator {
    private final Context mContext;
    private final PropertyModel mModel;
    private final ModelList mModelList;
    private final AtMemoryBottomSheetCoordinator.Delegate mDelegate;
    private final Runnable mHideKeyboardCallback;

    AtMemoryBottomSheetMediator(
            Context context,
            AtMemoryBottomSheetCoordinator.Delegate delegate,
            PropertyModel model,
            ModelList modelList,
            Runnable hideKeyboardCallback) {
        mContext = context;
        mModel = model;
        mModelList = modelList;
        mDelegate = delegate;
        mHideKeyboardCallback = hideKeyboardCallback;

        mModel.set(ON_QUERY_SUBMITTED_CALLBACK, this::onQuerySubmitted);
        mModel.set(ON_QUERY_TEXT_CHANGED_CALLBACK, this::onQueryTextChanged);
    }

    void show(List<AutofillSuggestion> suggestions) {
        setSuggestions(suggestions);
        // When an async search begins, the backend clears old results by sending an empty list.
        // We only reset the loading indicator when non-empty suggestions arrive. Completed searches
        // always return non-empty lists (using fallback items when no data is found).
        if (!suggestions.isEmpty()) {
            mModel.set(IS_LOADING, false);
        }
        mModel.set(SHOW_SUGGESTIONS_BACKGROUND, !suggestions.isEmpty());
        mModel.set(VISIBLE, true);
    }

    void onDismissed() {
        mModelList.clear();
        mModel.set(IS_LOADING, false);
        mModel.set(SHOW_SUGGESTIONS_BACKGROUND, false);
        mModel.set(VISIBLE, false);
        mDelegate.onDismissed();
    }

    private void setSuggestions(List<AutofillSuggestion> suggestions) {
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
        mModel.set(IS_LOADING, true);
        mDelegate.onQuerySubmitted(query);
    }

    void onQueryTextChanged(String query) {
        mModel.set(SHOW_SUGGESTIONS_BACKGROUND, false);
        // Update the existing search tile in place rather than re-creating it to avoid UI
        // flickering.
        if (!query.isEmpty() && isSearchTileAlreadyVisible()) {
            mModelList.get(0).model.set(TILE_TITLE, query);
            return;
        }
        mModelList.clear();
        if (query.isEmpty()) {
            return;
        }
        PropertyModel itemModel =
                new PropertyModel.Builder(AtMemoryBottomSheetSearchTileProperties.ALL_KEYS)
                        .with(TILE_ICON, R.drawable.ic_spark_24dp)
                        .with(TILE_TITLE, query)
                        .with(
                                TILE_DETAILS,
                                mContext.getString(R.string.autofill_at_memory_search_tile_details))
                        .with(ON_TILE_CLICKED, this::onSearchTileClicked)
                        .build();
        mModelList.add(new ListItem(ITEM_TYPE_SEARCH_TILE, itemModel));
    }

    private boolean isSearchTileAlreadyVisible() {
        return mModelList.size() == 1 && mModelList.get(0).type == ITEM_TYPE_SEARCH_TILE;
    }

    private void onSearchTileClicked() {
        if (mModelList.isEmpty()) return;

        String query = mModelList.get(0).model.get(TILE_TITLE);
        if (query == null) return;

        mHideKeyboardCallback.run();
        onQuerySubmitted(query);
    }
}
