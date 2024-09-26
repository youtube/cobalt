// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.app.Activity;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.graphics.drawable.Drawable;
import android.net.Uri;
import android.os.Handler;
import android.os.LocaleList;
import android.provider.Browser;
import android.text.TextUtils;

import androidx.annotation.ColorRes;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.BuildInfo;
import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityUtils;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.app.bookmarks.BookmarkActivity;
import org.chromium.chrome.browser.app.bookmarks.BookmarkAddEditFolderActivity;
import org.chromium.chrome.browser.app.bookmarks.BookmarkEditActivity;
import org.chromium.chrome.browser.app.bookmarks.BookmarkFolderSelectActivity;
import org.chromium.chrome.browser.commerce.ShoppingServiceFactory;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.incognito.IncognitoUtils;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.read_later.ReadingListUtils;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager.SnackbarController;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkItem;
import org.chromium.components.bookmarks.BookmarkType;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.commerce.core.ShoppingService;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.profile_metrics.BrowserProfileType;
import org.chromium.ui.UiUtils;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.url.GURL;

import java.text.DateFormat;
import java.util.ArrayList;
import java.util.Date;
import java.util.List;
import java.util.Locale;
import java.util.concurrent.TimeUnit;

/** A class holding static util functions for bookmark. */
public class BookmarkUtils {
    private static final String TAG = "BookmarkUtils";
    private static final int READING_LIST_SESSION_LENGTH_MS = (int) TimeUnit.HOURS.toMillis(1);

    /**
     * If the tab has already been bookmarked, start {@link BookmarkEditActivity} for the
     * normal bookmark or show the reading list page for reading list bookmark.
     * If not, add the bookmark to {@link BookmarkModel}, and show a snackbar notifying the user.
     *
     * @param existingBookmarkItem The {@link BookmarkItem} if the tab has already been bookmarked.
     * @param bookmarkModel The bookmark model.
     * @param tab The tab to add or edit a bookmark.
     * @param snackbarManager The {@link SnackbarManager} used to show the snackbar.
     * @param bottomSheetController The {@link BottomSheetController} used to show the bottom sheet.
     * @param activity Current activity.
     * @param fromCustomTab boolean indicates whether it is called by Custom Tab.
     * @param bookmarkType Type of the added bookmark.
     * @param callback Invoked with the resulting bookmark ID, which could be null if unsuccessful.
     * @param fromExplicitTrackUi Whether the bookmark was added directly from a tracking ui (e.g.
     *         the shopping "track price" button).
     */
    public static void addOrEditBookmark(@Nullable BookmarkItem existingBookmarkItem,
            BookmarkModel bookmarkModel, Tab tab, SnackbarManager snackbarManager,
            BottomSheetController bottomSheetController, Activity activity, boolean fromCustomTab,
            @BookmarkType int bookmarkType, Callback<BookmarkId> callback,
            boolean fromExplicitTrackUi) {
        assert bookmarkModel.isBookmarkModelLoaded();
        if (existingBookmarkItem != null) {
            startEditActivity(activity, existingBookmarkItem.getId());
            callback.onResult(existingBookmarkItem.getId());
            return;
        }

        BookmarkId newBookmarkId = addBookmarkInternal(activity, bookmarkModel, tab.getTitle(),
                tab.getOriginalUrl(),
                fromExplicitTrackUi ? bookmarkModel.getMobileFolderId() : null, bookmarkType);
        showSaveFlow(activity, bottomSheetController, fromExplicitTrackUi, newBookmarkId,
                /*wasBookmarkMoved=*/false, /*isNewBookmark=*/true);
        callback.onResult(newBookmarkId);
    }

