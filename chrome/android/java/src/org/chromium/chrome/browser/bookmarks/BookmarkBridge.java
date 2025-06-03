// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.os.SystemClock;
import android.text.TextUtils;
import android.util.Pair;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import com.google.protobuf.InvalidProtocolBufferException;

import org.jni_zero.CalledByNative;
import org.jni_zero.NativeMethods;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.ObserverList;
import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.partnerbookmarks.PartnerBookmarksShim;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.read_later.ReadingListUtils;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkItem;
import org.chromium.components.bookmarks.BookmarkType;
import org.chromium.components.power_bookmarks.PowerBookmarkMeta;
import org.chromium.components.power_bookmarks.PowerBookmarkType;
import org.chromium.content_public.browser.WebContents;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.List;

/**
 * Provides the communication channel for Android to fetch and manipulate the bookmark model stored
 * in native.
 */
class BookmarkBridge {
    private final ObserverList<BookmarkModelObserver> mObservers = new ObserverList<>();

    private long mNativeBookmarkBridge;
    private boolean mIsDestroyed;
    private boolean mIsDoingExtensiveChanges;
    private boolean mIsNativeBookmarkModelLoaded;

    // Lazily set pseudo-constants. These should never change at runtime. Used to avoid crossing
    // JNI to fetch information.
    private @Nullable BookmarkId mRootFolderId;
    private @Nullable BookmarkId mMobileFolderId;
    private @Nullable BookmarkId mOtherFolderId;
    private @Nullable BookmarkId mDesktopFolderId;
    private @Nullable BookmarkId mReadingListFolderId;

    /**
     * Handler to fetch the bookmarks, titles, urls and folder hierarchy.
     *
     * @param profile Profile instance corresponding to the active profile.
     */
    static BookmarkModel getForProfile(Profile profile) {
        ThreadUtils.assertOnUiThread();
        return BookmarkBridgeJni.get().getForProfile(profile);
    }

    @CalledByNative
    static BookmarkModel createBookmarkModel(long nativeBookmarkBridge) {
        return new BookmarkModel(nativeBookmarkBridge);
    }

    BookmarkBridge(long nativeBookmarkBridge) {
        mNativeBookmarkBridge = nativeBookmarkBridge;
        mIsDoingExtensiveChanges =
                BookmarkBridgeJni.get().isDoingExtensiveChanges(mNativeBookmarkBridge);
    }

    /** Destroys this instance so no further calls can be executed. */
    void destroy() {
        mIsDestroyed = true;
        if (mNativeBookmarkBridge != 0) {
            BookmarkBridgeJni.get().destroy(mNativeBookmarkBridge);
            mNativeBookmarkBridge = 0;
            mIsNativeBookmarkModelLoaded = false;
        }
        mObservers.clear();
    }

    /** Returns whether the bridge has been destroyed. */
    private boolean isDestroyed() {
        return mIsDestroyed;
    }

    /**
     * Gets the url for an image representing the given url.
     *
     * @param url The url to fetch the image for.
     * @param callback The callback which will receive the image url.
     */
    public void getImageUrlForBookmark(GURL url, Callback<GURL> callback) {
        BookmarkBridgeJni.get().getImageUrlForBookmark(mNativeBookmarkBridge, url, callback);
    }

    /**
     * @param tab Tab whose current URL is checked against.
     * @return {@code true} if the current Tab URL has a bookmark associated with it. If the
     *     bookmark backend is not loaded, return {@code false}.
     */
    public boolean hasBookmarkIdForTab(@Nullable Tab tab) {
        ThreadUtils.assertOnUiThread();
        if (mNativeBookmarkBridge == 0) return false;
        return getUserBookmarkIdForTab(tab) != null;
    }

    /**
     * @param tab Tab whose current URL is checked against.
     * @return BookmarkId or {@link null} if bookmark backend is not loaded or the tab is frozen.
     */
    public @Nullable BookmarkId getUserBookmarkIdForTab(@Nullable Tab tab) {
        ThreadUtils.assertOnUiThread();
        if (mNativeBookmarkBridge == 0) return null;
        if (tab == null || tab.isFrozen() || mNativeBookmarkBridge == 0) return null;
        return BookmarkBridgeJni.get()
                .getBookmarkIdForWebContents(mNativeBookmarkBridge, tab.getWebContents(), true);
    }

    /**
     * Load an empty partner bookmark shim for testing. The root node for bookmark will be an empty
     * node.
     */
    public void loadEmptyPartnerBookmarkShimForTesting() {
        BookmarkBridgeJni.get()
                .loadEmptyPartnerBookmarkShimForTesting(mNativeBookmarkBridge); // IN-TEST
    }

    /**
     * Load a fake partner bookmark shim for testing. To see (or edit) the titles and URLs of the
     * partner bookmarks, go to bookmark_bridge.cc.
     */
    public void loadFakePartnerBookmarkShimForTesting() {
        BookmarkBridgeJni.get()
                .loadFakePartnerBookmarkShimForTesting(mNativeBookmarkBridge); // IN-TEST
    }

