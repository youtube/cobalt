// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.search;

import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.MarginLayoutParams;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.core.widget.ImageViewCompat;

import org.chromium.chrome.R;
import org.chromium.ui.base.ViewUtils;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * Responsible for building and setting properties on the search box on new tab page.
 */
class SearchBoxViewBinder
        implements PropertyModelChangeProcessor.ViewBinder<PropertyModel, View, PropertyKey> {
    @Override
    public final void bind(PropertyModel model, View view, PropertyKey propertyKey) {
        ImageView voiceSearchButton =
                view.findViewById(org.chromium.chrome.R.id.voice_search_button);
        ImageView lensButton = view.findViewById(org.chromium.chrome.R.id.lens_camera_button);
        View searchBoxContainer = view;
        final TextView searchBoxTextView = searchBoxContainer.findViewById(R.id.search_box_text);

        if (SearchBoxProperties.VISIBILITY == propertyKey) {
            searchBoxContainer.setVisibility(
                    model.get(SearchBoxProperties.VISIBILITY) ? View.VISIBLE : View.GONE);
        } else if (SearchBoxProperties.ALPHA == propertyKey) {
            searchBoxContainer.setAlpha(model.get(SearchBoxProperties.ALPHA));
            // Disable the search box contents if it is the process of being animated away.
            ViewUtils.setEnabledRecursive(
                    searchBoxContainer, searchBoxContainer.getAlpha() == 1.0f);
        } else if (SearchBoxProperties.BACKGROUND == propertyKey) {
            searchBoxContainer.setBackground(model.get(SearchBoxProperties.BACKGROUND));
        } else if (SearchBoxProperties.VOICE_SEARCH_COLOR_STATE_LIST == propertyKey) {
            ImageViewCompat.setImageTintList(voiceSearchButton,
                    model.get(SearchBoxProperties.VOICE_SEARCH_COLOR_STATE_LIST));
            ImageViewCompat.setImageTintList(
                    lensButton, model.get(SearchBoxProperties.VOICE_SEARCH_COLOR_STATE_LIST));
        } else if (SearchBoxProperties.VOICE_SEARCH_DRAWABLE == propertyKey) {
            voiceSearchButton.setImageDrawable(
                    model.get(SearchBoxProperties.VOICE_SEARCH_DRAWABLE));
        } else if (SearchBoxProperties.VOICE_SEARCH_VISIBILITY == propertyKey) {
            voiceSearchButton.setVisibility(model.get(SearchBoxProperties.VOICE_SEARCH_VISIBILITY)
                            ? View.VISIBLE
                            : View.GONE);
        } else if (SearchBoxProperties.LENS_VISIBILITY == propertyKey) {
            lensButton.setVisibility(
                    model.get(SearchBoxProperties.LENS_VISIBILITY) ? View.VISIBLE : View.GONE);
        } else if (SearchBoxProperties.LENS_CLICK_CALLBACK == propertyKey) {
            lensButton.setOnClickListener(model.get(SearchBoxProperties.LENS_CLICK_CALLBACK));
        } else if (SearchBoxProperties.SEARCH_BOX_CLICK_CALLBACK == propertyKey) {
            searchBoxTextView.setOnClickListener(
                    model.get(SearchBoxProperties.SEARCH_BOX_CLICK_CALLBACK));
        } else if (SearchBoxProperties.SEARCH_BOX_TEXT_WATCHER == propertyKey) {
            searchBoxTextView.addTextChangedListener(
                    model.get(SearchBoxProperties.SEARCH_BOX_TEXT_WATCHER));
        } else if (SearchBoxProperties.SEARCH_TEXT == propertyKey) {
            searchBoxTextView.setText(model.get(SearchBoxProperties.SEARCH_TEXT));
        } else if (SearchBoxProperties.SEARCH_HINT_VISIBILITY == propertyKey) {
            boolean isHintVisible = model.get(SearchBoxProperties.SEARCH_HINT_VISIBILITY);
            searchBoxTextView.setHint(isHintVisible
                            ? view.getContext().getString(
                                    org.chromium.chrome.R.string.search_or_type_web_address)
                            : null);
        } else if (SearchBoxProperties.VOICE_SEARCH_CLICK_CALLBACK == propertyKey) {
            voiceSearchButton.setOnClickListener(
                    model.get(SearchBoxProperties.VOICE_SEARCH_CLICK_CALLBACK));
        } else if (SearchBoxProperties.SEARCH_BOX_HINT_COLOR == propertyKey) {
            searchBoxTextView.setHintTextColor(
                    model.get(SearchBoxProperties.SEARCH_BOX_HINT_COLOR));
        } else if (SearchBoxProperties.SEARCH_BOX_HEIGHT == propertyKey) {
            ViewGroup.LayoutParams lp = searchBoxContainer.getLayoutParams();
            lp.height = model.get(SearchBoxProperties.SEARCH_BOX_HEIGHT);
            searchBoxContainer.setLayoutParams(lp);
        } else if (SearchBoxProperties.SEARCH_BOX_TOP_MARGIN == propertyKey) {
            MarginLayoutParams marginLayoutParams =
                    (MarginLayoutParams) searchBoxContainer.getLayoutParams();
            marginLayoutParams.topMargin = model.get(SearchBoxProperties.SEARCH_BOX_TOP_MARGIN);
        } else if (SearchBoxProperties.SEARCH_BOX_END_PADDING == propertyKey) {
            searchBoxContainer.setPadding(searchBoxContainer.getPaddingLeft(),
                    searchBoxContainer.getPaddingTop(),
                    model.get(SearchBoxProperties.SEARCH_BOX_END_PADDING),
                    searchBoxContainer.getPaddingBottom());
        } else if (SearchBoxProperties.SEARCH_TEXT_TRANSLATION_X == propertyKey) {
            searchBoxTextView.setTranslationX(
                    model.get(SearchBoxProperties.SEARCH_TEXT_TRANSLATION_X));
        } else if (SearchBoxProperties.BUTTONS_HEIGHT == propertyKey) {
            int height = model.get(SearchBoxProperties.BUTTONS_HEIGHT);
            ViewGroup.LayoutParams layoutParams = voiceSearchButton.getLayoutParams();
            if (layoutParams != null) {
                layoutParams.height = height;
                voiceSearchButton.setLayoutParams(layoutParams);
            }

            layoutParams = lensButton.getLayoutParams();
            if (layoutParams != null) {
                layoutParams.height = height;
                lensButton.setLayoutParams(layoutParams);
            }

        } else if (SearchBoxProperties.BUTTONS_WIDTH == propertyKey) {
            int width = model.get(SearchBoxProperties.BUTTONS_WIDTH);
            ViewGroup.LayoutParams layoutParams = voiceSearchButton.getLayoutParams();
            if (layoutParams != null) {
                layoutParams.width = width;
                voiceSearchButton.setLayoutParams(layoutParams);
            }

            layoutParams = lensButton.getLayoutParams();
            if (layoutParams != null) {
                layoutParams.width = width;
                lensButton.setLayoutParams(layoutParams);
            }

        } else if (SearchBoxProperties.LENS_BUTTON_LEFT_MARGIN == propertyKey) {
            MarginLayoutParams marginLayoutParams =
                    (MarginLayoutParams) lensButton.getLayoutParams();
            if (marginLayoutParams != null) {
                marginLayoutParams.leftMargin =
                        model.get(SearchBoxProperties.LENS_BUTTON_LEFT_MARGIN);
            }
        } else {
            assert false : "Unhandled property detected in SearchBoxViewBinder!";
        }
    }
}