    /**
     * Shows the bookmark save flow.
     *
     * @param activity The current Activity.
     * @param bottomSheetController The BottomsheetController, used to show the save flow.
     * @param fromExplicitTrackUi Whether the bookmark was added from the explicit UI.
     * @param bookmarkId The BookmarkId to show the save flow for. Can be null in some cases.
     * @param wasBookmarkMoved Whether the save flow is shown as a reslult of a moved bookmark.
     * @param isNewBookmark Whether the bookmark is newly created.
     */
    public static void showSaveFlow(@NonNull Activity activity,
            @NonNull BottomSheetController bottomSheetController, boolean fromExplicitTrackUi,
            @Nullable BookmarkId bookmarkId, boolean wasBookmarkMoved, boolean isNewBookmark) {
        if (bookmarkId == null) {
            Log.e(TAG, "Null bookmark found when showing the save flow, aborting.");
            return;
        }

        Profile profile = Profile.getLastUsedRegularProfile();
        ShoppingService shoppingService = ShoppingServiceFactory.getForProfile(profile);

        BookmarkSaveFlowCoordinator bookmarkSaveFlowCoordinator =
                new BookmarkSaveFlowCoordinator(activity, bottomSheetController, shoppingService,
                        new UserEducationHelper(activity, new Handler()), profile);
        bookmarkSaveFlowCoordinator.show(
                bookmarkId, fromExplicitTrackUi, wasBookmarkMoved, isNewBookmark);
    }

    // The legacy code path to add or edit bookmark without triggering the bookmark bottom sheet.
    private static BookmarkId addBookmarkAndShowSnackbar(BookmarkModel bookmarkModel, Tab tab,
            SnackbarManager snackbarManager, Activity activity, boolean fromCustomTab,
            @BookmarkType int bookmarkType) {
        if (bookmarkType == BookmarkType.READING_LIST) {
            return addToReadingList(
                    tab.getOriginalUrl(), tab.getTitle(), snackbarManager, bookmarkModel, activity);
        }
        BookmarkId bookmarkId = addBookmarkInternal(activity, bookmarkModel, tab.getTitle(),
                tab.getOriginalUrl(), /*parent=*/null, BookmarkType.NORMAL);

        if (bookmarkId != null && bookmarkId.getType() == BookmarkType.NORMAL) {
            @BrowserProfileType
            int type = Profile.getBrowserProfileTypeFromProfile(
                    IncognitoUtils.getProfileFromWindowAndroid(
                            tab.getWindowAndroid(), tab.isIncognito()));
            RecordHistogram.recordEnumeratedHistogram(
                    "Bookmarks.AddedPerProfileType", type, BrowserProfileType.MAX_VALUE + 1);
        }

        Snackbar snackbar = null;
        if (bookmarkId == null) {
            snackbar = Snackbar.make(activity.getString(R.string.bookmark_page_failed),
                    new SnackbarController() {
                        @Override
                        public void onDismissNoAction(Object actionData) { }

                        @Override
                        public void onAction(Object actionData) { }
                    }, Snackbar.TYPE_NOTIFICATION, Snackbar.UMA_BOOKMARK_ADDED)
                    .setSingleLine(false);
            RecordUserAction.record("EnhancedBookmarks.AddingFailed");
        } else {
            String folderName = bookmarkModel.getBookmarkTitle(
                    bookmarkModel.getBookmarkById(bookmarkId).getParentId());
            SnackbarController snackbarController =
                    createSnackbarControllerForEditButton(activity, bookmarkId);
            if (getLastUsedParent(activity, bookmarkModel) == null) {
                if (fromCustomTab) {
                    String packageLabel = BuildInfo.getInstance().hostPackageLabel;
                    snackbar = Snackbar.make(
                            activity.getString(R.string.bookmark_page_saved, packageLabel),
                            snackbarController, Snackbar.TYPE_ACTION, Snackbar.UMA_BOOKMARK_ADDED);
                } else {
                    snackbar = Snackbar.make(
                            activity.getString(R.string.bookmark_page_saved_default),
                            snackbarController, Snackbar.TYPE_ACTION, Snackbar.UMA_BOOKMARK_ADDED);
                }
            } else {
                snackbar = Snackbar.make(folderName, snackbarController, Snackbar.TYPE_ACTION,
                        Snackbar.UMA_BOOKMARK_ADDED)
                        .setTemplateText(activity.getString(R.string.bookmark_page_saved_folder));
            }
            snackbar.setSingleLine(false).setAction(activity.getString(R.string.bookmark_item_edit),
                    null);
        }
        snackbarManager.showSnackbar(snackbar);
        return bookmarkId;
    }