    /**
     * Add an observer to bookmark model changes.
     *
     * @param observer The observer to be added.
     */
    public void addObserver(BookmarkModelObserver observer) {
        mObservers.addObserver(observer);
    }

    /**
     * Remove an observer of bookmark model changes.
     *
     * @param observer The observer to be removed.
     */
    public void removeObserver(BookmarkModelObserver observer) {
        mObservers.removeObserver(observer);
    }

    /**
     * @return Whether or not the underlying bookmark model is loaded.
     */
    public boolean isBookmarkModelLoaded() {
        return mIsNativeBookmarkModelLoaded;
    }

    /**
     * Schedules a runnable to run after the bookmark model is loaded. If the model is already
     * loaded, executes the runnable immediately. If not, also kick off partner bookmark reading.
     *
     * @return Whether the given runnable is executed synchronously.
     */
    public boolean finishLoadingBookmarkModel(final Runnable runAfterModelLoaded) {
        if (isBookmarkModelLoaded()) {
            runAfterModelLoaded.run();
            return true;
        }

        long startTime = SystemClock.elapsedRealtime();
        addObserver(
                new BookmarkModelObserver() {
                    @Override
                    public void bookmarkModelLoaded() {
                        removeObserver(this);
                        RecordHistogram.recordTimesHistogram(
                                "PartnerBookmark.LoadingTime",
                                SystemClock.elapsedRealtime() - startTime);
                        runAfterModelLoaded.run();
                    }

                    @Override
                    public void bookmarkModelChanged() {}
                });

        // Start reading as a fail-safe measure to avoid waiting forever if the caller forgets to
        // call kickOffReading().
        PartnerBookmarksShim.kickOffReading(ContextUtils.getApplicationContext());
        return false;
    }

    /**
     * Gets the {@link BookmarkItem} which is referenced by the given {@link BookmarkId}.
     *
     * @param id The {@link BookmarkId} used to lookup the corresponding {@link BookmarkItem}.
     * @return A BookmarkItem instance for the given BookmarkId. <code>null</code> if it doesn't
     *     exist.
     */
    public @Nullable BookmarkItem getBookmarkById(@Nullable BookmarkId id) {
        ThreadUtils.assertOnUiThread();
        if (mNativeBookmarkBridge == 0) return null;
        assert mIsNativeBookmarkModelLoaded;
        if (id == null) return null;

        if (BookmarkId.SHOPPING_FOLDER.equals(id)) {
            return new BookmarkItem(
                    id,
                    /* title= */ null,
                    /* url= */ null,
                    /* isFolder= */ true,
                    /* parentId= */ getRootFolderId(),
                    /* isEditable= */ false,
                    /* isManaged= */ false,
                    /* dateAdded= */ 0L,
                    /* read= */ false,
                    /* dateLastOpened= */ 0L);
        }

        return BookmarkBridgeJni.get()
                .getBookmarkById(mNativeBookmarkBridge, id.getId(), id.getType());
    }

    /**
     * @return The top level folders, including special folders (managed bookmarks, reading list,
     *     partner bookmarks).
     */
    public List<BookmarkId> getTopLevelFolderIds() {
        ThreadUtils.assertOnUiThread();
        if (mNativeBookmarkBridge == 0) return new ArrayList<>();
        assert mIsNativeBookmarkModelLoaded;
        List<BookmarkId> result = new ArrayList<>();
        BookmarkBridgeJni.get().getTopLevelFolderIds(mNativeBookmarkBridge, result);
        return result;
    }

    /** Returns the synthetic reading list folder. */
    public BookmarkId getReadingListFolder() {
        ThreadUtils.assertOnUiThread();
        if (mNativeBookmarkBridge == 0) return null;
        assert mIsNativeBookmarkModelLoaded;
        if (mReadingListFolderId == null) {
            mReadingListFolderId =
                    BookmarkBridgeJni.get().getReadingListFolder(mNativeBookmarkBridge);
        }
        return mReadingListFolderId;
    }

    /**
     * Populates folderList with BookmarkIds of folders users can move bookmarks to and all folders
     * have corresponding depth value in depthList. Folders having depths of 0 will be shown as
     * top-layered folders. These include "Desktop Folder" itself as well as all children of
     * "mobile" and "other". Children of 0-depth folders have depth of 1, and so on.
     *
     * <p>The result list will be sorted alphabetically by title. "mobile", "other", root node,
     * managed folder, partner folder are NOT included as results.
     */
    @VisibleForTesting
    public void getAllFoldersWithDepths(List<BookmarkId> folderList, List<Integer> depthList) {
        ThreadUtils.assertOnUiThread();
        if (mNativeBookmarkBridge == 0) return;
        assert mIsNativeBookmarkModelLoaded;
        BookmarkBridgeJni.get()
                .getAllFoldersWithDepths(mNativeBookmarkBridge, folderList, depthList);
    }

