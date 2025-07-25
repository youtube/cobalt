// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.modelutil;

import android.util.Pair;
import android.util.SparseArray;
import android.view.View;
import android.view.ViewGroup;
import android.widget.BaseAdapter;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.ui.R;
import org.chromium.ui.modelutil.ListObservable.ListObserver;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor.ViewBinder;

import java.util.Collection;

/**
 * Adapter for providing data and views to a ListView.
 *
 * To use, register a {@link PropertyModelChangeProcessor.ViewBinder} and {@link ViewBuilder}
 * for each view type in the list using
 * {@link #registerType(int, ViewBuilder, PropertyModelChangeProcessor.ViewBinder)}.
 * The constructor takes a {@link ListObservable} list in the form of a {@link ModelList}. Any
 * changes that occur in the list will be automatically updated in the view.
 *
 * When creating a new view, ModelListAdapter will bind all set properties. When reusing/rebinding
 * a view, in addition to binding all properties set on the new model, properties that were
 * previously set on the old model but are not set on the new model will be bound to "reset" the
 * view. ViewBinders registered for this adapter may therefore need to handle bind calls for
 * properties that are not set on the model being bound.
 *
 * Additionally, ModelListAdapter will hook up a {@link PropertyModelChangeProcessor} when binding
 * views to ensure that changes to the PropertyModel for that list item are bound to the view.
 */
public class ModelListAdapter extends BaseAdapter implements MVCListAdapter {
    private final ModelList mModelList;
    private final SparseArray<Pair<ViewBuilder, ViewBinder>> mViewBuilderMap = new SparseArray<>();
    private final ListObserver<Void> mListObserver;

    public ModelListAdapter(ModelList data) {
        mModelList = data;
        mListObserver = new ListObserver<Void>() {
            @Override
            public void onItemRangeInserted(ListObservable source, int index, int count) {
                notifyDataSetChanged();
            }

            @Override
            public void onItemRangeRemoved(ListObservable source, int index, int count) {
                notifyDataSetChanged();
            }

            @Override
            public void onItemRangeChanged(
                    ListObservable<Void> source, int index, int count, @Nullable Void payload) {
                notifyDataSetChanged();
            }

            @Override
            public void onItemMoved(ListObservable source, int curIndex, int newIndex) {
                notifyDataSetChanged();
            }
        };
        mModelList.addObserver(mListObserver);
    }

    @Override
    public int getCount() {
        return mModelList.size();
    }

    @Override
    public Object getItem(int position) {
        return mModelList.get(position);
    }

    @Override
    public long getItemId(int position) {
        return position;
    }

    @Override
    public <T extends View> void registerType(
            int typeId, ViewBuilder<T> builder, ViewBinder<PropertyModel, T, PropertyKey> binder) {
        assert mViewBuilderMap.get(typeId) == null;
        mViewBuilderMap.put(typeId, new Pair<>(builder, binder));
    }

    @Override
    public int getItemViewType(int position) {
        return mModelList.get(position).type;
    }

    @Override
    public int getViewTypeCount() {
        return Math.max(1, mViewBuilderMap.size());
    }

    /**
     * Make an attempt to convert view to desiredType.
     *
     * The basic implementation verifies whether the view can be re-used as is without any
     * modifications, assuming the current view type is same as the desired view type.
     * Subclasses should override this method if any specific changes can to be made in order
     * to convert views from one type to another.
     *
     * @param view View to convert
     * @param desiredType Target type of the view to convert to.
     * @return Whether conversion was successful.
     */
    protected boolean canReuseView(View view, int desiredType) {
        // Check if view type changed. If not, we can re-use this view as is without any
        // modifications.
        return view != null && view.getTag(R.id.view_type) != null
                && (int) view.getTag(R.id.view_type) == desiredType;
    }

    /**
     * Create a new view of the desired type.
     *
     * @param parent Parent view.
     * @param typeId Type of the view to create.
     * @return Created view.
     */
    protected View createView(ViewGroup parent, int typeId) {
        return mViewBuilderMap.get(typeId).first.buildView(parent);
    }

    @SuppressWarnings("unchecked")
    @Override
    public View getView(int position, View convertView, ViewGroup parent) {
        //  1. Destroy the old PropertyModelChangeProcessor if it exists.
        if (convertView != null && convertView.getTag(R.id.view_mcp) != null) {
            PropertyModelChangeProcessor propertyModelChangeProcessor =
                    (PropertyModelChangeProcessor) convertView.getTag(R.id.view_mcp);
            propertyModelChangeProcessor.destroy();
        }

        // 2. Build a new view if needed. Otherwise, fetch the old model from the convertView.
        PropertyModel oldModel = null;
        final int desiredViewType = getItemViewType(position);

        if (convertView == null || !canReuseView(convertView, desiredViewType)) {
            convertView = createView(parent, desiredViewType);
            // Since the view type returned by getView is not guaranteed to return a view of that
            // type, we need a means of checking it. The "view_type" tag is attached to the views
            // and identify what type the view is. This should allow lists that aren't necessarily
            // recycler views to work correctly with heterogeneous lists.
            convertView.setTag(R.id.view_type, desiredViewType);
        } else {
            oldModel = (PropertyModel) convertView.getTag(R.id.view_model);
        }

        PropertyModel model = mModelList.get(position).model;
        PropertyModelChangeProcessor.ViewBinder binder =
                mViewBuilderMap.get(mModelList.get(position).type).second;

        // 3. Attach a PropertyModelChangeProcessor and PropertyModel to the view (for #1/2 above
        //    when re-using a view).
        convertView.setTag(R.id.view_mcp,
                PropertyModelChangeProcessor.create(
                        model, convertView, binder, /* performInitialBind = */ false));
        convertView.setTag(R.id.view_model, model);

        // 4. Bind properties to the convertView.
        bindNewModel(model, oldModel, convertView, binder);

        // TODO(tedchoc): Investigate whether this is still needed.
        convertView.jumpDrawablesToCurrentState();

        return convertView;
    }

    /**
     * Binds all set properties to the view. If oldModel is not null, binds properties that were
     * previously set in the oldModel but are not set in the new model.
     *
     * @param newModel The new model to bind to {@code view}.
     * @param oldModel The old model previously bound to {@code view}. May be null.
     * @param view The view to bind.
     * @param binder The ViewBinder that will bind model properties to {@code view}.
     */
    @VisibleForTesting
    static void bindNewModel(PropertyModel newModel, @Nullable PropertyModel oldModel, View view,
            PropertyModelChangeProcessor.ViewBinder binder) {
        Collection<PropertyKey> setProperties = newModel.getAllSetProperties();
        for (PropertyKey key : newModel.getAllProperties()) {
            if (oldModel != null) {
                // Skip binding properties that haven't changed.
                if (newModel.compareValue(oldModel, key)) {
                    continue;
                }
            } else if (!setProperties.contains(key)) {
                // If there is no previous model, skip binding properties that haven't been set.
                continue;
            }

            binder.bind(newModel, view, key);
        }
    }
}
