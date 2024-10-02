// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.language.settings;

import static org.chromium.components.browser_ui.widget.listmenu.BasicListMenu.buildMenuListItem;
import static org.chromium.components.browser_ui.widget.listmenu.BasicListMenu.buildMenuListItemWithEndIcon;

import android.content.Context;
import android.util.AttributeSet;
import android.widget.TextView;

import androidx.preference.Preference;
import androidx.preference.PreferenceViewHolder;
import androidx.recyclerview.widget.DividerItemDecoration;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;
import androidx.recyclerview.widget.RecyclerView.ViewHolder;

import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.language.R;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.translate.TranslateBridge;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.browser_ui.widget.TintedDrawable;
import org.chromium.components.browser_ui.widget.listmenu.BasicListMenu;
import org.chromium.components.browser_ui.widget.listmenu.ListMenu;
import org.chromium.components.browser_ui.widget.listmenu.ListMenuItemProperties;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;

/**
 * A preference that displays the current accept language list.
 */
public class ContentLanguagesPreference extends Preference {
    private static class LanguageListAdapter
            extends LanguageListBaseAdapter implements LanguagesManager.AcceptLanguageObserver {
        private final Context mContext;

        LanguageListAdapter(Context context) {
            super(context);
            mContext = context;
        }

        @Override
        public void onBindViewHolder(ViewHolder holder, int position) {
            super.onBindViewHolder(holder, position);

            final LanguageItem info = getItemByPosition(position);

            showDragIndicatorInRow((LanguageRowViewHolder) holder);
            ModelList menuItems = new ModelList();

            // Show "Offer to translate" option if "Chrome Translate" is enabled and
            // the detailed languages settings page is not active.
            if (UserPrefs.get(Profile.getLastUsedRegularProfile())
                            .getBoolean(Pref.OFFER_TRANSLATE_ENABLED)
                    && !ChromeFeatureList.isEnabled(ChromeFeatureList.DETAILED_LANGUAGE_SETTINGS)) {
                // Set this row checked if the language is unblocked.
                int endIconResId = TranslateBridge.isBlockedLanguage(info.getCode())
                        ? 0
                        : R.drawable.ic_check_googblue_24dp;
                ListItem item = buildMenuListItemWithEndIcon(
                        R.string.languages_item_option_offer_to_translate, 0, endIconResId,
                        info.isTranslateSupported());
                item.model.set(ListMenuItemProperties.TINT_COLOR_ID,
                        R.color.default_icon_color_accent1_tint_list);

                // Add checked icon at the end.
                menuItems.add(item);
            }

            int languageCount = getItemCount();
            // Enable "Remove" option if there are multiple accept languages.
            menuItems.add(buildMenuListItem(R.string.remove, 0, 0, languageCount > 1));

            // Add some appropriate options for moving the language when the list is not
            // draggable. E.g. in the accessibility mode.
            if (!mDragStateDelegate.getDragEnabled()) {
                // Add "Move to top" and "Move up" menu when it's not the first one.
                if (position > 0) {
                    menuItems.add(buildMenuListItem(R.string.menu_item_move_to_top, 0, 0));
                    menuItems.add(buildMenuListItem(R.string.menu_item_move_up, 0, 0));
                }

                // Add "Move down" menu when it's not the last one.
                if (position < (languageCount - 1)) {
                    menuItems.add(buildMenuListItem(R.string.menu_item_move_down, 0, 0));
                }
            }
            ListMenu.Delegate delegate = (model) -> {
                int textId = model.get(ListMenuItemProperties.TITLE_ID);
                if (textId == R.string.languages_item_option_offer_to_translate) {
                    // Toggle current blocked state of this language.
                    boolean state = model.get(ListMenuItemProperties.END_ICON_ID) == 0;
                    TranslateBridge.setLanguageBlockedState(info.getCode(), !state);
                    LanguagesManager.recordAction(state
                                    ? LanguagesManager.LanguageSettingsActionType
                                              .ENABLE_TRANSLATE_FOR_SINGLE_LANGUAGE
                                    : LanguagesManager.LanguageSettingsActionType
                                              .DISABLE_TRANSLATE_FOR_SINGLE_LANGUAGE);
                } else if (textId == R.string.remove) {
                    LanguagesManager.getInstance().removeFromAcceptLanguages(info.getCode());
                    LanguagesManager.recordAction(
                            LanguagesManager.LanguageSettingsActionType.LANGUAGE_REMOVED);
                } else if (textId == R.string.menu_item_move_up) {
                    LanguagesManager.getInstance().moveLanguagePosition(info.getCode(), -1, true);
                } else if (textId == R.string.menu_item_move_down) {
                    LanguagesManager.getInstance().moveLanguagePosition(info.getCode(), 1, true);
                } else if (textId == R.string.menu_item_move_to_top) {
                    LanguagesManager.getInstance().moveLanguagePosition(
                            info.getCode(), -position, true);
                }
                // Re-generate list items.
                if (textId != R.string.remove) {
                    notifyDataSetChanged();
                }
            };
            ((LanguageRowViewHolder) holder).setMenuButtonDelegate(() -> {
                LanguagesManager.recordImpression(
                        LanguagesManager.LanguageSettingsPageType.LANGUAGE_OVERFLOW_MENU_OPENED);
                return new BasicListMenu(mContext, menuItems, delegate);
            });
        }

        @Override
        public void onDataUpdated() {
            if (mDragStateDelegate.getDragActive()) {
                enableDrag();
            } else {
                disableDrag();
            }
            setDisplayedLanguages(LanguagesManager.getInstance().getUserAcceptLanguageItems());
        }
    }