    /**
     * Calls {@link #getAllFoldersWithDepths(List, List)} and remove all folders and children in
     * bookmarksToMove. This method is useful when finding a list of possible parent folers when
     * moving some folders (a folder cannot be moved to its own children).
     */
    public void getMoveDestinations(
            List<BookmarkId> folderList,
            List<Integer> depthList,
            List<BookmarkId> bookmarksToMove) {
        if (mNativeBookmarkBridge == 0) return;
        ThreadUtils.assertOnUiThread();
        assert mIsNativeBookmarkModelLoaded;
        BookmarkBridgeJni.get()
                .getAllFoldersWithDepths(mNativeBookmarkBridge, folderList, depthList);
        if (bookmarksToMove == null || bookmarksToMove.size() == 0) return;

        boolean shouldTrim = false;
        int trimThreshold = -1;
        for (int i = 0; i < folderList.size(); i++) {
            int depth = depthList.get(i);
            if (shouldTrim) {
                if (depth <= trimThreshold) {
                    shouldTrim = false;
                    trimThreshold = -1;
                } else {
                    folderList.remove(i);
                    depthList.remove(i);
                    i--;
                }
            }
            // Do not use else here because shouldTrim could be set true after if (shouldTrim)
            // statement.
            if (!shouldTrim) {
                BookmarkId folder = folderList.get(i);
                if (bookmarksToMove.contains(folder)) {
                    shouldTrim = true;
                    trimThreshold = depth;
                    folderList.remove(i);
                    depthList.remove(i);
                    i--;
                }
            }
        }
    }

    /**
     * @return The BookmarkId for root folder node
     */
    public BookmarkId getRootFolderId() {
        ThreadUtils.assertOnUiThread();
        if (mNativeBookmarkBridge == 0) return null;
        assert mIsNativeBookmarkModelLoaded;
        if (mRootFolderId == null) {
            mRootFolderId = BookmarkBridgeJni.get().getRootFolderId(mNativeBookmarkBridge);
        }
        return mRootFolderId;
    }

    /**
     * @return The BookmarkId for Mobile folder node
     */
    public BookmarkId getMobileFolderId() {
        ThreadUtils.assertOnUiThread();
        if (mNativeBookmarkBridge == 0) return null;
        assert mIsNativeBookmarkModelLoaded;
        if (mMobileFolderId == null) {
            mMobileFolderId = BookmarkBridgeJni.get().getMobileFolderId(mNativeBookmarkBridge);
        }
        return mMobileFolderId;
    }

    /**
     * @return Id representing the special "other" folder from bookmark model.
     */
    public BookmarkId getOtherFolderId() {
        ThreadUtils.assertOnUiThread();
        if (mNativeBookmarkBridge == 0) return null;
        assert mIsNativeBookmarkModelLoaded;
        if (mOtherFolderId == null) {
            mOtherFolderId = BookmarkBridgeJni.get().getOtherFolderId(mNativeBookmarkBridge);
        }
        return mOtherFolderId;
    }

    /**
     * @return BookmarkId representing special "desktop" folder, namely "bookmark bar".
     */
    public BookmarkId getDesktopFolderId() {
        ThreadUtils.assertOnUiThread();
        if (mNativeBookmarkBridge == 0) return null;
        assert mIsNativeBookmarkModelLoaded;
        if (mDesktopFolderId == null) {
            mDesktopFolderId = BookmarkBridgeJni.get().getDesktopFolderId(mNativeBookmarkBridge);
        }
        return mDesktopFolderId;
    }

    /**
     * Gets Bookmark GUID which is immutable and differs from the BookmarkId in that it is
     * consistent across different clients and stable throughout the lifetime of the bookmark, with
     * the exception of nodes added to the Managed Bookmarks folder, whose GUIDs are re-assigned at
     * start-up every time.
     *
     * @return Bookmark GUID of the given node.
     */
    public String getBookmarkGuidByIdForTesting(BookmarkId id) {
        ThreadUtils.assertOnUiThread();
        if (mNativeBookmarkBridge == 0) return null;
        assert mIsNativeBookmarkModelLoaded;
        return BookmarkBridgeJni.get()
                .getBookmarkGuidByIdForTesting(mNativeBookmarkBridge, id.getId(), id.getType());
    }

    /**
     * @return The number of children that the given node has.
     */
    public int getChildCount(BookmarkId id) {
        ThreadUtils.assertOnUiThread();
        if (mNativeBookmarkBridge == 0) return 0;
        assert mIsNativeBookmarkModelLoaded;
        return BookmarkBridgeJni.get()
                .getChildCount(mNativeBookmarkBridge, id.getId(), id.getType());
    }

    /**
     * Reads sub-folder IDs, sub-bookmark IDs, or both of the given folder.
     *
     * @return Child IDs of the given folder, with the specified type.
     */
    public List<BookmarkId> getChildIds(BookmarkId id) {
        ThreadUtils.assertOnUiThread();
        if (mNativeBookmarkBridge == 0) return new ArrayList<>();
        assert mIsNativeBookmarkModelLoaded;
        if (BookmarkId.SHOPPING_FOLDER.equals(id)) {
            return searchBookmarks("", null, PowerBookmarkType.SHOPPING, -1);
        }
        List<BookmarkId> result = new ArrayList<>();
        BookmarkBridgeJni.get()
                .getChildIds(mNativeBookmarkBridge, id.getId(), id.getType(), result);
        return result;
    }

