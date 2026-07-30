// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.when;
import static org.robolectric.Shadows.shadowOf;

import android.app.Activity;
import android.content.ComponentName;
import android.content.Intent;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkItem;
import org.chromium.components.bookmarks.BookmarkType;
import org.chromium.url.GURL;

import java.util.Collections;

/** Unit tests for {@link BookmarkOpenerImpl}. */
@RunWith(BaseRobolectricTestRunner.class)
public class BookmarkOpenerImplUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private BookmarkModel mBookmarkModel;

    private Activity mActivity;
    private BookmarkOpenerImpl mOpener;
    private BookmarkId mBookmarkId;
    private BookmarkItem mBookmarkItem;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).get();

        mBookmarkId = new BookmarkId(1, BookmarkType.NORMAL);
        mBookmarkItem =
                new BookmarkItem(
                        mBookmarkId,
                        "Title",
                        new GURL("https://example.com"),
                        /* isFolder= */ false,
                        /* parentId= */ null,
                        /* isEditable= */ true,
                        /* isManaged= */ false,
                        /* dateAdded= */ 0,
                        /* read= */ false,
                        /* dateLastOpened= */ 0,
                        /* isAccountBookmark= */ false);

        when(mBookmarkModel.getBookmarkById(mBookmarkId)).thenReturn(mBookmarkItem);

        mOpener =
                new BookmarkOpenerImpl(
                        () -> mBookmarkModel,
                        mActivity,
                        new ComponentName(mActivity, "TestActivity"));
    }

    @Test
    public void testOpenBookmarkInCurrentTab() {
        assertTrue(mOpener.openBookmarkInCurrentTab(mBookmarkId, false));

        Intent startedIntent = shadowOf(mActivity).getNextStartedActivity();
        assertNotNull(startedIntent);
    }

    @Test
    public void testOpenBookmarksInNewTabs() {
        assertTrue(
                mOpener.openBookmarksInNewTabs(
                        Collections.singletonList(mBookmarkId), /* incognito= */ false));

        Intent startedIntent = shadowOf(mActivity).getNextStartedActivity();
        assertNotNull(startedIntent);
    }
}
