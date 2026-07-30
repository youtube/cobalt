// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.search_engines;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Shadows;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.search_engines.SearchEngineType;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.components.favicon.LargeIconBridge.LargeIconCallback;
import org.chromium.url.GURL;

/** Unit tests for {@link SearchEngineAssets}. */
@RunWith(BaseRobolectricTestRunner.class)
public class SearchEngineAssetsUnitTest {
    private static final int DESIRED_SIZE_PX = 16;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private LargeIconBridge mLargeIconBridge;
    @Mock private TemplateUrlService mTemplateUrlService;
    @Mock private Callback<Drawable> mCallback;
    @Mock private TemplateUrl mTemplateUrl;
    @Mock private Bitmap mBitmap;

    @Captor private ArgumentCaptor<LargeIconCallback> mLargeIconCallbackCaptor;
    @Captor private ArgumentCaptor<Drawable> mDrawableCaptor;

    private SearchEngineAssets mSearchEngineAssets;

    @Before
    public void setUp() {
        Context context = ContextUtils.getApplicationContext();
        mSearchEngineAssets = new SearchEngineAssets(context, mLargeIconBridge);
        doReturn("http://example.com").when(mTemplateUrl).getURL();
        doReturn("example.com").when(mTemplateUrl).getKeyword();
        doReturn(StarterPackId.NONE).when(mTemplateUrl).getStarterPackId();
    }

    @Test
    public void testRetrieveFavicon_GoogleDSE() {
        doReturn(SearchEngineType.SEARCH_ENGINE_GOOGLE)
                .when(mTemplateUrlService)
                .getSearchEngineTypeFromTemplateUrl("example.com");

        mSearchEngineAssets.retrieveFavicon(
                mTemplateUrlService, mTemplateUrl, DESIRED_SIZE_PX, mCallback);
        verify(mLargeIconBridge, never()).getLargeIconForUrl(any(), anyInt(), any());

        verify(mCallback).onResult(mDrawableCaptor.capture());
        assertEquals(
                R.drawable.search_engine_google,
                Shadows.shadowOf(mDrawableCaptor.getValue()).getCreatedFromResId());
    }

    @Test
    public void testRetrieveFavicon_StarterPack_Bookmarks() {
        doReturn(StarterPackId.BOOKMARKS).when(mTemplateUrl).getStarterPackId();

        mSearchEngineAssets.retrieveFavicon(
                mTemplateUrlService, mTemplateUrl, DESIRED_SIZE_PX, mCallback);
        verify(mLargeIconBridge, never()).getLargeIconForUrl(any(), anyInt(), any());

        verify(mCallback).onResult(mDrawableCaptor.capture());
        assertNotNull(mDrawableCaptor.getValue());
        assertEquals(
                R.drawable.search_engine_default,
                Shadows.shadowOf(mDrawableCaptor.getValue()).getCreatedFromResId());
    }

    @Test
    public void testRetrieveFavicon_StarterPack_History() {
        doReturn(StarterPackId.HISTORY).when(mTemplateUrl).getStarterPackId();

        mSearchEngineAssets.retrieveFavicon(
                mTemplateUrlService, mTemplateUrl, DESIRED_SIZE_PX, mCallback);
        verify(mLargeIconBridge, never()).getLargeIconForUrl(any(), anyInt(), any());

        verify(mCallback).onResult(mDrawableCaptor.capture());
        assertNotNull(mDrawableCaptor.getValue());
        assertEquals(
                R.drawable.search_engine_default,
                Shadows.shadowOf(mDrawableCaptor.getValue()).getCreatedFromResId());
    }

