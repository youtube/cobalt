// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.signin_promo;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.DimenRes;

import org.chromium.chrome.browser.signin.services.DisplayableProfileData;
import org.chromium.chrome.browser.ui.signin.PersonalizedSigninPromoView;
import org.chromium.chrome.browser.ui.signin.R;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

final class SigninPromoViewBinder {
    public static void bind(
            PropertyModel model, PersonalizedSigninPromoView view, PropertyKey key) {
        Context context = view.getContext();
        if (key == SigninPromoProperties.PROFILE_DATA) {
            DisplayableProfileData profileData = model.get(SigninPromoProperties.PROFILE_DATA);
            if (profileData == null) {
                view.getImage().setImageResource(R.drawable.chrome_sync_logo);
                setImageSize(context, view, R.dimen.signin_promo_cold_state_image_size);
            } else {
                Drawable accountImage = profileData.getImage();
                view.getImage().setImageDrawable(accountImage);
                setImageSize(context, view, R.dimen.sync_promo_account_image_size);
            }
        } else if (key == SigninPromoProperties.ON_PRIMARY_BUTTON_CLICKED) {
            view.getPrimaryButton()
                    .setOnClickListener(model.get(SigninPromoProperties.ON_PRIMARY_BUTTON_CLICKED));
        } else if (key == SigninPromoProperties.ON_SECONDARY_BUTTON_CLICKED) {
            view.getSecondaryButton()
                    .setOnClickListener(
                            model.get(SigninPromoProperties.ON_SECONDARY_BUTTON_CLICKED));
        } else if (key == SigninPromoProperties.TITLE_TEXT) {
            view.getTitle().setText(model.get(SigninPromoProperties.TITLE_TEXT));
        } else if (key == SigninPromoProperties.DESCRIPTION_TEXT) {
            view.getDescription().setText(model.get(SigninPromoProperties.DESCRIPTION_TEXT));
        } else if (key == SigninPromoProperties.PRIMARY_BUTTON_TEXT) {
            view.getPrimaryButton().setText(model.get(SigninPromoProperties.DESCRIPTION_TEXT));
        } else if (key == SigninPromoProperties.SECONDARY_BUTTON_TEXT) {
            view.getSecondaryButton().setText(model.get(SigninPromoProperties.DESCRIPTION_TEXT));
        } else if (key == SigninPromoProperties.SHOULD_HIDE_SECONDARY_BUTTON) {
            int visibility =
                    model.get(SigninPromoProperties.SHOULD_HIDE_SECONDARY_BUTTON)
                            ? View.GONE
                            : View.VISIBLE;
            view.getSecondaryButton().setVisibility(visibility);
        } else if (key == SigninPromoProperties.SHOULD_HIDE_DISMISS_BUTTON) {
            view.getDismissButton()
                    .setVisibility(
                            model.get(SigninPromoProperties.SHOULD_HIDE_DISMISS_BUTTON)
                                    ? View.GONE
                                    : View.VISIBLE);
        } else {
            throw new IllegalArgumentException("Unknown property key: " + key);
        }
    }

    private static void setImageSize(
            Context context, PersonalizedSigninPromoView view, @DimenRes int dimenResId) {
        ViewGroup.LayoutParams layoutParams = view.getImage().getLayoutParams();
        layoutParams.height = context.getResources().getDimensionPixelSize(dimenResId);
        layoutParams.width = context.getResources().getDimensionPixelSize(dimenResId);
        view.getImage().setLayoutParams(layoutParams);
    }
}
