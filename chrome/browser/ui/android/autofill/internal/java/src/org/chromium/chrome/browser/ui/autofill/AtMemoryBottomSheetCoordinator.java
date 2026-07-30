// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.autofill;

import android.content.Context;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.autofill.internal.R;
import org.chromium.components.autofill.AutofillSuggestion;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.ui.modelutil.LayoutViewBuilder;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

import java.util.List;

/** Coordinator for the AtMemoryBottomSheet. */
@NullMarked
public class AtMemoryBottomSheetCoordinator {
    private final AtMemoryBottomSheetContent mContent;
    private final AtMemoryBottomSheetMediator mMediator;
    private final BottomSheetController mBottomSheetController;

    public static final int ITEM_TYPE_SUGGESTION = 1;
    public static final int ITEM_TYPE_SEARCH_TILE = 2;
    public static final int ITEM_TYPE_ZERO_STATE = 3;

    private final BottomSheetObserver mBottomSheetObserver =
            new EmptyBottomSheetObserver() {
                @Override
                public void onSheetClosed(@BottomSheetController.StateChangeReason int reason) {
                    super.onSheetClosed(reason);
                    if (mBottomSheetController.getCurrentSheetContent() != null
                            && mBottomSheetController.getCurrentSheetContent() == mContent) {
                        onDismissed();
                    }
                }
            };

    /** Delegate to receive events from the bottom sheet. */
    interface Delegate {
        void onDismissed();

        void onQuerySubmitted(String query);

        void onQueryTextChanged(String query);

        void onSuggestionClicked(int position);

        boolean isSearching();
    }

    AtMemoryBottomSheetCoordinator(
            Context context,
            BottomSheetController sheetController,
            Delegate delegate,
            Profile profile) {
        mBottomSheetController = sheetController;

        AtMemoryBottomSheetView view = new AtMemoryBottomSheetView(context);

        ModelList modelList = new ModelList();
        mMediator =
                new AtMemoryBottomSheetMediator(
                        profile, delegate, modelList, view::hideKeyboardAndClearFocus);

        SimpleRecyclerViewAdapter adapter = new SimpleRecyclerViewAdapter(modelList);
        adapter.registerType(
                ITEM_TYPE_SUGGESTION,
                new LayoutViewBuilder<>(R.layout.at_memory_bottom_sheet_suggestion_item),
                AtMemoryBottomSheetSuggestionViewBinder::bind);
        adapter.registerType(
                ITEM_TYPE_SEARCH_TILE,
                new LayoutViewBuilder<>(R.layout.at_memory_bottom_sheet_search_item),
                AtMemoryBottomSheetSearchTileViewBinder::bind);
        adapter.registerType(
                ITEM_TYPE_ZERO_STATE,
                new LayoutViewBuilder<>(R.layout.at_memory_bottom_sheet_zero_state_item),
                // Zero-state illustration and text are static in the layout, so no view binding is
                // needed.
                (m, v, k) -> {});
        view.setRecyclerViewAdapter(adapter);

        mContent = new AtMemoryBottomSheetContent(view.getContentView(), mBottomSheetController);

        PropertyModelChangeProcessor.create(
                mMediator.getModel(), view, AtMemoryBottomSheetViewBinder::bind);
    }

    public void show(List<AutofillSuggestion> suggestions) {
        mBottomSheetController.addObserver(mBottomSheetObserver);
        if (mBottomSheetController.requestShowContent(mContent, /* animate= */ true)) {
            mMediator.show(suggestions);
        } else {
            onDismissed();
        }
    }

    public void hide() {
        mBottomSheetController.hideContent(mContent, /* animate= */ true);
    }

    private void onDismissed() {
        mBottomSheetController.removeObserver(mBottomSheetObserver);
        mMediator.onDismissed();
    }

    AtMemoryBottomSheetContent getBottomSheetContentForTesting() {
        return mContent;
    }
}
