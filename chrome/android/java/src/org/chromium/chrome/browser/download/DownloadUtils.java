// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import android.app.Activity;
import android.app.DownloadManager;
import android.content.ActivityNotFoundException;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.graphics.Typeface;
import android.net.Uri;
import android.text.Spannable;
import android.text.SpannableString;
import android.text.TextUtils;
import android.text.style.ClickableSpan;
import android.text.style.StyleSpan;

import androidx.annotation.MainThread;
import androidx.annotation.Nullable;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.ContentUriUtils;
import org.chromium.base.ContextUtils;
import org.chromium.base.FileUtils;
import org.chromium.base.IntentUtils;
import org.chromium.base.Log;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.app.download.home.DownloadActivity;
import org.chromium.chrome.browser.download.items.OfflineContentAggregatorFactory;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.media.MediaViewerUtils;
import org.chromium.chrome.browser.offlinepages.DownloadUiActionFlags;
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge;
import org.chromium.chrome.browser.offlinepages.OfflinePageOrigin;
import org.chromium.chrome.browser.offlinepages.OfflinePageUtils;
import org.chromium.chrome.browser.offlinepages.downloads.OfflinePageDownloadBridge;
import org.chromium.chrome.browser.profiles.OTRProfileID;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileKey;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.document.TabDelegate;
import org.chromium.chrome.browser.util.ChromeAccessibilityUtil;
import org.chromium.components.download.DownloadState;
import org.chromium.components.download.ResumeMode;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.offline_items_collection.ContentId;
import org.chromium.components.offline_items_collection.FailState;
import org.chromium.components.offline_items_collection.LaunchLocation;
import org.chromium.components.offline_items_collection.LegacyHelpers;
import org.chromium.components.offline_items_collection.OfflineItem;
import org.chromium.components.offline_items_collection.OpenParams;
import org.chromium.content_public.browser.BrowserStartupController;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.widget.Toast;
import org.chromium.url.GURL;

import java.io.File;

/**
 * A class containing some utility static methods.
 */
public class DownloadUtils {
    private static final String TAG = "download";

    private static final String EXTRA_OTR_PROFILE_ID =
            "org.chromium.chrome.browser.download.OTR_PROFILE_ID";
    private static final String MIME_TYPE_ZIP = "application/zip";
    private static final String DOCUMENTS_UI_PACKAGE_NAME = "com.android.documentsui";
    public static final String EXTRA_SHOW_PREFETCHED_CONTENT =
            "org.chromium.chrome.browser.download.SHOW_PREFETCHED_CONTENT";

    /**
     * Displays the download manager UI. Note the UI is different on tablets and on phones.
     * @param activity The current activity is available.
     * @param tab The current tab if it exists.
     * @param otrProfileID The {@link OTRProfileID} to determine whether download home should be
     * opened in incognito mode. If null, download page will be opened in normal profile.
     * @param source The source where the user action is coming from.
     * @return Whether the UI was shown.
     */
    public static boolean showDownloadManager(@Nullable Activity activity, @Nullable Tab tab,
            @Nullable OTRProfileID otrProfileID, @DownloadOpenSource int source) {
        return showDownloadManager(activity, tab, otrProfileID, source, false);
    }