    /**
     * Add an article to the reading list. If the article was already loaded, the entry will be
     * overwritten. After successful addition, a snackbar will be shown notifying the user about the
     * result of the operation.
     * @param url The associated URL.
     * @param title The title of the reading list item being added.
     * @param snackbarManager The snackbar manager that will be used to show a snackbar.
     * @param bookmarkBridge The bookmark bridge that talks to the bookmark backend.
     * @param context The associated context.
     * @return The bookmark ID created after saving the article to the reading list.
     */
    public static BookmarkId addToReadingList(GURL url, String title,
            SnackbarManager snackbarManager, BookmarkBridge bookmarkBridge, Context context) {
        assert bookmarkBridge.isBookmarkModelLoaded();
        BookmarkId bookmarkId = bookmarkBridge.addToReadingList(title, url);

        if (bookmarkId != null) {
            Snackbar snackbar = Snackbar.make(context.getString(R.string.reading_list_saved),
                    new SnackbarController() {}, Snackbar.TYPE_ACTION,
                    Snackbar.UMA_READING_LIST_BOOKMARK_ADDED);
            snackbarManager.showSnackbar(snackbar);

            TrackerFactory.getTrackerForProfile(Profile.getLastUsedRegularProfile())
                    .notifyEvent(EventConstants.READ_LATER_ARTICLE_SAVED);
        }
        return bookmarkId;
    }

    /**
     * Add all selected tabs from TabSelectionEditorV2 as bookmarks. This logic depends on the
     * snackbar workflow above. Currently there is no support for adding the selected tabs or newly
     * created folder directly to the reading list.
     * @param activity The current activity.
     * @param bookmarkModel The bookmark model.
     * @param tabList The list of all currently selected tabs from the TabSelectionEditor menu.
     * @param snackbarManager The SnackbarManager used to show the snackbar.
     */
    public static void addBookmarksOnMultiSelect(Activity activity,
            @NonNull BookmarkModel bookmarkModel, @NonNull List<Tab> tabList,
            @NonNull SnackbarManager snackbarManager) {
        // TODO(crbug.com/1385914): Refactor the bookmark folder select activity to allow for the
        // view to display in a dialog implementation approach.
        assert bookmarkModel != null;

        // For a single selected bookmark, default to the single tab-to-bookmark approach.
        if (tabList.size() == 1) {
            addBookmarkAndShowSnackbar(bookmarkModel, tabList.get(0), snackbarManager, activity,
                    false, BookmarkType.NORMAL);
            return;
        }

        // Current date time format with an example would be: Nov 17, 2022 4:34:20 PM PST
        DateFormat dateFormat = DateFormat.getDateTimeInstance(
                DateFormat.MEDIUM, DateFormat.LONG, getLocale(activity));
        String fileName =
                activity.getString(R.string.tab_selection_editor_add_bookmarks_folder_name,
                        dateFormat.format(new Date(System.currentTimeMillis())));
        BookmarkId newFolder =
                bookmarkModel.addFolder(bookmarkModel.getDefaultFolder(), 0, fileName);
        int tabsBookmarkedCount = 0;

        for (Tab tab : tabList) {
            BookmarkId tabToBookmark = addBookmarkInternal(activity, bookmarkModel, tab.getTitle(),
                    tab.getOriginalUrl(), newFolder, BookmarkType.NORMAL);

            if (bookmarkModel.doesBookmarkExist(tabToBookmark)) {
                tabsBookmarkedCount++;
            }
        }
        RecordHistogram.recordCount100Histogram(
                "Android.TabMultiSelectV2.BookmarkTabsCount", tabsBookmarkedCount);

        SnackbarController snackbarController =
                createSnackbarControllerForBookmarkFolderEditButton(activity, newFolder);
        Snackbar snackbar = Snackbar.make(activity.getString(R.string.bookmark_page_saved_default),
                snackbarController, Snackbar.TYPE_ACTION, Snackbar.UMA_BOOKMARK_ADDED);
        snackbar.setSingleLine(false).setAction(
                activity.getString(R.string.bookmark_item_edit), null);
        snackbarManager.showSnackbar(snackbar);
    }