    private TextView mAddLanguageButton;
    private RecyclerView mRecyclerView;
    private LanguageListAdapter mAdapter;
    private SelectLanguageFragment.Launcher mLauncher;

    public ContentLanguagesPreference(Context context, AttributeSet attrs) {
        super(context, attrs);
        mAdapter = new LanguageListAdapter(context);
    }

    @Override
    public void onBindViewHolder(PreferenceViewHolder holder) {
        super.onBindViewHolder(holder);

        assert mLauncher != null;

        mAddLanguageButton = (TextView) holder.findViewById(R.id.add_language);
        final TintedDrawable tintedDrawable =
                TintedDrawable.constructTintedDrawable(getContext(), R.drawable.plus);
        tintedDrawable.setTint(SemanticColorUtils.getDefaultControlColorActive(getContext()));
        mAddLanguageButton.setCompoundDrawablesRelativeWithIntrinsicBounds(
                tintedDrawable, null, null, null);
        mAddLanguageButton.setOnClickListener(view -> { mLauncher.launchAddLanguage(); });

        mRecyclerView = (RecyclerView) holder.findViewById(R.id.language_list);
        LinearLayoutManager layoutMangager = new LinearLayoutManager(getContext());
        mRecyclerView.setLayoutManager(layoutMangager);

        // If the RecyclerView is re-bound, adding item decoration a second time adds extra padding
        // and dividers. Only add the item decoration once.
        if (mRecyclerView.getItemDecorationCount() == 0) {
            mRecyclerView.addItemDecoration(
                    new DividerItemDecoration(getContext(), layoutMangager.getOrientation()));
        }

        // We do not want the RecyclerView to be announced by screen readers every time
        // the view is bound.
        if (mRecyclerView.getAdapter() != mAdapter) {
            mRecyclerView.setAdapter(mAdapter);
            LanguagesManager.getInstance().setAcceptLanguageObserver(mAdapter);
            // Initialize accept language list.
            mAdapter.onDataUpdated();
        }
    }

    /**
     * Register a launcher for SelectLanguageFragment. Preference's host fragment should call
     * this in its onCreate().
     */
    void registerActivityLauncher(SelectLanguageFragment.Launcher launcher) {
        mLauncher = launcher;
    }

    /**
     * Notify LanguageListAdapter of pref changes to update list items.
     */
    void notifyPrefChanged() {
        mAdapter.onDataUpdated();
    }
}