    /**
     * Gets the child of a folder at the specific position.
     *
     * @param folderId Id of the parent folder
     * @param index Position of child among all children in folder
     * @return BookmarkId of the child, which will be null if folderId does not point to a folder or
     *     index is invalid.
     */
    public BookmarkId getChildAt(BookmarkId folderId, int index) {
        ThreadUtils.assertOnUiThread();
        if (mNativeBookmarkBridge == 0) return null;
        assert mIsNativeBookmarkModelLoaded;
        return BookmarkBridgeJni.get()
                .getChildAt(mNativeBookmarkBridge, folderId.getId(), folderId.getType(), index);
    }

    /**
     * Get the total number of bookmarks in the sub tree of the specified folder.
     *
     * @param id The {@link BookmarkId} of the folder to be queried.
     * @return The total number of bookmarks in the folder.
     */
    public int getTotalBookmarkCount(BookmarkId id) {
        ThreadUtils.assertOnUiThread();
        if (mNativeBookmarkBridge == 0) return 0;
        assert mIsNativeBookmarkModelLoaded;
        return BookmarkBridgeJni.get()
                .getTotalBookmarkCount(mNativeBookmarkBridge, id.getId(), id.getType());
    }

    /**
     * Synchronously gets a list of bookmarks that match the specified search query.
     *
     * @param query Keyword used for searching bookmarks.
     * @param maxNumberOfResult Maximum number of result to fetch.
     * @return List of bookmark IDs that are related to the given query.
     */
    public List<BookmarkId> searchBookmarks(String query, int maxNumberOfResult) {
        return searchBookmarks(query, null, null, maxNumberOfResult);
    }

    /**
     * Synchronously gets a list of bookmarks that match the specified search query.
     *
     * @param query Keyword used for searching bookmarks.
     * @param tags A list of tags the resulting bookmarks should have.
     * @param powerBookmarkType The type of power bookmark type to search for (or null for all).
     * @param maxNumberOfResult Maximum number of result to fetch.
     * @return List of bookmark IDs that are related to the given query.
     */
    public List<BookmarkId> searchBookmarks(
            String query,
            @Nullable String[] tags,
            @Nullable PowerBookmarkType powerBookmarkType,
            int maxNumberOfResult) {
        ThreadUtils.assertOnUiThread();
        if (mNativeBookmarkBridge == 0) return new ArrayList<>();
        List<BookmarkId> bookmarkMatches = new ArrayList<>();
        int typeInt = powerBookmarkType == null ? -1 : powerBookmarkType.getNumber();
        BookmarkBridgeJni.get()
                .searchBookmarks(
                        mNativeBookmarkBridge,
                        bookmarkMatches,
                        query,
                        tags,
                        typeInt,
                        maxNumberOfResult);
        return bookmarkMatches;
    }

    /**
     * Synchronously gets a list of bookmarks of the given type
     *
     * @param powerBookmarkType The type of power bookmark type to search for (or null for all).
     * @return List of bookmark IDs that are related to the given query.
     */
    public List<BookmarkId> getBookmarksOfType(@NonNull PowerBookmarkType powerBookmarkType) {
        ThreadUtils.assertOnUiThread();
        if (mNativeBookmarkBridge == 0) return new ArrayList<>();
        List<BookmarkId> bookmarkMatches = new ArrayList<>();
        int typeInt = powerBookmarkType.getNumber();
        BookmarkBridgeJni.get().getBookmarksOfType(mNativeBookmarkBridge, bookmarkMatches, typeInt);
        return bookmarkMatches;
    }

    /** Set title of the given bookmark. */
    public void setBookmarkTitle(BookmarkId id, String title) {
        ThreadUtils.assertOnUiThread();
        if (mNativeBookmarkBridge == 0) return;
        assert mIsNativeBookmarkModelLoaded;
        BookmarkBridgeJni.get()
                .setBookmarkTitle(mNativeBookmarkBridge, id.getId(), id.getType(), title);
    }

    /** Set URL of the given bookmark. */
    public void setBookmarkUrl(BookmarkId id, GURL url) {
        ThreadUtils.assertOnUiThread();
        if (mNativeBookmarkBridge == 0) return;
        assert mIsNativeBookmarkModelLoaded;
        assert id.getType() == BookmarkType.NORMAL;
        BookmarkBridgeJni.get()
                .setBookmarkUrl(mNativeBookmarkBridge, id.getId(), id.getType(), url);
    }

    /**
     * Retrieve the PowerBookmarkMeta for a node if it exists.
     *
     * @param id The {@link BookmarkId} of the bookmark to fetch the meta for.
     * @return The meta or null if none exists.
     */
    public @Nullable PowerBookmarkMeta getPowerBookmarkMeta(@Nullable BookmarkId id) {
        if (mNativeBookmarkBridge == 0) return null;
        if (id == null) return null;
        byte[] protoBytes =
                BookmarkBridgeJni.get()
                        .getPowerBookmarkMeta(mNativeBookmarkBridge, id.getId(), id.getType());

        if (protoBytes == null) return null;

        try {
            return PowerBookmarkMeta.parseFrom(protoBytes);
        } catch (InvalidProtocolBufferException ex) {
            deletePowerBookmarkMeta(id);
            return null;
        }
    }