    /**
     * Adds a bookmark with the given {@link Tab}. This will reset last used parent if it fails to
     * add a bookmark.
     *
     * @param context The current Android {@link Context}.
     * @param bookmarkModel The current {@link BookmarkModel} which talks to native.
     * @param bookmarkType The {@link BookmarkType} of the bookmark.
     */
    static BookmarkId addBookmarkInternal(Context context, BookmarkModel bookmarkModel,
            String title, GURL url, @Nullable BookmarkId parent, @BookmarkType int bookmarkType) {
        parent = parent == null ? getLastUsedParent(context, bookmarkModel) : parent;
        BookmarkItem parentItem = null;
        if (parent != null) {
            parentItem = bookmarkModel.getBookmarkById(parent);
        }
        if (parent == null || parentItem == null || parentItem.isManaged()
                || !parentItem.isFolder()) {
            parent = bookmarkModel.getDefaultFolder();
        }

        // Reading list items will be added when either one of the 2 conditions is met:
        // 1. The bookmark type explicitly specifies READING_LIST.
        // 2. The last used parent implicitly specifies READING_LIST.
        if (bookmarkType == BookmarkType.READING_LIST
                || parent.getType() == BookmarkType.READING_LIST) {
            return bookmarkModel.addToReadingList(title, url);
        }

        BookmarkId bookmarkId = null;
        // Use "New tab" as title for both incognito and regular NTP.
        if (url.getSpec().equals(UrlConstants.NTP_URL)) {
            title = context.getResources().getString(R.string.new_tab_title);
        }

        bookmarkId =
                bookmarkModel.addBookmark(parent, bookmarkModel.getChildCount(parent), title, url);
        // TODO(lazzzis): remove log after bookmark sync is fixed, crbug.com/986978
        if (bookmarkId == null) {
            Log.e(TAG,
                    "Failed to add bookmarks: parentTypeAndId %s, defaultFolderTypeAndId %s, "
                            + "mobileFolderTypeAndId %s, parentEditable Managed isFolder %s,",
                    parent, bookmarkModel.getDefaultFolder(), bookmarkModel.getMobileFolderId(),
                    parentItem == null ? "null"
                                       : (parentItem.isEditable() + " " + parentItem.isManaged()
                                               + " " + parentItem.isFolder()));
            setLastUsedParent(context, bookmarkModel.getDefaultFolder());
        }
        return bookmarkId;
    }

    /**
     * Creates a snackbar controller for a case where "Edit" button is shown to edit the newly
     * created bookmark.
     */
    private static SnackbarController createSnackbarControllerForEditButton(
            final Activity activity, final BookmarkId bookmarkId) {
        return new SnackbarController() {
            @Override
            public void onDismissNoAction(Object actionData) {
                RecordUserAction.record("EnhancedBookmarks.EditAfterCreateButtonNotClicked");
            }

            @Override
            public void onAction(Object actionData) {
                RecordUserAction.record("EnhancedBookmarks.EditAfterCreateButtonClicked");
                startEditActivity(activity, bookmarkId);
            }
        };
    }

    /**
     * Creates a snackbar controller for a case where "Edit" button is shown to edit a newly
     * created bookmarks folder with bulk added bookmarks
     */
    private static SnackbarController createSnackbarControllerForBookmarkFolderEditButton(
            Context context, BookmarkId folder) {
        return new SnackbarController() {
            @Override
            public void onDismissNoAction(Object actionData) {
                RecordUserAction.record("TabMultiSelectV2.BookmarkTabsSnackbarEditNotClicked");
            }

            @Override
            public void onAction(Object actionData) {
                RecordUserAction.record("TabMultiSelectV2.BookmarkTabsSnackbarEditClicked");
                BookmarkAddEditFolderActivity.startEditFolderActivity(context, folder);
            }
        };
    }