    @Test
    public void testRetrieveFavicon_StarterPack_Tabs() {
        doReturn(StarterPackId.TABS).when(mTemplateUrl).getStarterPackId();

        mSearchEngineAssets.retrieveFavicon(
                mTemplateUrlService, mTemplateUrl, DESIRED_SIZE_PX, mCallback);
        verify(mLargeIconBridge, never()).getLargeIconForUrl(any(), anyInt(), any());

        verify(mCallback).onResult(mDrawableCaptor.capture());
        assertNotNull(mDrawableCaptor.getValue());
        assertEquals(
                R.drawable.search_engine_default,
                Shadows.shadowOf(mDrawableCaptor.getValue()).getCreatedFromResId());
    }

    @Test
    public void testRetrieveFavicon_StarterPack_Gemini() {
        doReturn(StarterPackId.GEMINI).when(mTemplateUrl).getStarterPackId();

        mSearchEngineAssets.retrieveFavicon(
                mTemplateUrlService, mTemplateUrl, DESIRED_SIZE_PX, mCallback);
        verify(mLargeIconBridge, never()).getLargeIconForUrl(any(), anyInt(), any());

        verify(mCallback).onResult(mDrawableCaptor.capture());
        assertNotNull(mDrawableCaptor.getValue());
        assertEquals(
                R.drawable.search_engine_gemini,
                Shadows.shadowOf(mDrawableCaptor.getValue()).getCreatedFromResId());
    }

    @Test
    public void testRetrieveFavicon_LargeIconBridgeSuccess() {
        doReturn(SearchEngineType.SEARCH_ENGINE_OTHER)
                .when(mTemplateUrlService)
                .getSearchEngineTypeFromTemplateUrl("example.com");
        doReturn(true)
                .when(mLargeIconBridge)
                .getLargeIconForUrl(any(GURL.class), anyInt(), mLargeIconCallbackCaptor.capture());

        mSearchEngineAssets.retrieveFavicon(
                mTemplateUrlService, mTemplateUrl, DESIRED_SIZE_PX, mCallback);

        mLargeIconCallbackCaptor.getValue().onLargeIconAvailable(mBitmap, 0, false, 0);

        verify(mCallback).onResult(mDrawableCaptor.capture());
        assertTrue(mDrawableCaptor.getValue() instanceof BitmapDrawable);
        assertEquals(mBitmap, ((BitmapDrawable) mDrawableCaptor.getValue()).getBitmap());
    }

    @Test
    public void testRetrieveFavicon_LargeIconBridgeReturnsNull() {
        doReturn(SearchEngineType.SEARCH_ENGINE_OTHER)
                .when(mTemplateUrlService)
                .getSearchEngineTypeFromTemplateUrl("example.com");
        doReturn(true)
                .when(mLargeIconBridge)
                .getLargeIconForUrl(any(GURL.class), anyInt(), mLargeIconCallbackCaptor.capture());

        mSearchEngineAssets.retrieveFavicon(
                mTemplateUrlService, mTemplateUrl, DESIRED_SIZE_PX, mCallback);

        mLargeIconCallbackCaptor.getValue().onLargeIconAvailable(null, 0, false, 0);

        verify(mCallback).onResult(mDrawableCaptor.capture());
        assertEquals(
                R.drawable.search_engine_default,
                Shadows.shadowOf(mDrawableCaptor.getValue()).getCreatedFromResId());
    }

    @Test
    public void testRetrieveFavicon_LargeIconBridgeWillNotCall() {
        doReturn(SearchEngineType.SEARCH_ENGINE_OTHER)
                .when(mTemplateUrlService)
                .getSearchEngineTypeFromTemplateUrl("example.com");
        doReturn(false).when(mLargeIconBridge).getLargeIconForUrl(any(GURL.class), anyInt(), any());

        mSearchEngineAssets.retrieveFavicon(
                mTemplateUrlService, mTemplateUrl, DESIRED_SIZE_PX, mCallback);

        verify(mCallback).onResult(mDrawableCaptor.capture());
        assertEquals(
                R.drawable.search_engine_default,
                Shadows.shadowOf(mDrawableCaptor.getValue()).getCreatedFromResId());
    }
}