    /**
     * Set the PowerBookmarkMeta for a node. This MUST be called in order to persist any changes
     * made to the proto in the java layer.
     *
     * @param id The ID of the bookmark to set the meta on.
     * @param meta The meta to store.
     */
    public void setPowerBookmarkMeta(BookmarkId id, PowerBookmarkMeta meta) {
        if (mNativeBookmarkBridge == 0) return;
        if (meta == null) return;
        BookmarkBridgeJni.get()
                .setPowerBookmarkMeta(
                        mNativeBookmarkBridge, id.getId(), id.getType(), meta.toByteArray());
    }

    /**
     * Delete the PowerBookmarkMeta from a node.
     *
     * @param id The ID of the bookmark to remove the meta from.
     */
    public void deletePowerBookmarkMeta(BookmarkId id) {
        if (mNativeBookmarkBridge == 0) return;
        BookmarkBridgeJni.get()
                .deletePowerBookmarkMeta(mNativeBookmarkBridge, id.getId(), id.getType());
    }

    /** Returns whether all of the given {@link BookmarkId}s exist in the current bookmark model. */
    public boolean doAllBookmarksExist(List<BookmarkId> bookmarkIds) {
        ThreadUtils.assertOnUiThread();
        for (BookmarkId bookmarkId : bookmarkIds) {
            if (!doesBookmarkExist(bookmarkId)) {
                return false;
            }
        }
        return true;
    }

    /**
     * @return Whether the given bookmark exist in the current bookmark model, e.g., not deleted.
     */
    public boolean doesBookmarkExist(BookmarkId id) {
        ThreadUtils.assertOnUiThread();
        if (mNativeBookmarkBridge == 0) return false;
        assert mIsNativeBookmarkModelLoaded;
        return BookmarkBridgeJni.get()
                .doesBookmarkExist(mNativeBookmarkBridge, id.getId(), id.getType());
    }

    /**
     * Fetches the bookmarks of the given folder. This is an always-synchronous version of another
     * getBookmarksForFolder function.
     *
     * @param folderId The parent folder id.
     * @return Bookmarks of the given folder.
     */
    public List<BookmarkItem> getBookmarksForFolder(BookmarkId folderId) {
        ThreadUtils.assertOnUiThread();
        if (mNativeBookmarkBridge == 0) return new ArrayList<>();
        assert mIsNativeBookmarkModelLoaded;
        List<BookmarkItem> result = new ArrayList<>();
        BookmarkBridgeJni.get().getBookmarksForFolder(mNativeBookmarkBridge, folderId, result);
        return result;
    }

    /**
     * Check whether the given folder should be visible. This is for top permanent folders that we
     * want to hide when there is no child.
     *
     * @return Whether the given folder should be visible.
     */
    public boolean isFolderVisible(BookmarkId id) {
        ThreadUtils.assertOnUiThread();
        if (mNativeBookmarkBridge == 0) return false;
        assert mIsNativeBookmarkModelLoaded;
        if (ReadingListUtils.isSwappableReadingListItem(id)) {
            return true;
        }
        return BookmarkBridgeJni.get()
                .isFolderVisible(mNativeBookmarkBridge, id.getId(), id.getType());
    }

    /**
     * Deletes a specified bookmark node.
     *
     * @param bookmarkId The ID of the bookmark to be deleted.
     */
    public void deleteBookmark(BookmarkId bookmarkId) {
        ThreadUtils.assertOnUiThread();
        if (mNativeBookmarkBridge == 0) return;
        BookmarkBridgeJni.get().deleteBookmark(mNativeBookmarkBridge, bookmarkId);
    }

    /**
     * Removes all the non-permanent bookmark nodes that are editable by the user. Observers are
     * only notified when all nodes have been removed. There is no notification for individual node
     * removals.
     */
    public void removeAllUserBookmarks() {
        ThreadUtils.assertOnUiThread();
        if (mNativeBookmarkBridge == 0) return;
        BookmarkBridgeJni.get().removeAllUserBookmarks(mNativeBookmarkBridge);
    }

    /**
     * Move the bookmark to the new index within same folder or to a different folder.
     *
     * @param bookmarkId The id of the bookmark that is being moved.
     * @param newParentId The parent folder id.
     * @param index The new index for the bookmark.
     */
    public void moveBookmark(BookmarkId bookmarkId, BookmarkId newParentId, int index) {
        ThreadUtils.assertOnUiThread();
        if (mNativeBookmarkBridge == 0) return;
        BookmarkBridgeJni.get().moveBookmark(mNativeBookmarkBridge, bookmarkId, newParentId, index);
    }

