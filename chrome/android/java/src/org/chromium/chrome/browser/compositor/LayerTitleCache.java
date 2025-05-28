// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.text.TextUtils;
import android.util.SparseArray;
import android.util.SparseIntArray;
import android.view.View;

import androidx.annotation.ColorInt;
import androidx.core.content.res.ResourcesCompat;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.compositor.layouts.content.TitleBitmapFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabFavicon;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.tab_management.TabUiThemeProvider;
import org.chromium.chrome.browser.tasks.tab_management.TabUiThemeUtil;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper.DefaultFaviconHelper;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper.FaviconImageCallback;
import org.chromium.ui.base.LocalizationUtils;
import org.chromium.ui.resources.ResourceManager;
import org.chromium.ui.resources.dynamics.BitmapDynamicResource;
import org.chromium.ui.resources.dynamics.DynamicResourceLoader;
import org.chromium.ui.resources.dynamics.ViewResourceAdapter;

import java.util.HashSet;

/**
 * A version of the {@link LayerTitleCache} that builds native cc::Layer objects that represent the
 * cached title textures.
 */
@JNINamespace("android")
public class LayerTitleCache {
    private final Context mContext;
    private TabModelSelector mTabModelSelector;

    private final SparseArray<FaviconTitle> mTabTitles = new SparseArray<>();
    private final SparseArray<Title> mGroupTitles = new SparseArray<>();
    private final SparseIntArray mSharedAvatarResIds = new SparseIntArray();
    private final HashSet<Integer> mTabBubbles = new HashSet<>();
    private final int mFaviconSize;
    private final int mSharedGroupAvatarPaddingPx;
    private final int mBubbleOuterCircleSize;
    private final int mBubbleInnerCircleSize;
    private final int mBubbleOffset;
    private final @ColorInt int mBubbleFillColor;
    private final @ColorInt int mBubbleBorderColor;

    private long mNativeLayerTitleCache;
    private final ResourceManager mResourceManager;

    private FaviconHelper mFaviconHelper;
    private DefaultFaviconHelper mDefaultFaviconHelper;

    /** Responsible for building titles on light themes or standard tabs. */
    protected final TitleBitmapFactory mStandardTitleBitmapFactory;

    /** Responsible for building incognito or dark theme titles. */
    protected final TitleBitmapFactory mDarkTitleBitmapFactory;

    /**
     * @param context The Android {@link Context}.
     * @param resourceManager The manager for static resources to be used by native layers.
     * @param tabStripHeightPx The height of the tab strip in pixels.
     */
    public LayerTitleCache(Context context, ResourceManager resourceManager, int tabStripHeightPx) {
        mContext = context;
        mResourceManager = resourceManager;
        Resources res = context.getResources();
        final int fadeWidthPx = res.getDimensionPixelOffset(R.dimen.border_texture_title_fade);
        final int faviconStartPaddingPx =
                res.getDimensionPixelSize(R.dimen.tab_title_favicon_start_padding);
        final int faviconEndPaddingPx =
                res.getDimensionPixelSize(R.dimen.tab_title_favicon_end_padding);
        mSharedGroupAvatarPaddingPx =
                res.getDimensionPixelSize(R.dimen.tablet_shared_group_avatar_padding);
        mFaviconSize = res.getDimensionPixelSize(R.dimen.compositor_tab_title_favicon_size);
        mStandardTitleBitmapFactory =
                new TitleBitmapFactory(context, /* incognito= */ false, tabStripHeightPx);
        mDarkTitleBitmapFactory =
                new TitleBitmapFactory(context, /* incognito= */ true, tabStripHeightPx);
        mDefaultFaviconHelper = new DefaultFaviconHelper();
        mBubbleOuterCircleSize =
                res.getDimensionPixelSize(R.dimen.compositor_tab_title_favicon_bubble_outer_size);
        mBubbleInnerCircleSize =
                res.getDimensionPixelSize(R.dimen.compositor_tab_title_favicon_bubble_inner_size);
        mBubbleOffset =
                res.getDimensionPixelSize(R.dimen.compositor_tab_title_favicon_bubble_offset);
        mBubbleBorderColor =
                TabUiThemeUtil.getTabStripBackgroundColorForActivityState(
                        context, /* isIncognito= */ false, /* isActivityFocused= */ true);
        mBubbleFillColor = TabUiThemeProvider.getTabBubbleFillColor(context);
        mNativeLayerTitleCache =
                LayerTitleCacheJni.get()
                        .init(
                                LayerTitleCache.this,
                                fadeWidthPx,
                                faviconStartPaddingPx,
                                faviconEndPaddingPx,
                                R.drawable.spinner,
                                R.drawable.spinner_white,
                                mBubbleInnerCircleSize,
                                mBubbleOuterCircleSize,
                                mBubbleOffset,
                                mBubbleFillColor,
                                mBubbleBorderColor,
                                mResourceManager);
    }

