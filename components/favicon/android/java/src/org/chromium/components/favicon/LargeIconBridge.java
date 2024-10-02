// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.favicon;

import android.graphics.Bitmap;
import android.util.LruCache;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.components.browser_ui.util.ConversionUtils;
import org.chromium.content_public.browser.BrowserContextHandle;
import org.chromium.url.GURL;

/**
 * A Java API for using the C++ LargeIconService.
 *
 * An instance of this class must be created, used, and destroyed on the same thread.
 */
@JNINamespace("favicon")
public class LargeIconBridge {
    private static final int CACHE_ENTRY_MIN_SIZE_BYTES = ConversionUtils.BYTES_PER_KILOBYTE;
    private final BrowserContextHandle mBrowserContextHandle;
    private long mNativeLargeIconBridge;
    private LruCache<GURL, CachedFavicon> mFaviconCache;

    private static class CachedFavicon {
        public Bitmap icon;
        public int fallbackColor;
        public boolean isFallbackColorDefault;
        public @IconType int iconType;

        CachedFavicon(Bitmap newIcon, int newFallbackColor, boolean newIsFallbackColorDefault,
                @IconType int newIconType) {
            icon = newIcon;
            fallbackColor = newFallbackColor;
            isFallbackColorDefault = newIsFallbackColorDefault;
            iconType = newIconType;
        }
    }

    /**
     * Callback for use with GetLargeIconForUrl().
     */
    public interface LargeIconCallback {
        /**
         * Called when the icon or fallback color is available.
         *
         * @param icon The icon, or null if none is available.
         * @param fallbackColor The fallback color to use if icon is null.
         * @param isFallbackColorDefault Whether the fallback color is the default color.
         * @param iconType The type of the icon contributing to this event as defined in {@link
         * IconType}.
         */
        @CalledByNative("LargeIconCallback")
        void onLargeIconAvailable(@Nullable Bitmap icon, int fallbackColor,
                boolean isFallbackColorDefault, @IconType int iconType);
    }

    /**
     * Initializes the C++ side of this class.
     * @param browserContext Browser context to use when fetching icons.
     */
    public LargeIconBridge(BrowserContextHandle browserContext) {
        mNativeLargeIconBridge = LargeIconBridgeJni.get().init();
        mBrowserContextHandle = browserContext;
    }

    /**
     * Constructor that leaves the bridge independent from the native side.
     * Note: {@link #getLargeIconForStringUrl(String, int, LargeIconCallback)} will crash with the
     * default implementation, it should then be overridden.
     */
    @VisibleForTesting
    public LargeIconBridge() {
        mNativeLargeIconBridge = 0;
        mBrowserContextHandle = null;
    }

    /**
     * Create an internal cache.
     * @param cacheSizeBytes The maximum size of the cache in bytes. Must be greater than 0. Note
     *                       that this will be an approximate as there is no easy way to measure
     *                       the precise size in Java.
     */
    public void createCache(int cacheSizeBytes) {
        assert cacheSizeBytes > 0;

        mFaviconCache = new LruCache<GURL, CachedFavicon>(cacheSizeBytes) {
            @Override
            protected int sizeOf(GURL key, CachedFavicon favicon) {
                int iconBitmapSize = favicon.icon == null ? 0 : favicon.icon.getByteCount();
                return Math.max(CACHE_ENTRY_MIN_SIZE_BYTES, iconBitmapSize);
            }
        };
    }

    /**
     * Deletes the C++ side of this class. This must be called when this object is no longer needed.
     */
    public void destroy() {
        if (mNativeLargeIconBridge != 0) {
            LargeIconBridgeJni.get().destroy(mNativeLargeIconBridge);
            mNativeLargeIconBridge = 0;
        }
    }

    /**
     * See {@link #getLargeIconForUrl(GURL, int, LargeIconCallback)}
     *
     * @deprecated please use getLargeIconForUrl instead.
     */
    @Deprecated
    public boolean getLargeIconForStringUrl(
            final String pageUrl, int desiredSizePx, final LargeIconCallback callback) {
        return getLargeIconForUrl(new GURL(pageUrl), desiredSizePx, callback);
    }

    /**
     * Given a URL, returns a large icon for that URL if one is available (e.g. a favicon or
     * touch icon). If none is available, a fallback color is returned, based on the dominant color
     * of any small icons for the URL, or a default gray if no small icons are available. The icon
     * and fallback color are returned synchronously(when it's from cache) or asynchronously to the
     * given callback.
     *
     * @param pageUrl The URL of the page whose icon will be fetched.
     * @param desiredSizePx The desired size of the icon in pixels.
     * @param callback The method to call asynchronously when the result is available. This callback
     *                 will not be called if this method returns false.
     * @return True if a callback should be expected.
     */
    public boolean getLargeIconForUrl(
            final GURL pageUrl, int desiredSizePx, final LargeIconCallback callback) {
        assert mNativeLargeIconBridge != 0;
        assert callback != null;

        if (mFaviconCache == null) {
            return LargeIconBridgeJni.get().getLargeIconForURL(mNativeLargeIconBridge,
                    mBrowserContextHandle, pageUrl, desiredSizePx, callback);
        } else {
            CachedFavicon cached = mFaviconCache.get(pageUrl);
            if (cached != null) {
                callback.onLargeIconAvailable(cached.icon, cached.fallbackColor,
                        cached.isFallbackColorDefault, cached.iconType);
                return true;
            }

            LargeIconCallback callbackWrapper = new LargeIconCallback() {
                @Override
                public void onLargeIconAvailable(Bitmap icon, int fallbackColor,
                        boolean isFallbackColorDefault, @IconType int iconType) {
                    mFaviconCache.put(pageUrl,
                            new CachedFavicon(
                                    icon, fallbackColor, isFallbackColorDefault, iconType));
                    callback.onLargeIconAvailable(
                            icon, fallbackColor, isFallbackColorDefault, iconType);
                }
            };
            return LargeIconBridgeJni.get().getLargeIconForURL(mNativeLargeIconBridge,
                    mBrowserContextHandle, pageUrl, desiredSizePx, callbackWrapper);
        }
    }

    /**
     * Removes the favicon from the local cache for the given URL.
     */
    public void clearFavicon(GURL url) {
        mFaviconCache.remove(url);
    }

    @NativeMethods
    public interface Natives {
        long init();
        void destroy(long nativeLargeIconBridge);
        boolean getLargeIconForURL(long nativeLargeIconBridge,
                BrowserContextHandle browserContextHandle, GURL pageUrl, int desiredSizePx,
                LargeIconCallback callback);
    }
}