    /**
     * Add a new folder to the given parent folder
     *
     * @param parent Folder where to add. Must be a normal editable folder, instead of a partner
     *     bookmark folder or a managed bookmark folder or root node of the entire bookmark model.
     * @param index The position to locate the new folder
     * @param title The title text of the new folder
     * @return Id of the added node. If adding failed (index is invalid, string is null, parent is
     *     not editable), returns null.
     */
    public BookmarkId addFolder(BookmarkId parent, int index, String title) {
        ThreadUtils.assertOnUiThread();
        if (mNativeBookmarkBridge == 0) return null;
        assert parent.getType() == BookmarkType.NORMAL;
        assert index >= 0;
        assert title != null;

        return BookmarkBridgeJni.get().addFolder(mNativeBookmarkBridge, parent, index, title);
    }

    /**
     * Add a new bookmark to a specific position below parent.
     *
     * @param parent Folder where to add. Must be a normal editable folder, instead of a partner
     *     bookmark folder or a managed bookmark folder or root node of the entire bookmark model.
     * @param index The position where the bookmark will be placed in parent folder
     * @param title Title of the new bookmark. If empty, the URL will be used as the title.
     * @param url Url of the new bookmark
     * @return Id of the added node. If adding failed (index is invalid, string is null, parent is
     *     not editable), returns null.
     */
    public BookmarkId addBookmark(BookmarkId parent, int index, String title, GURL url) {
        ThreadUtils.assertOnUiThread();
        if (mNativeBookmarkBridge == 0) return null;
        assert parent.getType() == BookmarkType.NORMAL;
        assert index >= 0;
        assert title != null;
        assert url != null;

        recordBookmarkAdded();

        if (TextUtils.isEmpty(title)) title = url.getSpec();
        return BookmarkBridgeJni.get()
                .addBookmark(mNativeBookmarkBridge, parent, index, title, url);
    }

    /** Record the user action for adding a bookmark. */
    private void recordBookmarkAdded() {
        RecordUserAction.record("BookmarkAdded");
    }

    /** Undo the last undoable action on the top of the bookmark undo stack */
    public void undo() {
        ThreadUtils.assertOnUiThread();
        if (mNativeBookmarkBridge == 0) return;
        BookmarkBridgeJni.get().undo(mNativeBookmarkBridge);
    }

    /**
     * Start grouping actions for a single undo operation Note: This only works with BookmarkModel,
     * not partner bookmarks.
     */
    public void startGroupingUndos() {
        ThreadUtils.assertOnUiThread();
        if (mNativeBookmarkBridge == 0) return;
        BookmarkBridgeJni.get().startGroupingUndos(mNativeBookmarkBridge);
    }

    /**
     * End grouping actions for a single undo operation Note: This only works with BookmarkModel,
     * not partner bookmarks.
     */
    public void endGroupingUndos() {
        ThreadUtils.assertOnUiThread();
        if (mNativeBookmarkBridge == 0) return;
        BookmarkBridgeJni.get().endGroupingUndos(mNativeBookmarkBridge);
    }

    public boolean isEditBookmarksEnabled() {
        ThreadUtils.assertOnUiThread();
        if (mNativeBookmarkBridge == 0) return false;
        return BookmarkBridgeJni.get().isEditBookmarksEnabled(mNativeBookmarkBridge);
    }

    /** Notifies the observer that bookmark model has been loaded. */
    @VisibleForTesting
    public void notifyBookmarkModelLoaded() {
        // Call isBookmarkModelLoaded() to do the check since it could be overridden by the child
        // class to add the addition logic.
        if (isBookmarkModelLoaded()) {
            for (BookmarkModelObserver observer : mObservers) {
                observer.bookmarkModelLoaded();
            }
        }
    }

    /**
     * Reorders the bookmarks of the folder "parent" to be as specified by newOrder.
     *
     * @param parent The parent folder for the reordered bookmarks.
     * @param newOrder A list of bookmark IDs that represents the new order for these bookmarks.
     */
    public void reorderBookmarks(BookmarkId parent, long[] newOrder) {
        ThreadUtils.assertOnUiThread();
        if (mNativeBookmarkBridge == 0) return;
        BookmarkBridgeJni.get().reorderChildren(mNativeBookmarkBridge, parent, newOrder);
    }

    /**
     * Adds an article to the reading list. If the article was already bookmarked, the existing
     * bookmark ID will be returned.
     *
     * @param title The title to be used for the reading list item.
     * @param url The URL of the reading list item.
     * @return The bookmark ID created after saving the article to the reading list, or null on
     *     error.
     */
    public @Nullable BookmarkId addToReadingList(String title, GURL url) {
        ThreadUtils.assertOnUiThread();
        if (mNativeBookmarkBridge == 0) return null;
        assert title != null;
        assert url != null;
        assert mIsNativeBookmarkModelLoaded;

        return BookmarkBridgeJni.get().addToReadingList(mNativeBookmarkBridge, title, url);
    }

    /**
     * @param url The URL of the reading list item.
     * @return The reading list item with the URL, or null if no such reading list item.
     */
    public BookmarkItem getReadingListItem(GURL url) {
        ThreadUtils.assertOnUiThread();
        if (mNativeBookmarkBridge == 0) return null;
        assert url != null;
        assert mIsNativeBookmarkModelLoaded;

        return BookmarkBridgeJni.get().getReadingListItem(mNativeBookmarkBridge, url);
    }