    /**
     * Displays the download manager UI. Note the UI is different on tablets and on phones.
     * @param activity The current activity is available.
     * @param tab The current tab if it exists.
     * @param otrProfileID The {@link OTRProfileID} to determine whether download home should be
     * opened in incognito mode. Only used when no valid current or recent tab presents.
     * @param source The source where the user action is coming from.
     * @param showPrefetchedContent Whether the manager should start with prefetched content section
     * expanded.
     * @return Whether the UI was shown.
     */
    @CalledByNative
    public static boolean showDownloadManager(@Nullable Activity activity, @Nullable Tab tab,
            @Nullable OTRProfileID otrProfileID, @DownloadOpenSource int source,
            boolean showPrefetchedContent) {
        // Figure out what tab was last being viewed by the user.
        if (activity == null) activity = ApplicationStatus.getLastTrackedFocusedActivity();
        Context appContext = ContextUtils.getApplicationContext();
        boolean isTablet;

        if (tab == null && activity instanceof ChromeTabbedActivity) {
            ChromeTabbedActivity chromeActivity = ((ChromeTabbedActivity) activity);
            tab = chromeActivity.getActivityTab();
            isTablet = chromeActivity.isTablet();
        } else {
            Context displayContext = activity != null ? activity : appContext;
            isTablet = DeviceFormFactor.isNonMultiDisplayContextOnTablet(displayContext);
        }

        // Use tab's profile if a valid tab exists.
        if (otrProfileID == null && tab != null) {
            Profile profile = Profile.fromWebContents(tab.getWebContents());
            otrProfileID = profile != null ? profile.getOTRProfileID() : otrProfileID;
        }

        // If the profile is off-the-record and it does not exist, then do not start the activity.
        if (OTRProfileID.isOffTheRecord(otrProfileID)
                && !Profile.getLastUsedRegularProfile().hasOffTheRecordProfile(otrProfileID)) {
            return false;
        }

        if (isTablet) {
            // Download Home shows up as a tab on tablets.
            LoadUrlParams params = new LoadUrlParams(UrlConstants.DOWNLOADS_URL);
            if (tab == null || !tab.isInitialized()) {
                // Open a new tab, which pops Chrome into the foreground.
                TabDelegate delegate = new TabDelegate(false);
                delegate.createNewTab(params, TabLaunchType.FROM_CHROME_UI, null);
            } else {
                // Download Home shows up inside an existing tab, but only if the last Activity was
                // the ChromeTabbedActivity.
                tab.loadUrl(params);

                // Bring Chrome to the foreground, if possible. Unless Chrome is already in the
                // foreground, this request is most likely coming from a notification.
                Intent intent = IntentHandler.createTrustedBringTabToFrontIntent(
                        tab.getId(), IntentHandler.BringToFrontSource.NOTIFICATION);
                if (intent != null) {
                    intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
                    IntentUtils.safeStartActivity(appContext, intent);
                }
            }
        } else {
            // Download Home shows up as a new Activity on phones.
            Intent intent = new Intent();
            intent.setClass(appContext, DownloadActivity.class);
            intent.putExtra(EXTRA_SHOW_PREFETCHED_CONTENT, showPrefetchedContent);
            if (otrProfileID != null) {
                intent.putExtra(EXTRA_OTR_PROFILE_ID, OTRProfileID.serialize(otrProfileID));
            }

            if (activity == null) {
                // Stands alone in its own task.
                intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
                appContext.startActivity(intent);
            } else {
                // Sits on top of another Activity.
                intent.addFlags(
                        Intent.FLAG_ACTIVITY_MULTIPLE_TASK | Intent.FLAG_ACTIVITY_SINGLE_TOP);
                activity.startActivity(intent);
            }
        }

        if (BrowserStartupController.getInstance().isFullBrowserStarted()) {
            Profile profile = otrProfileID == null
                    ? Profile.getLastUsedRegularProfile()
                    : Profile.getLastUsedRegularProfile().getOffTheRecordProfile(
                            otrProfileID, /*createIfNeeded=*/true);
            Tracker tracker = TrackerFactory.getTrackerForProfile(profile);
            tracker.notifyEvent(EventConstants.DOWNLOAD_HOME_OPENED);
        }
        DownloadMetrics.recordDownloadPageOpen(source, tab);
        return true;
    }

    /**
     * @param intent An {@link Intent} instance.
     * @return The {@link OTRProfileID} that is attached to the given intent.
     */
    public static OTRProfileID getOTRProfileIDFromIntent(Intent intent) {
        String serializedId = IntentUtils.safeGetString(intent.getExtras(), EXTRA_OTR_PROFILE_ID);
        return OTRProfileID.deserialize(serializedId);
    }

    /**
     * @param intent An {@link Intent} instance.
     * @return The boolean to state whether the profile exists or not.
     */
    public static boolean doesProfileExistFromIntent(Intent intent) {
        boolean isProfileManagerInitialized = ProfileManager.isInitialized();
        if (!isProfileManagerInitialized) {
            return false;
        }

        String serializedId = IntentUtils.safeGetString(intent.getExtras(), EXTRA_OTR_PROFILE_ID);
        OTRProfileID otrProfileID = OTRProfileID.deserializeWithoutVerify(serializedId);

        return otrProfileID == null
                || Profile.getLastUsedRegularProfile().hasOffTheRecordProfile(otrProfileID);
    }

    /**
     * @return Whether or not the prefetched content section should be expanded on launch of the
     * DownloadActivity.
     */
    public static boolean shouldShowPrefetchContent(Intent intent) {
        return IntentUtils.safeGetBooleanExtra(intent, EXTRA_SHOW_PREFETCHED_CONTENT, false);
    }