    /** Destroys the native reference. */
    public void shutDown() {
        if (mFaviconHelper != null) {
            mFaviconHelper.destroy();
            mFaviconHelper = null;
        }

        if (mNativeLayerTitleCache == 0) return;
        LayerTitleCacheJni.get().destroy(mNativeLayerTitleCache);
        mNativeLayerTitleCache = 0;
    }

    public void setTabModelSelector(TabModelSelector tabModelSelector) {
        mTabModelSelector = tabModelSelector;
    }

    @CalledByNative
    private long getNativePtr() {
        return mNativeLayerTitleCache;
    }

    @CalledByNative
    private void buildUpdatedTitle(int tabId) {
        if (mTabModelSelector == null) return;

        Tab tab = mTabModelSelector.getTabById(tabId);
        if (tab == null || tab.isDestroyed()) return;

        getUpdatedTitle(tab, "");
    }

    /**
     * @param tabId The ID of the tab that needs to show the notification bubble.
     */
    public void updateTabBubble(int tabId, boolean showBubble) {
        if (showBubble) {
            mTabBubbles.add(tabId);
        } else {
            mTabBubbles.remove(tabId);
        }
        LayerTitleCacheJni.get()
                .updateTabBubble(mNativeLayerTitleCache, LayerTitleCache.this, tabId, showBubble);
    }

    public String getUpdatedTitle(Tab tab, String defaultTitle) {
        // If content view core is null, tab does not have direct access to the favicon, and we
        // will initially show default favicon. But favicons are stored in the history database, so
        // we will fetch favicons asynchronously from database.
        boolean fetchFaviconFromHistory = tab.isNativePage() || tab.getWebContents() == null;

        String titleString = getTitleForTab(tab, defaultTitle);
        getUpdatedTitleInternal(tab, titleString, fetchFaviconFromHistory);
        if (fetchFaviconFromHistory) fetchFaviconForTab(tab);
        return titleString;
    }

    private String getUpdatedTitleInternal(
            Tab tab, String titleString, boolean fetchFaviconFromHistory) {
        final int tabId = tab.getId();
        boolean isDarkTheme = tab.isIncognito();
        Bitmap originalFavicon = getOriginalFavicon(tab);

        TitleBitmapFactory titleBitmapFactory =
                isDarkTheme ? mDarkTitleBitmapFactory : mStandardTitleBitmapFactory;

        FaviconTitle title = mTabTitles.get(tabId);
        if (title == null) {
            title = new FaviconTitle();
            mTabTitles.put(tabId, title);
            title.register();
        }

        title.set(
                titleBitmapFactory.getTabTitleBitmap(titleString),
                titleBitmapFactory.getFaviconBitmap(originalFavicon),
                fetchFaviconFromHistory);

        boolean showBubble = mTabBubbles.contains(tab.getId());
        if (mNativeLayerTitleCache != 0) {
            String tabTitle = tab.getTitle();
            boolean isRtl =
                    tabTitle != null
                            && LocalizationUtils.getFirstStrongCharacterDirection(tabTitle)
                                    == LocalizationUtils.RIGHT_TO_LEFT;
            LayerTitleCacheJni.get()
                    .updateLayer(
                            mNativeLayerTitleCache,
                            LayerTitleCache.this,
                            tabId,
                            title.getTitleResId(),
                            title.getFaviconResId(),
                            isDarkTheme,
                            isRtl,
                            showBubble);
        }
        return titleString;
    }

