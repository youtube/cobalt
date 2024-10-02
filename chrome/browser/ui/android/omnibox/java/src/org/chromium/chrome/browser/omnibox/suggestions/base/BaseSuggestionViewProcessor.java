// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.base;

import android.content.Context;
import android.graphics.Typeface;
import android.text.Spannable;
import android.text.style.StyleSpan;

import androidx.annotation.CallSuper;
import androidx.annotation.DrawableRes;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.chrome.browser.omnibox.MatchClassificationStyle;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;
import org.chromium.chrome.browser.omnibox.suggestions.ActionChipsDelegate;
import org.chromium.chrome.browser.omnibox.suggestions.FaviconFetcher;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionHost;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionProcessor;
import org.chromium.chrome.browser.omnibox.suggestions.base.BaseSuggestionViewProperties.Action;
import org.chromium.components.omnibox.AutocompleteMatch;
import org.chromium.components.omnibox.AutocompleteMatch.MatchClassification;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

import java.util.Arrays;
import java.util.List;

/**
 * A class that handles base properties and model for most suggestions.
 */
public abstract class BaseSuggestionViewProcessor implements SuggestionProcessor {
    private final @NonNull Context mContext;
    private final @NonNull SuggestionHost mSuggestionHost;
    private final @Nullable ActionChipsProcessor mActionChipsProcessor;
    private final @Nullable FaviconFetcher mFaviconFetcher;
    private final int mDesiredFaviconWidthPx;
    private final int mDecorationImageSizePx;
    private final int mSuggestionSizePx;

    /**
     * @param context Current context.
     * @param host A handle to the object using the suggestions.
     * @param faviconFetcher A mechanism to use to retrieve favicons.
     */
    public BaseSuggestionViewProcessor(@NonNull Context context, @NonNull SuggestionHost host,
            @Nullable ActionChipsDelegate actionChipsDelegate,
            @Nullable FaviconFetcher faviconFetcher) {
        mContext = context;
        mSuggestionHost = host;
        mDesiredFaviconWidthPx = mContext.getResources().getDimensionPixelSize(
                R.dimen.omnibox_suggestion_favicon_size);
        mDecorationImageSizePx = context.getResources().getDimensionPixelSize(
                R.dimen.omnibox_suggestion_decoration_image_size);
        mSuggestionSizePx = mContext.getResources().getDimensionPixelSize(
                R.dimen.omnibox_suggestion_content_height);
        mFaviconFetcher = faviconFetcher;

        if (actionChipsDelegate != null) {
            mActionChipsProcessor = new ActionChipsProcessor(context, host, actionChipsDelegate);
        } else {
            mActionChipsProcessor = null;
        }
    }

    /**
     * @return The desired size of Omnibox suggestion favicon.
     */
    protected int getDesiredFaviconSize() {
        return mDesiredFaviconWidthPx;
    }

    /**
     * @return The size of suggestion decoration images in pixels.
     */
    protected int getDecorationImageSize() {
        return mDecorationImageSizePx;
    }

    @Override
    public int getMinimumViewHeight() {
        return mSuggestionSizePx;
    }

    /**
     * Specify SuggestionDrawableState for suggestion decoration.
     *
     * @param decoration SuggestionDrawableState object defining decoration for the suggestion.
     */
    protected void setSuggestionDrawableState(
            PropertyModel model, SuggestionDrawableState decoration) {
        model.set(BaseSuggestionViewProperties.ICON, decoration);
    }

    /**
     * Specify SuggestionDrawableState for action button.
     *
     * @param model Property model to update.
     * @param actions List of actions for the suggestion.
     */
    protected void setActionButtons(PropertyModel model, List<Action> actions) {
        model.set(BaseSuggestionViewProperties.ACTION_BUTTONS, actions);
    }

    /**
     * Setup action icon base on the suggestion, either show query build arrow or switch to tab.
     *
     * @param model Property model to update.
     * @param suggestion Suggestion associated with the action button.
     * @param position The position of the button in the list.
     */
    protected void setTabSwitchOrRefineAction(
            PropertyModel model, AutocompleteMatch suggestion, int position) {
        @DrawableRes
        int icon;
        String iconString;
        Runnable action;
        if (suggestion.hasTabMatch()) {
            icon = R.drawable.switch_to_tab;
            iconString = OmniboxResourceProvider.getString(
                    mContext, R.string.accessibility_omnibox_switch_to_tab);
            action = () -> mSuggestionHost.onSwitchToTab(suggestion, position);
        } else {
            iconString = OmniboxResourceProvider.getString(mContext,
                    R.string.accessibility_omnibox_btn_refine, suggestion.getFillIntoEdit());
            icon = R.drawable.btn_suggestion_refine;
            action = () -> mSuggestionHost.onRefineSuggestion(suggestion);
        }
        setActionButtons(model,
                Arrays.asList(
                        new Action(SuggestionDrawableState.Builder.forDrawableRes(mContext, icon)
                                           .setLarge(true)
                                           .setAllowTint(true)
                                           .build(),
                                iconString, action)));
    }