    /**
     * Helper method to mark an article as read.
     *
     * @param url The URL of the reading list item.
     * @param read Whether the article should be marked as read.
     */
    public void setReadStatusForReadingList(GURL url, boolean read) {
        if (mNativeBookmarkBridge == 0) return;
        BookmarkBridgeJni.get().setReadStatus(mNativeBookmarkBridge, url, read);
    }

    /**
     * Checks whether supplied URL has already been bookmarked.
     *
     * @param url The URL to check.
     * @return Whether the URL has been bookmarked.
     */
    public boolean isBookmarked(GURL url) {
        if (mNativeBookmarkBridge == 0) return false;
        return BookmarkBridgeJni.get().isBookmarked(mNativeBookmarkBridge, url);
    }

    public BookmarkId getPartnerFolderId() {
        ThreadUtils.assertOnUiThread();
        if (mNativeBookmarkBridge == 0) return null;

        assert mIsNativeBookmarkModelLoaded;
        return BookmarkBridgeJni.get().getPartnerFolderId(mNativeBookmarkBridge);
    }

    /** Returns the total number of unread reading list articles. */
    public int getUnreadCount() {
        ThreadUtils.assertOnUiThread();
        if (mNativeBookmarkBridge == 0) return 0;
        return BookmarkBridgeJni.get().getUnreadCount(mNativeBookmarkBridge);
    }

    @CalledByNative
    private void bookmarkModelLoaded() {
        mIsNativeBookmarkModelLoaded = true;
        notifyBookmarkModelLoaded();
    }

    @CalledByNative
    private void destroyFromNative() {
        destroy();
    }

    @CalledByNative
    private void bookmarkNodeMoved(
            BookmarkItem oldParent, int oldIndex, BookmarkItem newParent, int newIndex) {
        if (mIsDoingExtensiveChanges) return;

        for (BookmarkModelObserver observer : mObservers) {
            observer.bookmarkNodeMoved(oldParent, oldIndex, newParent, newIndex);
        }
    }

    @CalledByNative
    private void bookmarkNodeAdded(BookmarkItem parent, int index) {
        if (mIsDoingExtensiveChanges) return;

        for (BookmarkModelObserver observer : mObservers) {
            observer.bookmarkNodeAdded(parent, index);
        }
    }

    @CalledByNative
    private void bookmarkNodeRemoved(BookmarkItem parent, int oldIndex, BookmarkItem node) {
        for (BookmarkModelObserver observer : mObservers) {
            observer.bookmarkNodeRemoved(parent, oldIndex, node, mIsDoingExtensiveChanges);
        }
    }

    @CalledByNative
    private void bookmarkAllUserNodesRemoved() {
        for (BookmarkModelObserver observer : mObservers) {
            observer.bookmarkAllUserNodesRemoved();
        }
    }

    @CalledByNative
    private void bookmarkNodeChanged(BookmarkItem node) {
        if (mIsDoingExtensiveChanges) return;

        for (BookmarkModelObserver observer : mObservers) {
            observer.bookmarkNodeChanged(node);
        }
    }

    @CalledByNative
    private void bookmarkNodeChildrenReordered(BookmarkItem node) {
        if (mIsDoingExtensiveChanges) return;

        for (BookmarkModelObserver observer : mObservers) {
            observer.bookmarkNodeChildrenReordered(node);
        }
    }

    @CalledByNative
    private void extensiveBookmarkChangesBeginning() {
        mIsDoingExtensiveChanges = true;
    }

    @CalledByNative
    private void extensiveBookmarkChangesEnded() {
        mIsDoingExtensiveChanges = false;
        bookmarkModelChanged();
    }

    @CalledByNative
    private void bookmarkModelChanged() {
        if (mIsDoingExtensiveChanges) return;

        for (BookmarkModelObserver observer : mObservers) {
            observer.bookmarkModelChanged();
        }
    }

    @CalledByNative
    private void editBookmarksEnabledChanged() {
        for (BookmarkModelObserver observer : mObservers) {
            observer.editBookmarksEnabledChanged();
        }
    }

    @CalledByNative
    private static BookmarkItem createBookmarkItem(
            long id,
            int type,
            String title,
            GURL url,
            boolean isFolder,
            long parentId,
            int parentIdType,
            boolean isEditable,
            boolean isManaged,
            long dateAdded,
            boolean read,
            long dateLastOpened) {
        return new BookmarkItem(
                new BookmarkId(id, type),
                title,
                url,
                isFolder,
                new BookmarkId(parentId, parentIdType),
                isEditable,
                isManaged,
                dateAdded,
                read,
                dateLastOpened);
    }

    @CalledByNative
    private static void addToList(List<BookmarkItem> bookmarksList, BookmarkItem bookmark) {
        bookmarksList.add(bookmark);
    }

    @CalledByNative
    private static void addToBookmarkIdList(
            List<BookmarkId> bookmarkIdList, long id, @BookmarkType int type) {
        bookmarkIdList.add(new BookmarkId(id, type));
    }

