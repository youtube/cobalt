// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.ClipData;
import android.content.Context;
import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.InsetDrawable;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;
import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.base.Callback;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.chrome.browser.tasks.tab_management.TabUiMetricsHelper.TabSelectionEditorActionMetricGroups;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.browser_ui.share.ShareImageFileUtils;
import org.chromium.components.browser_ui.share.ShareParams;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.content_public.common.ContentUrlConstants;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashSet;
import java.util.List;

/**
 * Share action for the {@link TabSelectionEditorMenu}.
 */
public class TabSelectionEditorShareAction extends TabSelectionEditorAction {
    private static final List<String> UNSUPPORTED_SCHEMES =
            new ArrayList<>(Arrays.asList(UrlConstants.CHROME_SCHEME,
                    UrlConstants.CHROME_NATIVE_SCHEME, ContentUrlConstants.ABOUT_SCHEME));
    private static Callback<Intent> sIntentCallbackForTesting;
    private Context mContext;
    private boolean mSkipUrlCheckForTesting;

    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    @IntDef({TabSelectionEditorShareActionState.UNKNOWN, TabSelectionEditorShareActionState.SUCCESS,
            TabSelectionEditorShareActionState.ALL_TABS_FILTERED,
            TabSelectionEditorShareActionState.NUM_ENTRIES})
    public @interface TabSelectionEditorShareActionState {
        int UNKNOWN = 0;
        int SUCCESS = 1;
        int ALL_TABS_FILTERED = 2;

        // Be sure to also update enums.xml when updating these values.
        int NUM_ENTRIES = 3;
    }

    /**
     * Create an action for sharing tabs.
     * @param context for loading resources.
     * @param showMode whether to show an action view.
     * @param buttonType the type of the action view.
     * @param iconPosition the position of the icon in the action view.
     */
    public static TabSelectionEditorAction createAction(Context context, @ShowMode int showMode,
            @ButtonType int buttonType, @IconPosition int iconPosition) {
        Drawable drawable =
                AppCompatResources.getDrawable(context, R.drawable.tab_selection_editor_share_icon);
        return new TabSelectionEditorShareAction(
                context, showMode, buttonType, iconPosition, drawable);
    }

    private TabSelectionEditorShareAction(Context context, @ShowMode int showMode,
            @ButtonType int buttonType, @IconPosition int iconPosition, Drawable drawable) {
        super(R.id.tab_selection_editor_share_menu_item, showMode, buttonType, iconPosition,
                R.plurals.tab_selection_editor_share_tabs_action_button,
                R.plurals.accessibility_tab_selection_editor_share_tabs_action_button, drawable);
        mContext = context;
    }

    @Override
    public void onSelectionStateChange(List<Integer> tabIds) {
        boolean enableShare = false;
        List<Tab> selectedTabs = getTabsAndRelatedTabsFromSelection();

        for (Tab tab : selectedTabs) {
            if (!shouldFilterUrl(tab.getUrl())) {
                enableShare = true;
                break;
            }
        }

        int size = editorSupportsActionOnRelatedTabs() ? selectedTabs.size() : tabIds.size();
        setEnabledAndItemCount(enableShare && !tabIds.isEmpty(), size);
    }

    @Override
    public boolean performAction(List<Tab> tabs) {
        assert !tabs.isEmpty() : "Share action should not be enabled for no tabs.";

        TabList tabList = getTabModelSelector().getCurrentModel();
        List<Integer> sortedTabIndexList = filterTabs(tabs, tabList);

        if (sortedTabIndexList.size() == 0) {
            TabUiMetricsHelper.recordShareStateHistogram(
                    TabSelectionEditorShareActionState.ALL_TABS_FILTERED);
            return false;
        }

        boolean isOnlyOneTab = (sortedTabIndexList.size() == 1);
        String tabText =
                isOnlyOneTab ? "" : getTabListStringForSharing(sortedTabIndexList, tabList);
        String tabTitle =
                isOnlyOneTab ? tabList.getTabAt(sortedTabIndexList.get(0)).getTitle() : "";
        String tabUrl =
                isOnlyOneTab ? tabList.getTabAt(sortedTabIndexList.get(0)).getUrl().getSpec() : "";
        @TabSelectionEditorActionMetricGroups
        int actionId = isOnlyOneTab ? TabSelectionEditorActionMetricGroups.SHARE_TAB
                                    : TabSelectionEditorActionMetricGroups.SHARE_TABS;

        ShareParams shareParams =
                new ShareParams
                        .Builder(tabList.getTabAt(sortedTabIndexList.get(0)).getWindowAndroid(),
                                tabTitle, tabUrl)
                        .setText(tabText)
                        .build();

        final Intent shareIntent = new Intent(Intent.ACTION_SEND);
        shareIntent.putExtra(Intent.EXTRA_TEXT, shareParams.getTextAndUrl());
        shareIntent.setType("text/plain");
        shareIntent.putExtra(Intent.EXTRA_TITLE,
                mContext.getResources().getQuantityString(
                        R.plurals.tab_selection_editor_share_sheet_preview_message,
                        sortedTabIndexList.size(), sortedTabIndexList.size()));
        shareIntent.setFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);