    @CalledByNative
    private void buildUpdatedGroupTitle(int groupRootId, boolean incognito) {
        // TODO(crbug.com/331642736): Investigate if this can be called with a different width than
        //  what is stored for the corresponding group title.
        TabGroupModelFilter filter =
                mTabModelSelector
                        .getTabGroupModelFilterProvider()
                        .getTabGroupModelFilter(incognito);
        if (!filter.tabGroupExistsForRootId(groupRootId)) return;

        String titleString = filter.getTabGroupTitle(groupRootId);
        getUpdatedGroupTitle(groupRootId, titleString, incognito);
    }

    public String getUpdatedGroupTitle(int groupRootId, String titleString, boolean incognito) {
        if (TextUtils.isEmpty(titleString)) return null;

        getUpdatedGroupTitleInternal(groupRootId, titleString, incognito);
        return titleString;
    }

    private String getUpdatedGroupTitleInternal(int rootId, String titleString, boolean incognito) {
        TitleBitmapFactory titleBitmapFactory =
                incognito ? mDarkTitleBitmapFactory : mStandardTitleBitmapFactory;

        Title title = mGroupTitles.get(rootId);
        if (title == null) {
            title = new Title();
            mGroupTitles.put(rootId, title);
            title.register();
        }

        TabGroupModelFilter filter =
                mTabModelSelector.getTabGroupModelFilterProvider().getCurrentTabGroupModelFilter();
        Bitmap titleBitmap =
                titleBitmapFactory.getGroupTitleBitmap(filter, mContext, rootId, titleString);
        title.set(titleBitmap);

        int avatarResId = mSharedAvatarResIds.get(rootId, ResourcesCompat.ID_NULL);
        ViewResourceAdapter avatarResource = null;
        if (avatarResId != ResourcesCompat.ID_NULL) {
            avatarResource = getResourceAdapterFromLoader(avatarResId);
            if (avatarResource != null) avatarResource.invalidate(null);
        }

        if (mNativeLayerTitleCache != 0) {
            boolean isRtl =
                    titleString != null
                            && LocalizationUtils.getFirstStrongCharacterDirection(titleString)
                                    == LocalizationUtils.RIGHT_TO_LEFT;
            LayerTitleCacheJni.get()
                    .updateGroupLayer(
                            mNativeLayerTitleCache,
                            LayerTitleCache.this,
                            rootId,
                            title.getTitleResId(),
                            avatarResource == null ? ResourcesCompat.ID_NULL : avatarResId,
                            avatarResource == null ? 0 : mSharedGroupAvatarPaddingPx,
                            incognito,
                            isRtl);
        }
        return titleString;
    }

    /**
     * @param incognito Whether or not the tab group is from the Incognito model.
     * @param titleString The title of the tab group.
     * @return The width in px of the title.
     */
    public int getGroupTitleWidth(boolean incognito, String titleString) {
        if (titleString == null) return 0;

        TitleBitmapFactory titleBitmapFactory =
                incognito ? mDarkTitleBitmapFactory : mStandardTitleBitmapFactory;
        return titleBitmapFactory.getGroupTitleWidth(titleString);
    }

    private void fetchFaviconForTab(final Tab tab) {
        fetchFaviconWithCallback(tab, (favicon, iconUrl) -> updateFaviconFromHistory(tab, favicon));
    }

    /**
     * Requests the favicon for the given tab.
     *
     * @param tab The {@link Tab} to request the favicon for.
     * @param callback A callback to run when the favicon is available.
     */
    public void fetchFaviconWithCallback(final Tab tab, FaviconImageCallback callback) {
        if (mFaviconHelper == null) mFaviconHelper = new FaviconHelper();

        if (tab.getTabGroupId() != null
                && !tab.isOffTheRecord()
                && ChromeFeatureList.sTabSwitcherForeignFaviconSupport.isEnabled()) {
            // This mirrors the async tab favicon request implementation for tab list.
            // See TabListFaviconProvider#getFaviconForTabAsync for more detailed notes.
            // TODO(crbug.com/394165786): Unify with the aforementioned TabListFaviconProvider code.
            mFaviconHelper.getForeignFaviconImageForURL(
                    tab.getProfile(), tab.getUrl(), mFaviconSize, callback);
        } else {
            mFaviconHelper.getLocalFaviconImageForURL(
                    tab.getProfile(), tab.getUrl(), mFaviconSize, callback);
        }
    }