    @CalledByNative
    private static void addToBookmarkIdListWithDepth(
            List<BookmarkId> folderList,
            long id,
            @BookmarkType int type,
            List<Integer> depthList,
            int depth) {
        folderList.add(new BookmarkId(id, type));
        depthList.add(depth);
    }

    private static List<Pair<Integer, Integer>> createPairsList(int[] left, int[] right) {
        List<Pair<Integer, Integer>> pairList = new ArrayList<>();
        for (int i = 0; i < left.length; i++) {
            pairList.add(new Pair<>(left[i], right[i]));
        }
        return pairList;
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    @NativeMethods
    public interface Natives {
        BookmarkModel getForProfile(Profile profile);

        void getImageUrlForBookmark(long nativeBookmarkBridge, GURL url, Callback<GURL> callback);

        BookmarkId getBookmarkIdForWebContents(
                long nativeBookmarkBridge, WebContents webContents, boolean onlyEditable);

        BookmarkItem getBookmarkById(long nativeBookmarkBridge, long id, int type);

        void getTopLevelFolderIds(long nativeBookmarkBridge, List<BookmarkId> bookmarksList);

        BookmarkId getReadingListFolder(long nativeBookmarkBridge);

        void getAllFoldersWithDepths(
                long nativeBookmarkBridge, List<BookmarkId> folderList, List<Integer> depthList);

        BookmarkId getRootFolderId(long nativeBookmarkBridge);

        BookmarkId getMobileFolderId(long nativeBookmarkBridge);

        BookmarkId getOtherFolderId(long nativeBookmarkBridge);

        BookmarkId getDesktopFolderId(long nativeBookmarkBridge);

        BookmarkId getPartnerFolderId(long nativeBookmarkBridge);

        String getBookmarkGuidByIdForTesting( // IN-TEST
                long nativeBookmarkBridge, long id, int type);

        int getChildCount(long nativeBookmarkBridge, long id, int type);

        void getChildIds(
                long nativeBookmarkBridge, long id, int type, List<BookmarkId> bookmarksList);

        BookmarkId getChildAt(long nativeBookmarkBridge, long id, int type, int index);

        int getTotalBookmarkCount(long nativeBookmarkBridge, long id, int type);

        void setBookmarkTitle(long nativeBookmarkBridge, long id, int type, String title);

        void setBookmarkUrl(long nativeBookmarkBridge, long id, int type, GURL url);

        byte[] getPowerBookmarkMeta(long nativeBookmarkBridge, long id, int type);

        void setPowerBookmarkMeta(long nativeBookmarkBridge, long id, int type, byte[] meta);

        void deletePowerBookmarkMeta(long nativeBookmarkBridge, long id, int type);

        boolean doesBookmarkExist(long nativeBookmarkBridge, long id, int type);

        void getBookmarksForFolder(
                long nativeBookmarkBridge, BookmarkId folderId, List<BookmarkItem> bookmarksList);

        boolean isFolderVisible(long nativeBookmarkBridge, long id, int type);

        BookmarkId addFolder(long nativeBookmarkBridge, BookmarkId parent, int index, String title);

        void deleteBookmark(long nativeBookmarkBridge, BookmarkId bookmarkId);

        void removeAllUserBookmarks(long nativeBookmarkBridge);

        void moveBookmark(
                long nativeBookmarkBridge,
                BookmarkId bookmarkId,
                BookmarkId newParentId,
                int index);

        BookmarkId addBookmark(
                long nativeBookmarkBridge, BookmarkId parent, int index, String title, GURL url);

        BookmarkId addToReadingList(long nativeBookmarkBridge, String title, GURL url);

        BookmarkItem getReadingListItem(long nativeBookmarkBridge, GURL url);

        void setReadStatus(long nativeBookmarkBridge, GURL url, boolean read);

        void undo(long nativeBookmarkBridge);

        void startGroupingUndos(long nativeBookmarkBridge);

        void endGroupingUndos(long nativeBookmarkBridge);

        void loadEmptyPartnerBookmarkShimForTesting(long nativeBookmarkBridge); // IN-TEST

        void loadFakePartnerBookmarkShimForTesting(long nativeBookmarkBridge); // IN-TEST

        void searchBookmarks(
                long nativeBookmarkBridge,
                List<BookmarkId> bookmarkMatches,
                String query,
                String[] tags,
                int powerBookmarkType,
                int maxNumber);

        void getBookmarksOfType(
                long nativeBookmarkBridge, List<BookmarkId> bookmarkMatches, int powerBookmarkType);

        boolean isDoingExtensiveChanges(long nativeBookmarkBridge);

        void destroy(long nativeBookmarkBridge);

        boolean isEditBookmarksEnabled(long nativeBookmarkBridge);

        void reorderChildren(long nativeBookmarkBridge, BookmarkId parent, long[] orderedNodes);

        boolean isBookmarked(long nativeBookmarkBridge, GURL url);

        int getUnreadCount(long nativeBookmarkBridge);
    }
}