    /**
     * Process the click event.
     *
     * @param suggestion Selected suggestion.
     * @param position Position of the suggestion on the list.
     */
    protected void onSuggestionClicked(@NonNull AutocompleteMatch suggestion, int position) {
        mSuggestionHost.onSuggestionClicked(suggestion, position, suggestion.getUrl());
    }

    /**
     * Process the long-click event.
     *
     * @param suggestion Selected suggestion.
     * @param position Position of the suggestion on the list.
     */
    protected void onSuggestionLongClicked(@NonNull AutocompleteMatch suggestion, int position) {
        mSuggestionHost.onDeleteMatch(suggestion, suggestion.getDisplayText(), position);
    }

    @Override
    public void populateModel(AutocompleteMatch suggestion, PropertyModel model, int position) {
        model.set(BaseSuggestionViewProperties.ON_CLICK,
                () -> onSuggestionClicked(suggestion, position));
        model.set(BaseSuggestionViewProperties.ON_LONG_CLICK,
                () -> onSuggestionLongClicked(suggestion, position));
        model.set(BaseSuggestionViewProperties.ON_FOCUS_VIA_SELECTION,
                () -> mSuggestionHost.setOmniboxEditingText(suggestion.getFillIntoEdit()));
        setActionButtons(model, null);

        if (mActionChipsProcessor != null) {
            mActionChipsProcessor.populateModel(suggestion, model, position);
        }
    }

    @Override
    @CallSuper
    public void onUrlFocusChange(boolean hasFocus) {
        if (mActionChipsProcessor != null) {
            mActionChipsProcessor.onUrlFocusChange(hasFocus);
        }
    }

    @Override
    @CallSuper
    public void onSuggestionsReceived() {
        if (mActionChipsProcessor != null) {
            mActionChipsProcessor.onSuggestionsReceived();
        }
    }

    /**
     * Apply In-Place highlight to matching sections of Suggestion text.
     *
     * @param text Suggestion text to apply highlight to.
     * @param classifications Classifications describing how to format text.
     * @return true, if at least one highlighted match section was found.
     */
    protected static boolean applyHighlightToMatchRegions(
            Spannable text, List<MatchClassification> classifications) {
        if (text == null || classifications == null) return false;

        boolean hasAtLeastOneMatch = false;
        for (int i = 0; i < classifications.size(); i++) {
            MatchClassification classification = classifications.get(i);
            if ((classification.style & MatchClassificationStyle.MATCH)
                    == MatchClassificationStyle.MATCH) {
                int matchStartIndex = classification.offset;
                int matchEndIndex;
                if (i == classifications.size() - 1) {
                    matchEndIndex = text.length();
                } else {
                    matchEndIndex = classifications.get(i + 1).offset;
                }
                matchStartIndex = Math.min(matchStartIndex, text.length());
                matchEndIndex = Math.min(matchEndIndex, text.length());

                hasAtLeastOneMatch = true;
                // Bold the part of the URL that matches the user query.
                text.setSpan(new StyleSpan(Typeface.BOLD), matchStartIndex, matchEndIndex,
                        Spannable.SPAN_EXCLUSIVE_EXCLUSIVE);
            }
        }
        return hasAtLeastOneMatch;
    }

    /**
     * Fetch suggestion favicon, if one is available.
     * Updates icon decoration in supplied |model| if |url| is not null and points to an already
     * visited website.
     *
     * @param model Model representing current suggestion.
     * @param url Target URL the suggestion points to.
     */
    protected void fetchSuggestionFavicon(PropertyModel model, GURL url) {
        assert mFaviconFetcher != null : "You must supply the FaviconFetcher in order to use it";
        mFaviconFetcher.fetchFaviconWithBackoff(url, false, (icon, type) -> {
            if (icon != null) {
                setSuggestionDrawableState(
                        model, SuggestionDrawableState.Builder.forBitmap(mContext, icon).build());
            }
        });
    }

    /**
     * @return Current context.
     */
    protected Context getContext() {
        return mContext;
    }
}
