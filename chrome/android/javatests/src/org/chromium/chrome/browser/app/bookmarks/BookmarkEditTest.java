// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.bookmarks;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.view.MenuItem;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.MediumTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.After;
import org.junit.AfterClass;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ApplicationStatus.ActivityStateListener;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.RequiresRestart;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.bookmarks.BookmarkModelObserver;
import org.chromium.chrome.browser.bookmarks.BookmarkModelTest;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.test.ChromeBrowserTestRule;
import org.chromium.chrome.test.util.BookmarkTestUtil;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkItem;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.url.GURL;

import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;

/** Tests functionality in BookmarkEditActivity. */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class BookmarkEditTest {
    @Rule public final ChromeBrowserTestRule mChromeBrowserTestRule = new ChromeBrowserTestRule();

    private static final String TITLE_A = "a";
    private static final String TITLE_B = "b";
    private static final String URL_A = "http://a.com/";
    private static final String URL_B = "http://b.com/";
    private static final String FOLDER_A = "FolderA";
    private static BookmarkModel sBookmarkModel;
    private static BookmarkModelObserver sModelObserver;
    private static BookmarkId sBookmarkId;
    private static BookmarkId sMobileNode;
    private static BookmarkId sOtherNode;
    private static BookmarkEditActivity sBookmarkEditActivity;

    private static CallbackHelper sDestroyedCallback = new CallbackHelper();
    private static ActivityStateListener sActivityStateListener =
            new ActivityStateListener() {
                @Override
                public void onActivityStateChange(Activity activity, int newState) {
                    if (newState == ActivityState.DESTROYED) sDestroyedCallback.notifyCalled();
                }
            };
    private CallbackHelper mModelChangedCallback = new CallbackHelper();

    @Before
    public void setUp() throws TimeoutException {
        if (sBookmarkEditActivity == null) {
            TestThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        sBookmarkModel =
                                BookmarkModel.getForProfile(Profile.getLastUsedRegularProfile());
                        sBookmarkModel.loadEmptyPartnerBookmarkShimForTesting();
                    });

            BookmarkTestUtil.waitForBookmarkModelLoaded();
            TestThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        sMobileNode = sBookmarkModel.getMobileFolderId();
                        sOtherNode = sBookmarkModel.getOtherFolderId();
                    });
            sBookmarkId =
                    BookmarkModelTest.addBookmark(
                            sBookmarkModel, sMobileNode, 0, TITLE_A, new GURL(URL_A));

            sModelObserver =
                    new BookmarkModelObserver() {
                        @Override
                        public void bookmarkModelChanged() {
                            mModelChangedCallback.notifyCalled();
                        }
                    };
            TestThreadUtils.runOnUiThreadBlocking(() -> sBookmarkModel.addObserver(sModelObserver));

            startEditActivity(sBookmarkId);

            TestThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        ApplicationStatus.registerStateListenerForActivity(
                                sActivityStateListener, sBookmarkEditActivity);
                    });
        }
    }

    @After
    public void resetBookmark() throws ExecutionException {
        if (getBookmarkItem(sBookmarkId) != null) {
            TestThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        sBookmarkModel.setBookmarkTitle(sBookmarkId, TITLE_A);
                        sBookmarkModel.setBookmarkUrl(sBookmarkId, new GURL(URL_A));
                        sBookmarkModel.moveBookmark(sBookmarkId, sMobileNode, 0);
                    });
        }
        if (sBookmarkEditActivity != null) {
            TestThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        sBookmarkEditActivity.getTitleEditText().getEditText().setText(TITLE_A);
                        sBookmarkEditActivity.getUrlEditText().getEditText().setText(URL_A);
                    });
        }
    }

    @AfterClass
    public static void tearDown() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    sBookmarkModel.removeObserver(sModelObserver);
                    sBookmarkModel.removeAllUserBookmarks();
                    ApplicationStatus.unregisterActivityStateListener(sActivityStateListener);
                });
    }

    @Test
    @MediumTest
    @Feature({"Bookmark"})
    public void testEditTitleAndUrl() throws ExecutionException, TimeoutException {
        Assert.assertEquals(
                "Incorrect title.",
                TITLE_A,
                sBookmarkEditActivity.getTitleEditText().getEditText().getText().toString());
        Assert.assertEquals(
                "Incorrect url.",
                URL_A,
                sBookmarkEditActivity.getUrlEditText().getEditText().getText().toString());

        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    sBookmarkEditActivity.getTitleEditText().getEditText().setText(TITLE_B);
                    sBookmarkEditActivity.getUrlEditText().getEditText().setText(URL_B);
                    sBookmarkEditActivity.onStop();
                });

        BookmarkItem bookmarkItem = getBookmarkItem(sBookmarkId);
        Assert.assertEquals("Incorrect title after edit.", TITLE_B, bookmarkItem.getTitle());
        Assert.assertEquals("Incorrect url after edit.", URL_B, bookmarkItem.getUrl().getSpec());
    }

    @Test
    @MediumTest
    @Feature({"Bookmark"})
    public void testEditEmptyInputRejected() throws ExecutionException, TimeoutException {
        Assert.assertEquals(
                "Incorrect title.",
                TITLE_A,
                sBookmarkEditActivity.getTitleEditText().getEditText().getText().toString());
        Assert.assertEquals(
                "Incorrect url.",
                URL_A,
                sBookmarkEditActivity.getUrlEditText().getEditText().getText().toString());

        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    sBookmarkEditActivity.getTitleEditText().getEditText().setText("");
                    sBookmarkEditActivity.getUrlEditText().getEditText().setText("");
                    sBookmarkEditActivity.onStop();
                });

        BookmarkItem bookmarkItem = getBookmarkItem(sBookmarkId);
        Assert.assertEquals("Incorrect title after edit.", TITLE_A, bookmarkItem.getTitle());
        Assert.assertEquals("Incorrect url after edit.", URL_A, bookmarkItem.getUrl().getSpec());
    }

    @Test
    @MediumTest
    @Feature({"Bookmark"})
    public void testInvalidUrlRejected() throws ExecutionException, TimeoutException {
        Assert.assertEquals(
                "Incorrect url.",
                URL_A,
                sBookmarkEditActivity.getUrlEditText().getEditText().getText().toString());

        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    sBookmarkEditActivity
                            .getUrlEditText()
                            .getEditText()
                            .setText("http:://?foo=bar");
                    sBookmarkEditActivity.onStop();
                });

        BookmarkItem bookmarkItem = getBookmarkItem(sBookmarkId);
        Assert.assertEquals("Incorrect url after edit.", URL_A, bookmarkItem.getUrl().getSpec());
    }

    @Test
    @MediumTest
    @Feature({"Bookmark"})
    @RequiresRestart("tests destruction of BookmarkEditActivity")
    public void testEditActivityDeleteButton() throws ExecutionException, TimeoutException {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    sBookmarkEditActivity.onOptionsItemSelected(
                            sBookmarkEditActivity.getDeleteButton());
                });
        sDestroyedCallback.waitForCallback(0);

        BookmarkItem bookmarkItem = getBookmarkItem(sBookmarkId);
        Assert.assertNull("Bookmark item should have been deleted.", bookmarkItem);
    }

    @Test
    @MediumTest
    @Feature({"Bookmark"})
    @RequiresRestart("tests destruction of BookmarkEditActivity")
    public void testEditActivityHomeButton() throws ExecutionException, TimeoutException {
        MenuItem item = Mockito.mock(MenuItem.class);
        Mockito.when(item.getItemId()).thenReturn(android.R.id.home);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> sBookmarkEditActivity.onOptionsItemSelected(item));

        Assert.assertTrue(
                "BookmarkActivity should be finishing or destroyed.",
                sBookmarkEditActivity.isFinishing() || sBookmarkEditActivity.isDestroyed());
    }

    @Test
    @MediumTest
    @Feature({"Bookmark"})
    public void testEditActivityReflectsModelChanges() throws TimeoutException, ExecutionException {
        Assert.assertEquals(
                "Incorrect title.",
                TITLE_A,
                sBookmarkEditActivity.getTitleEditText().getEditText().getText().toString());
        Assert.assertEquals(
                "Incorrect folder.",
                getBookmarkItem(sMobileNode).getTitle(),
                sBookmarkEditActivity.getFolderTextView().getText());

        int currentModelChangedCount = mModelChangedCallback.getCallCount();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    sBookmarkModel.setBookmarkTitle(sBookmarkId, TITLE_B);
                    sBookmarkModel.moveBookmark(sBookmarkId, sOtherNode, 0);
                });
        mModelChangedCallback.waitForCallback(currentModelChangedCount);

        Assert.assertEquals(
                "Title shouldn't change after model update.",
                TITLE_A,
                sBookmarkEditActivity.getTitleEditText().getEditText().getText().toString());
        Assert.assertEquals(
                "Folder should change after model update.",
                getBookmarkItem(sOtherNode).getTitle(),
                sBookmarkEditActivity.getFolderTextView().getText());
    }

    @Test
    @MediumTest
    @Feature({"Bookmark"})
    @RequiresRestart("tests destruction of BookmarkEditActivity")
    public void testEditActivityFinishesWhenBookmarkDeleted() throws TimeoutException {
        int currentModelChangedCount = mModelChangedCallback.getCallCount();
        TestThreadUtils.runOnUiThreadBlocking(() -> sBookmarkModel.deleteBookmark(sBookmarkId));
        mModelChangedCallback.waitForCallback(currentModelChangedCount);

        Assert.assertTrue(
                "BookmarkActivity should be finishing or destroyed.",
                sBookmarkEditActivity.isFinishing() || sBookmarkEditActivity.isDestroyed());
    }

    @Test
    @MediumTest
    @Feature({"Bookmark"})
    public void testEditFolderLocation() throws ExecutionException, TimeoutException {
        BookmarkId testFolder = addFolder(sMobileNode, 0, FOLDER_A);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> sBookmarkEditActivity.getFolderTextView().performClick());
        waitForMoveFolderActivity();

        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    BookmarkFolderSelectActivity folderSelectActivity =
                            (BookmarkFolderSelectActivity)
                                    ApplicationStatus.getLastTrackedFocusedActivity();
                    int pos = folderSelectActivity.getFolderPositionForTesting(testFolder);
                    Assert.assertNotEquals("Didn't find position for test folder.", -1, pos);
                    folderSelectActivity.performClickForTesting(pos);
                });

        waitForEditActivity();
        Assert.assertEquals(
                "Folder should change after folder activity finishes.",
                FOLDER_A,
                sBookmarkEditActivity.getFolderTextView().getText());
    }

    @Test
    @MediumTest
    @Feature({"Bookmark"})
    @RequiresRestart("tests destruction of BookmarkEditActivity")
    public void testChangeFolderWhenBookmarkRemoved() throws ExecutionException, TimeoutException {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> sBookmarkEditActivity.getFolderTextView().performClick());
        waitForMoveFolderActivity();

        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertTrue(
                            "Expected BookmarkFolderSelectActivity.",
                            ApplicationStatus.getLastTrackedFocusedActivity()
                                    instanceof BookmarkFolderSelectActivity);
                    sBookmarkModel.deleteBookmark(sBookmarkId);
                });
        CriteriaHelper.pollUiThread(
                () ->
                        !(ApplicationStatus.getLastTrackedFocusedActivity()
                                instanceof BookmarkFolderSelectActivity),
                "Timed out waiting for BookmarkFolderSelectActivity to close");
    }

    private BookmarkItem getBookmarkItem(BookmarkId bookmarkId) throws ExecutionException {
        return TestThreadUtils.runOnUiThreadBlocking(
                () -> sBookmarkModel.getBookmarkById(bookmarkId));
    }

    private static void startEditActivity(BookmarkId bookmarkId) {
        Context context = ApplicationProvider.getApplicationContext();
        Intent intent = new Intent(context, BookmarkEditActivity.class);
        intent.putExtra(BookmarkEditActivity.INTENT_BOOKMARK_ID, bookmarkId.toString());
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        sBookmarkEditActivity =
                (BookmarkEditActivity)
                        InstrumentationRegistry.getInstrumentation().startActivitySync(intent);
    }

    private BookmarkId addFolder(BookmarkId parent, int index, String title)
            throws ExecutionException {
        return TestThreadUtils.runOnUiThreadBlocking(
                () -> sBookmarkModel.addFolder(parent, index, title));
    }

    private void waitForMoveFolderActivity() {
        CriteriaHelper.pollUiThread(
                () ->
                        ApplicationStatus.getLastTrackedFocusedActivity()
                                instanceof BookmarkFolderSelectActivity,
                "Timed out waiting for BookmarkFolderSelectActivity");
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
    }

    private void waitForEditActivity() {
        CriteriaHelper.pollUiThread(
                () ->
                        ApplicationStatus.getLastTrackedFocusedActivity()
                                instanceof BookmarkEditActivity,
                "Timed out waiting for BookmarkEditActivity");
        sBookmarkEditActivity =
                (BookmarkEditActivity) ApplicationStatus.getLastTrackedFocusedActivity();
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
    }
}