        Drawable drawable = new InsetDrawable(
                AppCompatResources.getDrawable(mContext, R.drawable.chrome_sync_logo),
                (int) mContext.getResources().getDimension(
                        R.dimen.tab_selection_editor_share_sheet_preview_thumbnail_padding));
        createShareableImageAndSendIntent(shareIntent, drawable, actionId);
        return true;
    }

    @Override
    public boolean shouldHideEditorAfterAction() {
        return true;
    }

    private void createShareableImageAndSendIntent(Intent shareIntent, Drawable drawable,
            @TabSelectionEditorActionMetricGroups int actionId) {
        PostTask.postTask(TaskTraits.USER_BLOCKING_MAY_BLOCK, () -> {
            // Allotted thumbnail size is approx. 72 dp, with the icon left at default size.
            // The padding is adjusted accordingly, taking into account the scaling factor.
            Bitmap bitmap = Bitmap.createBitmap(drawable.getIntrinsicWidth(),
                    drawable.getIntrinsicHeight(), Bitmap.Config.ARGB_8888);
            Canvas canvas = new Canvas(bitmap);
            drawable.setBounds(0, 0, canvas.getWidth(), canvas.getHeight());
            drawable.draw(canvas);

            ShareImageFileUtils.generateTemporaryUriFromBitmap(
                    mContext.getResources().getString(
                            R.string.tab_selection_editor_share_sheet_preview_thumbnail),
                    bitmap, uri -> {
                        bitmap.recycle();
                        PostTask.postTask(TaskTraits.UI_DEFAULT, () -> {
                            shareIntent.setClipData(ClipData.newRawUri("", uri));
                            mContext.startActivity(Intent.createChooser(shareIntent, null));
                            TabUiMetricsHelper.recordSelectionEditorActionMetrics(actionId);
                            TabUiMetricsHelper.recordShareStateHistogram(
                                    TabSelectionEditorShareActionState.SUCCESS);
                        });

                        if (sIntentCallbackForTesting != null) {
                            sIntentCallbackForTesting.onResult(shareIntent);
                        }
                    });
        });
    }

    // TODO(crbug.com/1373579): Current filtering does not remove duplicates or show a "Toast" if
    // no shareable URLs are present after filtering.
    private List<Integer> filterTabs(List<Tab> tabs, TabList tabList) {
        assert tabs.size() > 0;
        List<Integer> sortedTabIndexList = new ArrayList<Integer>();

        HashSet<Tab> selectedTabs = new HashSet<Tab>(tabs);
        for (int i = 0; i < tabList.getCount(); i++) {
            Tab tab = tabList.getTabAt(i);
            if (!selectedTabs.contains(tab)) continue;

            if (!shouldFilterUrl(tab.getUrl())) {
                sortedTabIndexList.add(i);
            }
        }
        return sortedTabIndexList;
    }

    private String getTabListStringForSharing(List<Integer> sortedTabIndexList, TabList list) {
        StringBuilder sb = new StringBuilder();

        // TODO(crbug.com/1373579): Check if this string builder assembles the shareable URLs in
        // accordance with internationalization and translation standards
        for (int i = 0; i < sortedTabIndexList.size(); i++) {
            sb.append(i + 1)
                    .append(". ")
                    .append(list.getTabAt(sortedTabIndexList.get(i)).getUrl().getSpec())
                    .append("\n");
        }
        return sb.toString();
    }

    private boolean shouldFilterUrl(GURL url) {
        if (mSkipUrlCheckForTesting) return false;

        return url == null || !url.isValid() || url.isEmpty()
                || UNSUPPORTED_SCHEMES.contains(url.getScheme());
    }

    @VisibleForTesting
    void setSkipUrlCheckForTesting(boolean skip) {
        mSkipUrlCheckForTesting = skip;
    }

    @VisibleForTesting
    static void setIntentCallbackForTesting(Callback<Intent> callback) {
        sIntentCallbackForTesting = callback;
    }
}
