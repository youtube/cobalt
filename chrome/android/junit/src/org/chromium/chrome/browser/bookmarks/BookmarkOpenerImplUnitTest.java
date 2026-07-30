// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.robolectric.Shadows.shadowOf;

import android.app.Activity;
import android.app.ActivityManager.AppTask;
import android.app.Application;
import android.content.ComponentName;
import android.content.Intent;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.app.tabwindow.TabWindowManagerSingleton;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.tabwindow.TabWindowManager;
import org.chromium.chrome.browser.util.AndroidTaskUtils;
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
    @Mock private TabWindowManager mTabWindowManager;
    @Mock private ChromeTabbedActivity mChromeTabbedActivity;
    @Mock private AppTask mAppTask;

    private Activity mActivity;
    private BookmarkOpenerImpl mOpener;
    private BookmarkId mBookmarkId;
    private BookmarkItem mBookmarkItem;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).get();
        TabWindowManagerSingleton.setTabWindowManagerForTesting(mTabWindowManager);

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
    public void testOpenBookmarkInCurrentTab_routesToWindowInstance() {
        int windowId = 2;
        int taskId = 10;

        // Stub TabWindowManager to return a valid window ID.
        when(mTabWindowManager.getIdForWindow(mActivity)).thenReturn(windowId);

        // Associate the window ID with a ChromeTabbedActivity and AppTask.
        MultiWindowUtils.setActivityByWindowIdForTesting(windowId, mChromeTabbedActivity);
        when(mChromeTabbedActivity.getTaskId()).thenReturn(taskId);
        AndroidTaskUtils.setAppTaskForTesting(mAppTask);
        MultiWindowUtils.setMultiInstanceApi31EnabledForTesting(true);

        assertTrue(mOpener.openBookmarkInCurrentTab(mBookmarkId, false));

        // Verify the intent was routed via AppTask.startActivity rather than Context.startActivity.
        verify(mAppTask).startActivity(eq(ContextUtils.getApplicationContext()), any(), eq(null));
        assertNull(shadowOf(mActivity).getNextStartedActivity());
        assertNull(
                shadowOf((Application) ApplicationProvider.getApplicationContext())
                        .getNextStartedActivity());
    }

    @Test
    public void testOpenBookmarkInCurrentTab_invalidWindowId_fallsBackToActivityContext() {
        when(mTabWindowManager.getIdForWindow(mActivity))
                .thenReturn(TabWindowManager.INVALID_WINDOW_ID);

        assertTrue(mOpener.openBookmarkInCurrentTab(mBookmarkId, false));

        // Verify no routing via AppTask.
        verify(mAppTask, never())
                .startActivity(eq(ContextUtils.getApplicationContext()), any(), eq(null));

        // Verify the regression: launched via mActivity (Activity Context) instead of Application
        // Context.
        Intent startedIntent = shadowOf(mActivity).getNextStartedActivity();
        assertNotNull(startedIntent);
        assertNull(
                shadowOf((Application) ApplicationProvider.getApplicationContext())
                        .getNextStartedActivity());
    }

    @Test
    public void testOpenBookmarkInCurrentTab_nonTabbedActivity_fallsBackToActivityContext() {
        int windowId = 2;
        Activity regularActivity = mock(Activity.class);

        when(mTabWindowManager.getIdForWindow(mActivity)).thenReturn(windowId);
        MultiWindowUtils.setActivityByWindowIdForTesting(windowId, regularActivity);

        assertTrue(mOpener.openBookmarkInCurrentTab(mBookmarkId, false));

        // Verify fallback is executed via Activity Context.
        Intent startedIntent = shadowOf(mActivity).getNextStartedActivity();
        assertNotNull(startedIntent);
        assertNull(
                shadowOf((Application) ApplicationProvider.getApplicationContext())
                        .getNextStartedActivity());
    }

    @Test
    public void testOpenBookmarksInNewTabs_routesToWindowInstance() {
        int windowId = 2;
        int taskId = 10;

        when(mTabWindowManager.getIdForWindow(mActivity)).thenReturn(windowId);
        MultiWindowUtils.setActivityByWindowIdForTesting(windowId, mChromeTabbedActivity);
        when(mChromeTabbedActivity.getTaskId()).thenReturn(taskId);
        AndroidTaskUtils.setAppTaskForTesting(mAppTask);
        MultiWindowUtils.setMultiInstanceApi31EnabledForTesting(true);

        assertTrue(
                mOpener.openBookmarksInNewTabs(
                        Collections.singletonList(mBookmarkId), /* incognito= */ false));

        verify(mAppTask).startActivity(eq(ContextUtils.getApplicationContext()), any(), eq(null));
        assertNull(shadowOf(mActivity).getNextStartedActivity());
        assertNull(
                shadowOf((Application) ApplicationProvider.getApplicationContext())
                        .getNextStartedActivity());
    }

    @Test
    public void testOpenBookmarksInNewTabs_invalidWindowId_fallsBackToActivityContext() {
        when(mTabWindowManager.getIdForWindow(mActivity))
                .thenReturn(TabWindowManager.INVALID_WINDOW_ID);

        assertTrue(
                mOpener.openBookmarksInNewTabs(
                        Collections.singletonList(mBookmarkId), /* incognito= */ false));

        verify(mAppTask, never())
                .startActivity(eq(ContextUtils.getApplicationContext()), any(), eq(null));

        Intent startedIntent = shadowOf(mActivity).getNextStartedActivity();
        assertNotNull(startedIntent);
        assertNull(
                shadowOf((Application) ApplicationProvider.getApplicationContext())
                        .getNextStartedActivity());
    }

    @Test
    public void testOpenBookmarksInNewTabs_nonTabbedActivity_fallsBackToActivityContext() {
        int windowId = 2;
        Activity regularActivity = mock(Activity.class);

        when(mTabWindowManager.getIdForWindow(mActivity)).thenReturn(windowId);
        MultiWindowUtils.setActivityByWindowIdForTesting(windowId, regularActivity);

        assertTrue(
                mOpener.openBookmarksInNewTabs(
                        Collections.singletonList(mBookmarkId), /* incognito= */ false));

        Intent startedIntent = shadowOf(mActivity).getNextStartedActivity();
        assertNotNull(startedIntent);
        assertNull(
                shadowOf((Application) ApplicationProvider.getApplicationContext())
                        .getNextStartedActivity());
    }
}