    /**
     * Requests a default favicon for the given tab.
     *
     * @param tab The {@link Tab} to request the favicon for.
     * @return The tab's favicon based on its web contents. Otherwise, a default favicon.
     */
    public Bitmap getOriginalFavicon(Tab tab) {
        boolean isDarkTheme = tab.isIncognito();
        Bitmap originalFavicon = TabFavicon.getBitmap(tab);
        if (originalFavicon == null) {
            originalFavicon =
                    mDefaultFaviconHelper.getDefaultFaviconBitmap(
                            mContext, tab.getUrl(), !isDarkTheme);
        }

        return originalFavicon;
    }

    private ViewResourceAdapter getResourceAdapterFromLoader(int resId) {
        DynamicResourceLoader dynamicResourceLoader = mResourceManager.getDynamicResourceLoader();
        return (ViewResourceAdapter) dynamicResourceLoader.getResource(resId);
    }

    public void registerSharedGroupAvatar(int rootId, ViewResourceAdapter avatarResource) {
        DynamicResourceLoader dynamicResourceLoader = mResourceManager.getDynamicResourceLoader();
        int resId = View.generateViewId();
        dynamicResourceLoader.registerResource(resId, avatarResource);
        mSharedAvatarResIds.put(rootId, resId);
    }

    public void transferAvatarToNewRootId(int oldRootId, int newRootId) {
        int avatarResId = mSharedAvatarResIds.get(oldRootId, ResourcesCompat.ID_NULL);
        if (avatarResId == ResourcesCompat.ID_NULL) return;
        mSharedAvatarResIds.delete(oldRootId);
        mSharedAvatarResIds.put(newRootId, avatarResId);
    }

    private void unregisterSharedGroupAvatar(int resId) {
        DynamicResourceLoader dynamicResourceLoader = mResourceManager.getDynamicResourceLoader();
        dynamicResourceLoader.unregisterResource(resId);
    }

    /**
     * Comes up with a valid title to return for a tab.
     *
     * @param tab The {@link Tab} to build a title for.
     * @return The title to use.
     */
    private String getTitleForTab(Tab tab, String defaultTitle) {
        String title = tab.getTitle();
        if (TextUtils.isEmpty(title)) {
            title = tab.getUrl().getSpec();
            if (TextUtils.isEmpty(title)) {
                title = defaultTitle;
                if (TextUtils.isEmpty(title)) {
                    title = "";
                }
            }
        }
        return title;
    }

    private void updateFaviconFromHistory(Tab tab, Bitmap faviconBitmap) {
        if (!tab.isInitialized()) return;

        int tabId = tab.getId();
        FaviconTitle title = mTabTitles.get(tabId);
        if (title == null) return;

        boolean showBubble = mTabBubbles.contains(tab.getId());
        if (!title.updateFaviconFromHistory(faviconBitmap)) return;

        if (mNativeLayerTitleCache != 0) {
            LayerTitleCacheJni.get()
                    .updateIcon(
                            mNativeLayerTitleCache,
                            LayerTitleCache.this,
                            tabId,
                            title.getFaviconResId(),
                            showBubble);
        }
    }

    public void removeTabTitle(int tabId) {
        FaviconTitle title = mTabTitles.get(tabId);
        if (title == null) return;
        title.unregister();
        mTabTitles.remove(tabId);
        if (mNativeLayerTitleCache == 0) return;
        LayerTitleCacheJni.get()
                .updateLayer(
                        mNativeLayerTitleCache,
                        LayerTitleCache.this,
                        tabId,
                        ResourcesCompat.ID_NULL,
                        ResourcesCompat.ID_NULL,
                        false,
                        false,
                        false);
    }