    /**
     * Shows bookmark main UI.
     * @param activity An activity to start the manager with.
     * @param isIncognito Whether the bookmark manager is opened in incognito mode.
     */
    public static void showBookmarkManager(Activity activity, boolean isIncognito) {
        showBookmarkManager(activity, null, isIncognito);
    }

    /**
     * Shows bookmark main UI.
     * @param activity An activity to start the manager with. If null, the bookmark manager will be
     *         started as a new task.
     * @param folderId The bookmark folder to open. If null, the bookmark manager will open the most
     *         recent folder.
     * @param isIncognito Whether the bookmark UI is opened in incognito mode.
     */
    public static void showBookmarkManager(
            @Nullable Activity activity, @Nullable BookmarkId folderId, boolean isIncognito) {
        ThreadUtils.assertOnUiThread();
        Context context = activity == null ? ContextUtils.getApplicationContext() : activity;
        String url = getFirstUrlToLoad(context, folderId);

        if (SharedPreferencesManager.getInstance().contains(
                    ChromePreferenceKeys.BOOKMARKS_LAST_USED_URL)) {
            RecordUserAction.record("MobileBookmarkManagerReopenBookmarksInSameSession");
        }

        if (DeviceFormFactor.isNonMultiDisplayContextOnTablet(context)) {
            showBookmarkManagerOnTablet(context,
                    activity == null ? null : activity.getComponentName(), url, isIncognito);
        } else {
            showBookmarkManagerOnPhone(activity, url, isIncognito);
        }
    }

    private static void showBookmarkManagerOnPhone(
            Activity activity, String url, boolean isIncognito) {
        Intent intent =
                new Intent(activity == null ? ContextUtils.getApplicationContext() : activity,
                        BookmarkActivity.class);
        intent.putExtra(IntentHandler.EXTRA_INCOGNITO_MODE, isIncognito);
        intent.setData(Uri.parse(url));
        if (activity != null) {
            // Start from an existing activity.
            intent.putExtra(IntentHandler.EXTRA_PARENT_COMPONENT, activity.getComponentName());
            activity.startActivity(intent);
        } else {
            // Start a new task.
            intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            IntentHandler.startActivityForTrustedIntent(intent);
        }
    }

    private static void showBookmarkManagerOnTablet(Context context,
            @Nullable ComponentName componentName, String url, boolean isIncognito) {
        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse(url));
        intent.putExtra(IntentHandler.EXTRA_INCOGNITO_MODE, isIncognito);
        intent.putExtra(
                Browser.EXTRA_APPLICATION_ID, context.getApplicationContext().getPackageName());
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

        if (componentName != null) {
            ActivityUtils.setNonAliasedComponentForMainBrowsingActivity(intent, componentName);
        } else {
            // If the bookmark manager is shown in a tab on a phone (rather than in a separate
            // activity) the component name may be null. Send the intent through
            // ChromeLauncherActivity instead to avoid crashing. See crbug.com/615012.
            intent.setClass(context.getApplicationContext(), ChromeLauncherActivity.class);
        }

