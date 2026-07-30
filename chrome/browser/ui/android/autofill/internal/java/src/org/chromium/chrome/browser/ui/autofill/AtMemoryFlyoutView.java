// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.autofill;

import android.content.Context;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.constraintlayout.helper.widget.Flow;
import androidx.constraintlayout.widget.ConstraintLayout;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ui.autofill.internal.R;
import org.chromium.components.autofill.AutofillSuggestion;
import org.chromium.components.browser_ui.widget.chips.ChipView;

import java.util.ArrayList;
import java.util.List;

/** View wrapper for the flyout screen of the @memory bottom sheet. */
@NullMarked
public class AtMemoryFlyoutView extends LinearLayout {
    private @Nullable ConstraintLayout mChipsContainer;
    private @Nullable Flow mChipsFlow;
    private final List<ChipView> mActiveChips = new ArrayList<>();

    public AtMemoryFlyoutView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mChipsContainer = findViewById(R.id.flyout_chips_container);
        mChipsFlow = findViewById(R.id.chips_flow);
    }

    /** Populates the flyout screen with chip suggestions. */
    public void setSuggestions(List<AutofillSuggestion> suggestions) {
        if (mChipsContainer == null || mChipsFlow == null) return;

        for (ChipView chip : mActiveChips) {
            mChipsContainer.removeView(chip);
        }
        mActiveChips.clear();

        int[] ids = new int[suggestions.size()];
        for (int i = 0; i < suggestions.size(); i++) {
            AutofillSuggestion suggestion = suggestions.get(i);
            ChipView chip = createFlyoutChipView(mChipsContainer, suggestion);
            ids[i] = chip.getId();
            mChipsContainer.addView(chip);
            mActiveChips.add(chip);
        }
        mChipsFlow.setReferencedIds(ids);
    }

    private ChipView createFlyoutChipView(ViewGroup parent, AutofillSuggestion suggestion) {
        Context context = parent.getContext();
        ChipView chip =
                (ChipView)
                        LayoutInflater.from(context)
                                .inflate(
                                        R.layout.at_memory_flyout_chip,
                                        parent,
                                        /* attachToRoot= */ false);
        chip.setId(View.generateViewId());

        TextView primaryTextView = chip.getPrimaryTextView();
        primaryTextView.setText(suggestion.getLabel());

        TextView secondaryTextView = chip.getSecondaryTextView();
        if (!TextUtils.isEmpty(suggestion.getSublabel())) {
            secondaryTextView.setText(suggestion.getSublabel());
            secondaryTextView.setVisibility(View.VISIBLE);
        } else {
            secondaryTextView.setVisibility(View.GONE);
        }

        return chip;
    }

    @SuppressWarnings("unused")
    private void alignFlyoutChipHeights() {
        // TODO(crbug.com/513146609): Implement the logic for the same height chips alignment.
    }
}