    public void removeGroupTitle(int rootId) {
        Title title = mGroupTitles.get(rootId);
        if (title == null) return;
        title.unregister();
        mGroupTitles.remove(rootId);
        if (mNativeLayerTitleCache == 0) return;
        LayerTitleCacheJni.get()
                .updateGroupLayer(
                        mNativeLayerTitleCache,
                        LayerTitleCache.this,
                        rootId,
                        ResourcesCompat.ID_NULL,
                        ResourcesCompat.ID_NULL,
                        0,
                        false,
                        false);
    }

    public void removeSharedGroupAvatar(int rootId) {
        int resId = mSharedAvatarResIds.get(rootId, ResourcesCompat.ID_NULL);
        if (resId == ResourcesCompat.ID_NULL) return;
        unregisterSharedGroupAvatar(resId);
        mSharedAvatarResIds.delete(rootId);
    }

    private class Title {
        final BitmapDynamicResource mTitle = new BitmapDynamicResource(View.generateViewId());

        public Title() {}

        public void set(Bitmap titleBitmap) {
            mTitle.setBitmap(titleBitmap);
        }

        public void register() {
            DynamicResourceLoader loader = mResourceManager.getBitmapDynamicResourceLoader();
            loader.registerResource(mTitle.getResId(), mTitle);
        }

        public void unregister() {
            DynamicResourceLoader loader = mResourceManager.getBitmapDynamicResourceLoader();
            loader.unregisterResource(mTitle.getResId());
        }

        public int getTitleResId() {
            return mTitle.getResId();
        }
    }

    private class FaviconTitle extends Title {
        private final BitmapDynamicResource mFavicon =
                new BitmapDynamicResource(View.generateViewId());

        // We don't want to override updated favicon (e.g. from Tab#onFaviconAvailable) with one
        // fetched from history. You can set this to true / false to control that.
        private boolean mExpectUpdateFromHistory;

        public FaviconTitle() {}

        public void set(Bitmap titleBitmap, Bitmap faviconBitmap, boolean expectUpdateFromHistory) {
            set(titleBitmap);
            mFavicon.setBitmap(faviconBitmap);
            mExpectUpdateFromHistory = expectUpdateFromHistory;
        }

        public boolean updateFaviconFromHistory(Bitmap faviconBitmap) {
            if (!mExpectUpdateFromHistory) return false;
            mFavicon.setBitmap(faviconBitmap);
            mExpectUpdateFromHistory = false;
            return true;
        }

        @Override
        public void register() {
            super.register();
            DynamicResourceLoader loader = mResourceManager.getBitmapDynamicResourceLoader();
            loader.registerResource(mFavicon.getResId(), mFavicon);
        }

        @Override
        public void unregister() {
            super.unregister();
            DynamicResourceLoader loader = mResourceManager.getBitmapDynamicResourceLoader();
            loader.unregisterResource(mFavicon.getResId());
        }

        public int getFaviconResId() {
            return mFavicon.getResId();
        }
    }

    @NativeMethods
    interface Natives {
        long init(
                LayerTitleCache caller,
                int fadeWidth,
                int faviconStartPadding,
                int faviconEndPadding,
                int spinnerResId,
                int spinnerIncognitoResId,
                int tabBubbleInnerDimension,
                int tabBubbleOuterDimension,
                int bubbleOffset,
                @ColorInt int tabBubbleInnerColor,
                @ColorInt int tabBubbleOuterColor,
                ResourceManager resourceManager);

        void destroy(long nativeLayerTitleCache);

        void updateLayer(
                long nativeLayerTitleCache,
                LayerTitleCache caller,
                int tabId,
                int titleResId,
                int faviconResId,
                boolean isIncognito,
                boolean isRtl,
                boolean showBubble);

        void updateGroupLayer(
                long nativeLayerTitleCache,
                LayerTitleCache caller,
                int groupRootId,
                int titleResId,
                int avatarResId,
                int avatarPadding,
                boolean isIncognito,
                boolean isRtl);

        void updateIcon(
                long nativeLayerTitleCache,
                LayerTitleCache caller,
                int tabId,
                int faviconResId,
                boolean showBubble);

        void updateTabBubble(
                long nativeLayerTitleCache, LayerTitleCache caller, int tabId, boolean showBubble);
    }
}
