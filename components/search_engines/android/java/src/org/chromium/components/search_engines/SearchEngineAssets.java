// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.components.search_engines;

import static org.chromium.build.NullUtil.assertNonNull;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;

import androidx.annotation.DrawableRes;
import androidx.annotation.Px;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.build.BuildConfig;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.search_engines.SearchEngineType;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.content_public.browser.BrowserContextHandle;
import org.chromium.url.GURL;

/** Utility class for retrieving search engine assets (like favicons). */
@NullMarked
public class SearchEngineAssets implements Destroyable {
    private final Context mContext;
    private final LargeIconBridge mLargeIconBridge;

    public SearchEngineAssets(Context context, BrowserContextHandle browserContext) {
        mContext = context;
        mLargeIconBridge = new LargeIconBridge(browserContext);
    }

    @VisibleForTesting
    public SearchEngineAssets(Context context, LargeIconBridge bridge) {
        mContext = context;
        mLargeIconBridge = bridge;
    }

    @Override
    public void destroy() {
        mLargeIconBridge.destroy();
    }

    /**
     * Retrieve the favicon for the given TemplateUrl.
     *
     * @param templateUrlService The TemplateUrlService to check search engine types.
     * @param templateUrl The TemplateUrl to retrieve the favicon for.
     * @param desiredSizePx The desired size of the icon in pixels.
     * @param callback The callback to receive the Drawable, or null if not found.
     */
    public void retrieveFavicon(
            TemplateUrlService templateUrlService,
            TemplateUrl templateUrl,
            @Px int desiredSizePx,
            Callback<Drawable> callback) {
        retrieveFaviconFromBrandedResources(
                templateUrlService, templateUrl, desiredSizePx, callback);
    }

    private void retrieveFaviconFromBrandedResources(
            TemplateUrlService templateUrlService,
            TemplateUrl templateUrl,
            @Px int desiredSizePx,
            Callback<Drawable> callback) {
        // Branded resources are only available on Chrome branded builds.
        if (BuildConfig.IS_CHROME_BRANDED) {
            @Nullable Bitmap bm = templateUrl.getBuiltInSearchEngineIcon();
            if (bm != null) {
                callback.onResult(new BitmapDrawable(mContext.getResources(), bm));
                return;
            }
        }
        retrieveFaviconForStarterPack(templateUrlService, templateUrl, desiredSizePx, callback);
    }

    private void retrieveFaviconForStarterPack(
            TemplateUrlService templateUrlService,
            TemplateUrl templateUrl,
            @Px int desiredSizePx,
            Callback<Drawable> callback) {
        @StarterPackId int starterPackId = templateUrl.getStarterPackId();
        if (starterPackId != StarterPackId.NONE) {
            // TODO(crbug.com/522176230): Apply desiredSizePx to regenerate starter pack icons.
            @Nullable Drawable drawable = getStarterPackDrawable(starterPackId);
            if (drawable != null) {
                callback.onResult(drawable);
                return;
            }
        }
        retrieveFaviconFromDefaultResources(
                templateUrlService, templateUrl, desiredSizePx, callback);
    }

    private void retrieveFaviconFromDefaultResources(
            TemplateUrlService templateUrlService,
            TemplateUrl templateUrl,
            @Px int desiredSizePx,
            Callback<Drawable> callback) {
        if (templateUrlService.getSearchEngineTypeFromTemplateUrl(templateUrl.getKeyword())
                != SearchEngineType.SEARCH_ENGINE_GOOGLE) {
            // Fall back to next source.
            retrieveFaviconFromOriginUrl(templateUrl, desiredSizePx, callback);
            return;
        }
        callback.onResult(assertNonNull(mContext.getDrawable(R.drawable.search_engine_google)));
    }

    private void retrieveFaviconFromOriginUrl(
            TemplateUrl templateUrl, @Px int desiredSizePx, Callback<Drawable> callback) {
        var originUrl = new GURL(templateUrl.getURL()).getOrigin();
        boolean willCall =
                mLargeIconBridge.getLargeIconForUrl(
                        originUrl,
                        desiredSizePx,
                        (image, fallbackColor, isFallbackColorDefault, iconType) -> {
                            if (image == null) {
                                callback.onResult(getDefaultFavicon());
                            } else {
                                callback.onResult(
                                        new BitmapDrawable(mContext.getResources(), image));
                            }
                        });
        if (!willCall) {
            callback.onResult(getDefaultFavicon());
        }
    }

    private @Nullable Drawable getStarterPackDrawable(@StarterPackId int starterPackId) {
        @DrawableRes
        int id =
                switch (starterPackId) {
                    case StarterPackId.BOOKMARKS, StarterPackId.HISTORY, StarterPackId.TABS ->
                            R.drawable.search_engine_default;
                    case StarterPackId.GEMINI -> R.drawable.search_engine_gemini;
                    case StarterPackId.AI_MODE -> R.drawable.search_engine_aimode;
                    default -> Resources.ID_NULL;
                };
        if (id == Resources.ID_NULL) return null;
        return mContext.getDrawable(id);
    }

    public Drawable getDefaultFavicon() {
        return assertNonNull(mContext.getDrawable(R.drawable.search_engine_default));
    }
}
