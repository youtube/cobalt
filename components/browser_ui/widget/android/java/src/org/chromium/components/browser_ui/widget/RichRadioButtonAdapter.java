// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget;

import android.view.View;
import android.view.ViewGroup;

import androidx.recyclerview.widget.RecyclerView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Objects;

/**
 * A RecyclerView adapter for displaying a list of {@link RichRadioButtonData} objects. It manages
 * single-item selection logic across the list and applies internal item orientation based on the
 * overall list layout mode.
 */
@NullMarked
public class RichRadioButtonAdapter
        extends RecyclerView.Adapter<RichRadioButtonAdapter.ViewHolder> {

    /** A listener interface for notifying the host of selected item changes. */
    public interface OnItemSelectedListener {
        void onItemSelected(String selectedId);
    }

    private final OnItemSelectedListener mListener;
    private final Map<String, Integer> mIdToPositionMap;
    private final List<RichRadioButtonData> mOptions;

    private @Nullable String mSelectedItemId;
    private int mSelectedPosition;

    /**
     * Creates a new RichRadioButtonAdapter with the given options.
     *
     * @param options The list of options to display.
     * @param listener The listener for selection changes.
     */
    public RichRadioButtonAdapter(
            List<RichRadioButtonData> options, OnItemSelectedListener listener) {
        mOptions = options;
        mListener = listener;
        mIdToPositionMap = new HashMap<>();

        initOptions();
    }

    private void initOptions() {

        buildIdToPositionMap();

        if (!mOptions.isEmpty()) {
            selectFirstItemAsDefault();
        }
    }

    private void selectFirstItemAsDefault() {
        if (mSelectedItemId == null && !mOptions.isEmpty()) {
            setSelection(0, mOptions.get(0).id);
        }
    }

    /**
     * Builds or rebuilds the ID to position map. This must be called whenever `mOptions` changes to
     * ensure the map is up-to-date.
     */
    private void buildIdToPositionMap() {
        mIdToPositionMap.clear();
        for (int i = 0; i < mOptions.size(); i++) {
            mIdToPositionMap.put(mOptions.get(i).id, i);
        }
    }

    /**
     * Sets the selected item by its ID.
     *
     * <p>If the {@code itemId} is found: the item at that ID is selected. If the {@code itemId} is
     * NOT found: - If no item is currently selected and the options list is not empty, the first
     * item in the list is selected by default. - Otherwise (an item is already selected, or the
     * options list is empty), no change occurs, and the selection remains as is.
     *
     * <p>The `onItemSelected` listener is only notified if the selection state genuinely changes.
     *
     * @param itemId The ID of the item to select.
     */
    public void setSelectedItem(String itemId) {
        Integer newPositionWrapper = mIdToPositionMap.get(itemId);

        assert newPositionWrapper != null
                : "Attempted to select an item with ID "
                        + itemId
                        + " that is not in the options list.";

        setSelection(newPositionWrapper, itemId);
    }

    /**
     * Internal method to manage selection state and notify UI/listener. This method ensures
     * `onItemSelected` is called ONLY if the selected item ID changes.
     *
     * @param newPosition The new selected position.
     * @param newSelectedItemId The new newSelectedItemId (can be null for no selection).
     */
    private void setSelection(int newPosition, @Nullable String newSelectedItemId) {
        @Nullable String oldSelectedItemIdBeforeUpdate = mSelectedItemId;
        int oldSelectedPositionBeforeUpdate = mSelectedPosition;

        if (newPosition == oldSelectedPositionBeforeUpdate
                && Objects.equals(newSelectedItemId, oldSelectedItemIdBeforeUpdate)) {
            return;
        }

        mSelectedItemId = newSelectedItemId;
        mSelectedPosition = newPosition;

        notifyItemChanged(oldSelectedPositionBeforeUpdate);
        notifyItemChanged(mSelectedPosition);

        if (mSelectedItemId != null) {
            mListener.onItemSelected(mSelectedItemId);
        }
    }

    @Override
    public int getItemViewType(int position) {
        return 0;
    }

    @Override
    public ViewHolder onCreateViewHolder(ViewGroup parent, int viewType) {
        View itemView = new RichRadioButton(parent.getContext());

        itemView.setLayoutParams(
                new ViewGroup.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));

        return new ViewHolder(itemView);
    }

    @Override
    public void onBindViewHolder(ViewHolder holder, int position) {
        RichRadioButtonData data = mOptions.get(position);
        RichRadioButton singleButton = (RichRadioButton) holder.itemView;

        singleButton.setChecked(false);

        singleButton.setItemData(data.iconResId, data.title, data.description);
        singleButton.setChecked(data.id.equals(mSelectedItemId));

        singleButton.setOnClickListener(
                v -> {
                    if (!data.id.equals(mSelectedItemId)) {
                        setSelection(position, data.id);
                    }
                });
    }

    @Override
    public int getItemCount() {
        return mOptions.size();
    }

    static class ViewHolder extends RecyclerView.ViewHolder {
        public ViewHolder(View itemView) {
            super(itemView);
        }
    }

    @Nullable
    String getSelectedItemIdForTesting() {
        return mSelectedItemId;
    }

    int getSelectedPositionForTesting() {
        return mSelectedPosition;
    }

    List<RichRadioButtonData> getOptionsForTesting() {
        return mOptions;
    }
}
