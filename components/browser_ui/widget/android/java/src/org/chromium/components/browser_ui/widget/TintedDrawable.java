// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget;

import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.PorterDuff;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.VectorDrawable;

import androidx.annotation.ColorInt;
import androidx.annotation.DrawableRes;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.vectordrawable.graphics.drawable.VectorDrawableCompat;

/**
 * Implementation of BitmapDrawable that allows to tint the color of the drawable for all
 * bitmap drawable states.
 */
public class TintedDrawable extends BitmapDrawable {
    /**
     * The set of colors that just be used for tinting this bitmap drawable.
     */
    protected ColorStateList mTint;

    public TintedDrawable(Context context, Bitmap bitmap) {
        super(context.getResources(), bitmap);
        mTint = AppCompatResources.getColorStateList(context, R.color.default_icon_color_tint_list);
    }

    @Override
    protected boolean onStateChange(int[] state) {
        boolean ret = updateTintColor();
        super.onStateChange(state);
        return ret;
    }

    @Override
    public boolean isStateful() {
        return true;
    }

    /**
     * Sets the tint color for the given Drawable for all button states.
     * @param tint The set of colors to use to color the ImageButton.
     */
    public void setTint(ColorStateList tint) {
        if (mTint == tint) return;
        mTint = tint;
        updateTintColor();
    }

    /**
     * Sets the tint color for the given Drawable for all states.
     * @param tint The tint.
     */
    @Override
    public void setTint(@ColorInt int tint) {
        setTint(ColorStateList.valueOf(tint));
    }

    /**
     * Factory method for creating a {@link TintedDrawable} with a resource id.
     */
    public static TintedDrawable constructTintedDrawable(Context context, int drawableId) {
        assert !isVectorDrawable(context, drawableId)
            : "TintedDrawable doesn't support "
              + "VectorDrawables! Please use UiUtils.getTintedDrawable() instead.";
        Bitmap icon = BitmapFactory.decodeResource(context.getResources(), drawableId);
        return new TintedDrawable(context, icon);
    }

    /**
     * Factory method for creating a {@link TintedDrawable} with a resource id and specific tint.
     */
    public static TintedDrawable constructTintedDrawable(
            Context context, int drawableId, int tintColorId) {
        TintedDrawable drawable = constructTintedDrawable(context, drawableId);
        drawable.setTint(AppCompatResources.getColorStateList(context, tintColorId));
        return drawable;
    }

    private boolean updateTintColor() {
        if (mTint == null) return false;
        setColorFilter(mTint.getColorForState(getState(), 0), PorterDuff.Mode.SRC_IN);
        return true;
    }

    /**
     * Only called in debug builds to ensure that TintedDrawable isn't constructed for a vector
     * grahpic.
     * @param context A {@link Context} used to load the Drawable.
     * @param drawableId A {@link DrawableRes} to load and check whether it's a vector graphic.
     * @return True iff the loaded resource is either a {@link VectorDrawableCompat} or
     * a {@link VectorDrawable}. The latter is only checked for Android L and later.
     */
    private static boolean isVectorDrawable(Context context, @DrawableRes int drawableId) {
        Drawable drawable = AppCompatResources.getDrawable(context, drawableId);
        return drawable instanceof VectorDrawableCompat || drawable instanceof VectorDrawable;
    }
}
