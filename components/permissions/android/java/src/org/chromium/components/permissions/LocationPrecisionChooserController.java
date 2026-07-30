// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.permissions;

import android.content.Context;
import android.view.View;
import android.widget.LinearLayout;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.widget.RichRadioButtonData;
import org.chromium.components.browser_ui.widget.RichRadioButtonList;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.function.Consumer;

/**
 * Controller for the location precision chooser UI (radio buttons). Responsible for creating,
 * displaying, and managing the state of the precision selection.
 */
@NullMarked
public class LocationPrecisionChooserController {

    private final Context mContext;
    private final LinearLayout mContainer;
    private final @LocationAccuracy int mInitialSelection;
    private final @Nullable Consumer<Integer> mSelectionListener;
    private final List<RichRadioButtonData> mOptionsToDisplay;
    private final Map<String, Integer> mIdToAccuracyMap;

    private @Nullable RichRadioButtonList mRichRadioButtonList;

    public LocationPrecisionChooserController(
            Context context,
            LinearLayout container,
            @LocationAccuracy int initialSelection,
            @Nullable Consumer<Integer> selectionListener) {

        mContext = context;
        mContainer = container;
        mInitialSelection = initialSelection;
        mSelectionListener = selectionListener;

        mIdToAccuracyMap = new HashMap<>();
        mOptionsToDisplay = buildRichRadioButtonOptions();
    }

    public void show() {
        if (mContainer == null) {
            return;
        }

        mContainer.removeAllViews();
        mContainer.setVisibility(View.VISIBLE);

        mRichRadioButtonList = new RichRadioButtonList(mContext);
        mRichRadioButtonList.setLayoutParams(
                new LinearLayout.LayoutParams(
                        LinearLayout.LayoutParams.MATCH_PARENT,
                        LinearLayout.LayoutParams.WRAP_CONTENT));

        mRichRadioButtonList.initialize(
                mOptionsToDisplay,
                (selectedId) -> {
                    if (mSelectionListener != null) {
                        @LocationAccuracy Integer accuracy = mIdToAccuracyMap.get(selectedId);
                        if (accuracy != null) {
                            mSelectionListener.accept(accuracy);
                        }
                    }
                });

        String initialSelectionId = accuracyToId(mInitialSelection);
        if (initialSelectionId != null) {
            mRichRadioButtonList.setSelectedItem(initialSelectionId);
        }

        mContainer.addView(mRichRadioButtonList);
    }

    /** Builds the list of {@link RichRadioButtonData} options for the chooser. */
    private List<RichRadioButtonData> buildRichRadioButtonOptions() {
        List<RichRadioButtonData> options = new ArrayList<>();

        final String preciseId = "precise_location_option";
        final String approximateId = "approximate_location_option";

        mIdToAccuracyMap.put(preciseId, LocationAccuracy.PRECISE);
        mIdToAccuracyMap.put(approximateId, LocationAccuracy.APPROXIMATE);

        RichRadioButtonData.Builder preciseOptionBuilder =
                new RichRadioButtonData.Builder(
                        preciseId, mContext.getString(R.string.permission_allow_precise_geo));
        RichRadioButtonData.Builder approximateOptionBuilder =
                new RichRadioButtonData.Builder(
                        approximateId,
                        mContext.getString(R.string.permission_allow_approximate_geo));

        preciseOptionBuilder
                .setIconResId(R.drawable.location_precise)
                .setDescription(
                        mContext.getString(R.string.permission_allow_precise_geo_description));
        approximateOptionBuilder
                .setIconResId(R.drawable.location_approximate)
                .setDescription(
                        mContext.getString(R.string.permission_allow_approximate_geo_description));

        options.add(preciseOptionBuilder.build());
        options.add(approximateOptionBuilder.build());
        return options;
    }

    private @Nullable String accuracyToId(@LocationAccuracy int accuracy) {
        for (Map.Entry<String, Integer> entry : mIdToAccuracyMap.entrySet()) {
            if (entry.getValue().equals(accuracy)) {
                return entry.getKey();
            }
        }
        return null;
    }
}