        IntentHandler.startActivityForTrustedIntent(intent);
    }

    /**
     * @return the bookmark folder URL to open.
     */
    private static String getFirstUrlToLoad(Context context, @Nullable BookmarkId folderId) {
        String url;
        if (folderId == null) {
            // Load most recently visited bookmark folder.
            url = getLastUsedUrl(context);
        } else {
            // Load a specific folder.
            url = BookmarkUiState.createFolderUrl(folderId).toString();
        }

        return TextUtils.isEmpty(url) ? UrlConstants.BOOKMARKS_URL : url;
    }

    /**
     * Saves the last used url to preference. The saved url will be later queried by
     * {@link #getLastUsedUrl(Context)}
     */
    static void setLastUsedUrl(Context context, String url) {
        SharedPreferencesManager.getInstance().writeString(
                ChromePreferenceKeys.BOOKMARKS_LAST_USED_URL, url);
    }

    /**
     * Fetches url representing the user's state last time they close the bookmark manager.
     */
    @VisibleForTesting
    public static String getLastUsedUrl(Context context) {
        return SharedPreferencesManager.getInstance().readString(
                ChromePreferenceKeys.BOOKMARKS_LAST_USED_URL, UrlConstants.BOOKMARKS_URL);
    }

    /**
     * Save the last used {@link BookmarkId} as a folder to put new bookmarks to.
     */
    public static void setLastUsedParent(Context context, BookmarkId bookmarkId) {
        SharedPreferencesManager.getInstance().writeString(
                ChromePreferenceKeys.BOOKMARKS_LAST_USED_PARENT, bookmarkId.toString());
    }

    /**
     * @param context The current android {@link Context}.
     * @param bookmarkModel The bookmark model used to reset the last used parent for type swapping
     *         edge cases.
     * @return The parent {@link BookmarkId} that the user used the last time or null if the user
     *         has never selected a parent folder to use.
     */
    static BookmarkId getLastUsedParent(Context context, BookmarkModel bookmarkModel) {
        SharedPreferencesManager preferences = SharedPreferencesManager.getInstance();
        if (!preferences.contains(ChromePreferenceKeys.BOOKMARKS_LAST_USED_PARENT)) return null;

        BookmarkId parent = BookmarkId.getBookmarkIdFromString(
                preferences.readString(ChromePreferenceKeys.BOOKMARKS_LAST_USED_PARENT, null));

        return parent;
    }

    /** Starts an {@link BookmarkEditActivity} for the given {@link BookmarkId}. */
    public static void startEditActivity(Context context, BookmarkId bookmarkId) {
        RecordUserAction.record("MobileBookmarkManagerEditBookmark");
        Intent intent = new Intent(context, BookmarkEditActivity.class);
        intent.putExtra(BookmarkEditActivity.INTENT_BOOKMARK_ID, bookmarkId.toString());
        if (context instanceof BookmarkActivity) {
            ((BookmarkActivity) context).startActivityForResult(
                    intent, BookmarkActivity.EDIT_BOOKMARK_REQUEST_CODE);
        } else {
            context.startActivity(intent);
        }
    }

    /** Starts an {@link BookmarkFolderSelectActivity} for the given {@link BookmarkId}. */
    public static void startFolderSelectActivity(Context context, BookmarkId bookmarkId) {
        BookmarkFolderSelectActivity.startFolderSelectActivity(context, bookmarkId);
    }

    /**
     * @param context {@link Context} used to retrieve the drawable.
     * @param type The bookmark type of the folder.
     * @return A {@link Drawable} to use for displaying bookmark folders.
     */
    public static Drawable getFolderIcon(Context context, @BookmarkType int type) {
        if (type == BookmarkType.READING_LIST) {
            return UiUtils.getTintedDrawable(
                    context, R.drawable.ic_reading_list_folder_24dp, getFolderIconTint(type));
        }
        return UiUtils.getTintedDrawable(
                context, R.drawable.ic_folder_blue_24dp, getFolderIconTint(type));
    }

    /**
     * @param type The bookmark type.
     * @return The tint used on the bookmark folder icon.
     */
    public static @ColorRes int getFolderIconTint(@BookmarkType int type) {
        return R.color.default_icon_color_tint_list;
    }

    /**
     * Retrieve the save flow start icon for the given bookmark.
     *
     * @param bookmarkId The {@link BookmarkId} to get the start icon for.
     * @return The start icon associated with the given bookmarkId.
     */
    public static Drawable getSaveFlowStartIconForBookmark(BookmarkId bookmarkId) {
        // TODO(crbug.com/1243383): Add start icon for price tracking.
        return null;
    }

    /**
     * Closes the {@link BookmarkActivity} on Phone. Does nothing on tablet.
     */
    public static void finishActivityOnPhone(Context context) {
        if (context instanceof BookmarkActivity) {
            ((Activity) context).finish();
        }
    }

    /**
     * Populates the top level bookmark folder ids.
     * @param bookmarkModel The bookmark model that talks to bookmark native backend.
     * @return The list of top level bookmark folder ids.
     */
    public static List<BookmarkId> populateTopLevelFolders(BookmarkModel bookmarkModel) {
        List<BookmarkId> topLevelFolders = new ArrayList<>();
        BookmarkId desktopNodeId = bookmarkModel.getDesktopFolderId();
        BookmarkId mobileNodeId = bookmarkModel.getMobileFolderId();
        BookmarkId othersNodeId = bookmarkModel.getOtherFolderId();

        List<BookmarkId> specialFoldersIds =
                bookmarkModel.getTopLevelFolderIds(/*getSpecial=*/true, /*getNormal=*/false);
        BookmarkId rootFolder = bookmarkModel.getRootFolderId();

        // managed and partner bookmark folders will be put to the bottom.
        List<BookmarkId> managedAndPartnerFolderIds = new ArrayList<>();

        for (BookmarkId bookmarkId : specialFoldersIds) {
            // Adds reading list as the first top level folder.
            if (bookmarkId.getType() == BookmarkType.READING_LIST) {
                topLevelFolders.add(bookmarkId);
                TrackerFactory.getTrackerForProfile(Profile.getLastUsedRegularProfile())
                        .notifyEvent(EventConstants.READ_LATER_BOTTOM_SHEET_FOLDER_SEEN);
                continue;
            }
            BookmarkId parent = bookmarkModel.getBookmarkById(bookmarkId).getParentId();
            if (parent.equals(rootFolder)) managedAndPartnerFolderIds.add(bookmarkId);
        }

        // Adds normal bookmark top level folders.
        if (bookmarkModel.isFolderVisible(mobileNodeId)) {
            topLevelFolders.add(mobileNodeId);
        }
        if (bookmarkModel.isFolderVisible(desktopNodeId)) {
            topLevelFolders.add(desktopNodeId);
        }
        if (bookmarkModel.isFolderVisible(othersNodeId)) {
            topLevelFolders.add(othersNodeId);
        }

        // Add any top-level managed and partner bookmark folders that are children of the root
        // folder.
        topLevelFolders.addAll(managedAndPartnerFolderIds);
        return topLevelFolders;
    }

    /**
     * Expires the stored last used url if Chrome has been in the background long enough to mark it
     * as a new session. We're using the "Start Surface" concept of session here which is if the
     * app has been in the background for X amount of time. Called from #onStartWithNative, after
     * which the time stored in {@link ChromeInactivityTracker} is expired.
     *
     * @param timeSinceLastBackgroundedMs The time since Chrome has sent into the background.
     */
    public static void maybeExpireLastBookmarkLocationForReadLater(
            long timeSinceLastBackgroundedMs) {
        if (timeSinceLastBackgroundedMs > READING_LIST_SESSION_LENGTH_MS) {
            SharedPreferencesManager.getInstance().removeKey(
                    ChromePreferenceKeys.BOOKMARKS_LAST_USED_URL);
        }
    }

    /** Returns whether this bookmark can be moved */
    public static boolean isMovable(BookmarkItem node) {
        return ReadingListUtils.isSwappableReadingListItem(node.getId()) || node.isReorderable();
    }

    /** Allows strings to be landed for translation. */
    private void fakeFunctiontoAllowStringMerge() {
        int id = R.string.price_tracking_save_flow_notification_switch_subtitle_error;
        id = R.string.disable_price_tracking_menu_item;
        id = R.string.price_tracking_bookmarks_filter_title;
        id = R.string.tracked_products_empty_list_title;
        id = R.string.price_tracking_disabled_snackbar;
        id = R.string.price_tracking_enabled_snackbar;
        id = R.string.price_tracking_error_snackbar;
        id = R.string.price_tracking_error_snackbar_action;
        id = R.string.iph_price_tracking_menu_item;
        id = R.string.iph_price_tracking_menu_item_accessibility;
        id = R.string.iph_shopping_list_save_flow;
        id = R.string.iph_shopping_list_save_flow_accessibility;
    }

    private static Locale getLocale(Activity activity) {
        LocaleList locales = activity.getResources().getConfiguration().getLocales();
        if (locales.size() > 0) {
            return locales.get(0);
        }
        @SuppressWarnings("deprecation")
        Locale locale = activity.getResources().getConfiguration().locale;
        return locale;
    }
}