    /**
     * @return Whether or not pagination headers should be shown on download home.
     */
    public static boolean shouldShowPaginationHeaders() {
        return ChromeAccessibilityUtil.get().isAccessibilityEnabled()
                || ChromeAccessibilityUtil.isHardwareKeyboardAttached(
                        ContextUtils.getApplicationContext().getResources().getConfiguration());
    }

    /**
     * Records metrics related to downloading a page. Should be called after a tap on the download
     * page button.
     * @param tab The Tab containing the page being downloaded.
     */
    public static void recordDownloadPageMetrics(Tab tab) {
        RecordHistogram.recordPercentageHistogram(
                "OfflinePages.SavePage.PercentLoaded", Math.round(tab.getProgress() * 100));
    }

    /**
     * Shows a "Downloading..." toast. Should be called after a download has been started.
     * @param context The {@link Context} used to make the toast.
     */
    public static void showDownloadStartToast(Context context) {
        Toast.makeText(context, R.string.download_started, Toast.LENGTH_SHORT).show();
    }

    /**
     * Issues a request to the {@link DownloadManagerService} associated to check for externally
     * removed downloads.
     * See {@link DownloadManagerService#checkForExternallyRemovedDownloads}.
     * @param profileKey  The {@link ProfileKey} to check downloads of the given profile.
     */
    public static void checkForExternallyRemovedDownloads(ProfileKey profileKey) {
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.DOWNLOAD_OFFLINE_CONTENT_PROVIDER)) {
            return;
        }

        if (profileKey.isOffTheRecord()) {
            DownloadManagerService.getDownloadManagerService().checkForExternallyRemovedDownloads(
                    profileKey);
        }
        DownloadManagerService.getDownloadManagerService().checkForExternallyRemovedDownloads(
                ProfileKey.getLastUsedRegularProfileKey());
        RecordUserAction.record(
                "Android.DownloadManager.CheckForExternallyRemovedItems");
    }

    /**
     * Trigger the download of an Offline Page.
     * @param context Context to pull resources from.
     */
    public static void downloadOfflinePage(Context context, Tab tab) {
        OfflinePageOrigin origin = new OfflinePageOrigin(context, tab);

        if (tab.isShowingErrorPage()) {
            // The download needs to be scheduled to happen at later time due to current network
            // error.
            final OfflinePageBridge bridge =
                    OfflinePageBridge.getForProfile(Profile.fromWebContents(tab.getWebContents()));
            bridge.scheduleDownload(tab.getWebContents(), OfflinePageBridge.ASYNC_NAMESPACE,
                    tab.getUrl().getSpec(), DownloadUiActionFlags.PROMPT_DUPLICATE, origin);
        } else {
            // Otherwise, the download can be started immediately.
            OfflinePageDownloadBridge.startDownload(tab, origin);
            DownloadUtils.recordDownloadPageMetrics(tab);
        }

        WebContents webContents = tab.getWebContents();
        if (webContents == null) return;

        Profile profile = Profile.fromWebContents(webContents);
        if (profile == null) return;
        Tracker tracker = TrackerFactory.getTrackerForProfile(profile);
        tracker.notifyEvent(EventConstants.DOWNLOAD_PAGE_STARTED);
    }

    /**
     * Whether the user should be allowed to download the current page.
     * @param tab Tab displaying the page that will be downloaded.
     * @return    Whether the "Download Page" button should be enabled.
     */
    public static boolean isAllowedToDownloadPage(Tab tab) {
        if (tab == null) return false;

        // Offline pages isn't supported in Incognito. This should be checked before calling
        // OfflinePageBridge.getForProfile because OfflinePageBridge instance will not be found
        // for incognito profile.
        if (tab.isIncognito()) return false;

        // Check if the page url is supported for saving. Only HTTP and HTTPS pages are allowed.
        if (!OfflinePageBridge.canSavePage(tab.getUrl())) return false;

        // Download will only be allowed for the error page if download button is shown in the page.
        if (tab.isShowingErrorPage()) {
            final OfflinePageBridge bridge =
                    OfflinePageBridge.getForProfile(Profile.fromWebContents(tab.getWebContents()));
            return bridge.isShowingDownloadButtonInErrorPage(tab.getWebContents());
        }

        // Don't allow re-downloading the currently displayed offline page.
        if (OfflinePageUtils.isOfflinePage(tab)) return false;

        return true;
    }

    /**
     * Returns a URI that points at the file.
     * @param filePath File path to get a URI for.
     * @return URI that points at that file, either as a content:// URI or a file:// URI.
     */
    @MainThread
    public static Uri getUriForItem(String filePath) {
        if (ContentUriUtils.isContentUri(filePath)) return Uri.parse(filePath);

        // It's ok to use blocking calls on main thread here, since the user is waiting to open or
        // share the file to other apps.
        boolean isOnSDCard = DownloadDirectoryProvider.isDownloadOnSDCard(filePath);
        if (isOnSDCard) {
            // Use custom file provider to generate content URI for download on SD card.
            return DownloadFileProvider.createContentUri(filePath);
        }
        // Use FileProvider to generate content URI or file URI.
        return FileUtils.getUriForFile(new File(filePath));
    }

    /**
     * Get the URI when shared or opened by other apps.
     *
     * @param filePath Downloaded file path.
     * @return URI for other apps to use the file via {@link android.content.ContentResolver}.
     */
    public static Uri getUriForOtherApps(String filePath) {
        return getUriForItem(filePath);
    }

    @CalledByNative
    private static String getUriStringForPath(String filePath) {
        if (ContentUriUtils.isContentUri(filePath)) return filePath;
        Uri uri = getUriForItem(filePath);
        return uri != null ? uri.toString() : new String();
    }

    /**
     * Utility method to open an {@link OfflineItem}, which can be a chrome download, offline page.
     * Falls back to open download home.
     * @param contentId The {@link ContentId} of the associated offline item.
     * @param otrProfileID The {@link OTRProfileID} of the download. Null if in regular mode.
     * @param source The location from which the download was opened.
     */
    public static void openItem(ContentId contentId, OTRProfileID otrProfileID,
            @DownloadOpenSource int source, Context context) {
        if (LegacyHelpers.isLegacyAndroidDownload(contentId)) {
            ContextUtils.getApplicationContext().startActivity(
                    new Intent(DownloadManager.ACTION_VIEW_DOWNLOADS)
                            .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK));
        } else {
            OpenParams openParams = new OpenParams(LaunchLocation.PROGRESS_BAR);
            openParams.openInIncognito = OTRProfileID.isOffTheRecord(otrProfileID);
            OfflineContentAggregatorFactory.get().openItem(openParams, contentId);
        }
    }

    /**
     * Opens a file in Chrome or in another app if appropriate.
     * @param filePath Path to the file to open, can be a content Uri.
     * @param mimeType mime type of the file.
     * @param downloadGuid The associated download GUID.
     * @param otrProfileID The {@link OTRProfileID} of the download. Null if in regular mode.
     * @param originalUrl The original url of the downloaded file.
     * @param referrer Referrer of the downloaded file.
     * @param source The source that tries to open the download file.
     * @return whether the file could successfully be opened.
     */
    public static boolean openFile(String filePath, String mimeType, String downloadGuid,
            OTRProfileID otrProfileID, String originalUrl, String referrer,
            @DownloadOpenSource int source, Context context) {
        DownloadMetrics.recordDownloadOpen(source, mimeType);
        DownloadManagerService service = DownloadManagerService.getDownloadManagerService();

        // Check if Chrome should open the file itself.
        if (service.isDownloadOpenableInBrowser(mimeType)) {
            // Share URIs use the content:// scheme when able, which looks bad when displayed
            // in the URL bar.
            Uri contentUri = getUriForItem(filePath);
            Uri fileUri = contentUri;
            if (!ContentUriUtils.isContentUri(filePath)) {
                File file = new File(filePath);
                fileUri = Uri.fromFile(file);
            }
            String normalizedMimeType = Intent.normalizeMimeType(mimeType);

            Intent intent = MediaViewerUtils.getMediaViewerIntent(fileUri /*displayUri*/,
                    contentUri /*contentUri*/, normalizedMimeType,
                    true /* allowExternalAppHandlers */, context);
            IntentHandler.startActivityForTrustedIntent(context, intent);
            service.updateLastAccessTime(downloadGuid, otrProfileID);
            return true;
        }

        // Check if any apps can open the file.
        try {
            // TODO(qinmin): Move this to an AsyncTask so we don't need to temper with strict mode.
            Uri uri = ContentUriUtils.isContentUri(filePath) ? Uri.parse(filePath)
                                                             : getUriForOtherApps(filePath);
            Intent viewIntent =
                    MediaViewerUtils.createViewIntentForUri(uri, mimeType, originalUrl, referrer);
            context.startActivity(viewIntent);
            service.updateLastAccessTime(downloadGuid, otrProfileID);
            return true;
        } catch (Exception e) {
            Log.e(TAG, "Cannot start activity to open file", e);
        }

        // If this is a zip file, check if Android Files app exists.
        if (MIME_TYPE_ZIP.equals(mimeType)) {
            try {
                PackageInfo packageInfo = context.getPackageManager().getPackageInfo(
                        DOCUMENTS_UI_PACKAGE_NAME, PackageManager.GET_ACTIVITIES);
                if (packageInfo != null) {
                    Intent viewDownloadsIntent = new Intent(DownloadManager.ACTION_VIEW_DOWNLOADS);
                    viewDownloadsIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
                    viewDownloadsIntent.setPackage(DOCUMENTS_UI_PACKAGE_NAME);
                    context.startActivity(viewDownloadsIntent);
                    return true;
                }
            } catch (Exception e) {
                Log.e(TAG, "Cannot find files app for openning zip files", e);
            }
        }
        // Can't launch the Intent.
        if (source != DownloadOpenSource.DOWNLOAD_PROGRESS_INFO_BAR) {
            Toast.makeText(context, context.getString(R.string.download_cant_open_file),
                         Toast.LENGTH_SHORT)
                    .show();
        }
        return false;
    }

    /**
     * Opens a completed download.
     * @param filePath File path on disk of the download to open.
     * @param mimeType MIME type of the downloaded file.
     * @param downloadGuid Unique GUID of the download.
     * @param otrProfileID User's OTRProfileID.
     * @param originalUrl URL which initially triggered the download itself.
     * @param referer URL of the page which redirected to the download URL.
     * @param source Where this download was initiated from.
     */
    @CalledByNative
    public static void openDownload(String filePath, String mimeType, String downloadGuid,
            OTRProfileID otrProfileID, String originalUrl, String referer,
            @DownloadOpenSource int source) {
        // Mapping generic MIME type to android openable type based on URL and file extension.
        String newMimeType = MimeUtils.remapGenericMimeType(mimeType, originalUrl, filePath);
        Activity activity = ApplicationStatus.getLastTrackedFocusedActivity();
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.CCT_NEW_DOWNLOAD_TAB)) {
            DownloadMessageUiController messageUiController =
                    DownloadManagerService.getDownloadManagerService().getMessageUiController(null);
            if (messageUiController != null
                    && messageUiController.isDownloadInterstitialItem(
                            new GURL(originalUrl), downloadGuid)) {
                return;
            }
        }
        boolean canOpen = DownloadUtils.openFile(filePath, newMimeType, downloadGuid, otrProfileID,
                originalUrl, referer, source,
                activity == null ? ContextUtils.getApplicationContext() : activity);
        if (!canOpen) {
            DownloadUtils.showDownloadManager(null, null, otrProfileID, source);
        }
    }

    /**
     * Fires an Intent to open a downloaded item.
     * @param context Context to use.
     * @param intent  Intent that can be fired.
     * @return Whether an Activity was successfully started for the Intent.
     */
    static boolean fireOpenIntentForDownload(Context context, Intent intent) {
        try {
            if (TextUtils.equals(intent.getPackage(), context.getPackageName())) {
                IntentHandler.startActivityForTrustedIntent(intent);
            } else {
                context.startActivity(intent);
            }
            return true;
        } catch (ActivityNotFoundException ex) {
            Log.d(TAG, "Activity not found for " + intent.getType() + " over "
                    + intent.getData().getScheme(), ex);
        } catch (SecurityException ex) {
            Log.d(TAG, "cannot open intent: " + intent, ex);
        } catch (Exception ex) {
            Log.d(TAG, "cannot open intent: " + intent, ex);
        }

        return false;
    }

    /**
     * Get the resume mode based on the current fail state, to distinguish the case where download
     * cannot be resumed at all or can be resumed in the middle, or should be restarted from the
     * beginning.
     * @param url URL of the download.
     * @param failState Why the download failed.
     * @return The resume mode for the current fail state.
     */
    public static @ResumeMode int getResumeMode(String url, @FailState int failState) {
        return DownloadUtilsJni.get().getResumeMode(url, failState);
    }

    /**
     * Query the Download backends about whether a download is paused.
     *
     * The Java-side contains more information about the status of a download than is persisted
     * by the native backend, so it is queried first.
     *
     * @param item Download to check the status of.
     * @return Whether the download is paused or not.
     */
    public static boolean isDownloadPaused(DownloadItem item) {
        DownloadSharedPreferenceHelper helper = DownloadSharedPreferenceHelper.getInstance();
        DownloadSharedPreferenceEntry entry =
                helper.getDownloadSharedPreferenceEntry(item.getContentId());

        if (entry != null) {
            // The Java downloads backend knows more about the download than the native backend.
            return !entry.isAutoResumable;
        } else {
            // Only the native downloads backend knows about the download.
            if (item.getDownloadInfo().state() == DownloadState.IN_PROGRESS) {
                return item.getDownloadInfo().isPaused();
            } else {
                return item.getDownloadInfo().state() == DownloadState.INTERRUPTED;
            }
        }
    }

    /**
     * Return whether a download is pending.
     * @param item Download to check the status of.
     * @return Whether the download is pending or not.
     */
    public static boolean isDownloadPending(DownloadItem item) {
        DownloadSharedPreferenceHelper helper = DownloadSharedPreferenceHelper.getInstance();
        DownloadSharedPreferenceEntry entry =
                helper.getDownloadSharedPreferenceEntry(item.getContentId());
        return entry != null && item.getDownloadInfo().state() == DownloadState.INTERRUPTED
                && entry.isAutoResumable;
    }

    /**
     * Gets the duplicate infobar or dialog text for regular downloads.
     * @param context Context to be used.
     * @param template Template of the text to be displayed.
     * @param filePath Suggested file path of the download.
     * @param addSizeStringIfAvailable Whether size string should be added to the dialog text.
     * @param totalBytes Size of the download.
     * @param clickableSpan Action to perform when clicking on the file name.
     * @return message to be displayed on the infobar or dialog.
     */
    public static CharSequence getDownloadMessageText(final Context context, final String template,
            final String filePath, boolean addSizeStringIfAvailable, long totalBytes,
            final ClickableSpan clickableSpan) {
        return getMessageText(template, new File(filePath).getName(), addSizeStringIfAvailable,
                totalBytes, clickableSpan);
    }

    /**
     * Gets the infobar or dialog text for offline page downloads.
     * @param context Context to be used.
     * @param filePath Path of the file to be displayed.
     * @param duplicateRequestExists Whether a duplicate request exists.
     * @param clickableSpan Action to perform when clicking on the page link.
     * @return message to be displayed on the infobar or dialog.
     */
    public static CharSequence getOfflinePageMessageText(final Context context, String filePath,
            boolean duplicateRequestExists, ClickableSpan clickableSpan) {
        String template = context.getString(duplicateRequestExists
                        ? R.string.duplicate_download_request_infobar_text
                        : R.string.duplicate_download_infobar_text);
        return getMessageText(template, filePath, false /*addSizeStringIfAvailable*/,
                0 /*totalBytes*/, clickableSpan);
    }

    /**
     * Open a given page URL.
     * @param context Context to be used.
     * @param pageUrl URL of the page.
     */
    public static void openPageUrl(Context context, String pageUrl) {
        Intent intent = new Intent(Intent.ACTION_VIEW);
        intent.setData(Uri.parse(pageUrl));
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.setPackage(context.getPackageName());
        context.startActivity(intent);
    }

    /**
     * Helper method to get the text to be displayed for duplicate download.
     * @param template Message template.
     * @param fileName Name of the file.
     * @param addSizeStringIfAvailable Whether size string should be added to the dialog text.
     * @param totalBytes Size of the download.
     * @param clickableSpan Action to perform when clicking on the file name.
     * @return message to be displayed on the infobar.
     */
    private static CharSequence getMessageText(final String template, final String fileName,
            boolean addSizeStringIfAvailable, long totalBytes, final ClickableSpan clickableSpan) {
        final SpannableString formattedFilePath = new SpannableString(fileName);
        formattedFilePath.setSpan(new StyleSpan(Typeface.BOLD), 0, fileName.length(),
                Spannable.SPAN_EXCLUSIVE_EXCLUSIVE);
        formattedFilePath.setSpan(
                clickableSpan, 0, fileName.length(), Spannable.SPAN_EXCLUSIVE_EXCLUSIVE);

        if (!addSizeStringIfAvailable) {
            return TextUtils.expandTemplate(template, formattedFilePath);
        } else {
            String sizeString = "";
            if (totalBytes > 0) {
                sizeString = " ("
                        + org.chromium.components.browser_ui.util.DownloadUtils.getStringForBytes(
                                ContextUtils.getApplicationContext(), totalBytes)
                        + ")";
            }

            return TextUtils.expandTemplate(template, formattedFilePath, sizeString);
        }
    }

    @NativeMethods
    interface Natives {
        int getResumeMode(String url, @FailState int failState);
    }
}
