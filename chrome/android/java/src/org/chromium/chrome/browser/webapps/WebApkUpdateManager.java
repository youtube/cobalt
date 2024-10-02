// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import static org.chromium.components.webapk.lib.common.WebApkConstants.WEBAPK_PACKAGE_PREFIX;

import android.content.Context;
import android.os.Handler;
import android.text.TextUtils;
import android.text.format.DateUtils;
import android.util.Pair;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.CommandLine;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.AsyncTask;
import org.chromium.blink.mojom.DisplayMode;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.intents.MergedWebappInfo;
import org.chromium.chrome.browser.browserservices.intents.WebApkExtras;
import org.chromium.chrome.browser.browserservices.intents.WebApkShareTarget;
import org.chromium.chrome.browser.browserservices.intents.WebappInfo;
import org.chromium.chrome.browser.browserservices.metrics.WebApkUmaRecorder;
import org.chromium.chrome.browser.browserservices.metrics.WebApkUmaRecorder.UpdateRequestQueued;
import org.chromium.chrome.browser.dependency_injection.ActivityScope;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.DestroyObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.background_task_scheduler.BackgroundTaskSchedulerFactory;
import org.chromium.components.background_task_scheduler.TaskIds;
import org.chromium.components.background_task_scheduler.TaskInfo;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.webapps.WebApkInstallResult;
import org.chromium.components.webapps.WebApkUpdateReason;
import org.chromium.components.webapps.WebappsIconUtils;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.util.ArrayList;
import java.util.List;
import java.util.Map;

import javax.inject.Inject;

/**
 * WebApkUpdateManager manages when to check for updates to the WebAPK's Web Manifest, and sends
 * an update request to the WebAPK Server when an update is needed.
 */
@ActivityScope
public class WebApkUpdateManager implements WebApkUpdateDataFetcher.Observer, DestroyObserver {
    private static final String TAG = "WebApkUpdateManager";

    // Maximum wait time for WebAPK update to be scheduled.
    private static final long UPDATE_TIMEOUT_MILLISECONDS = DateUtils.SECOND_IN_MILLIS * 30;

    // Flags for AppIdentity Update dialog histograms.
    private static final int CHANGING_NOTHING = 0;
    private static final int CHANGING_ICON = 1 << 0;
    private static final int CHANGING_ICON_MASK = 1 << 1;
    private static final int CHANGING_APP_NAME = 1 << 2;
    private static final int CHANGING_SHORTNAME = 1 << 3;
    private static final int HISTOGRAM_SCOPE = 1 << 4;

    private static final String PARAM_SHELL_VERSION = "shell_version";

    private final ActivityTabProvider mTabProvider;

    /** Whether updates are enabled. Some tests disable updates. */
    private static boolean sUpdatesEnabled = true;

    /** The minimum shell version the WebAPK needs to be using. */
    private static int sWebApkTargetShellVersion;

    /** Data extracted from the WebAPK's launch intent and from the WebAPK's Android Manifest. */
    private WebappInfo mInfo;

    /** The updated manifest information. */
    private MergedWebappInfo mFetchedInfo;

    /** The URL of the primary icon. */
    private String mFetchedPrimaryIconUrl;

    /** The URL of the splash icon. */
    private String mFetchedSplashIconUrl;

    /** The list of reasons why an update was triggered. */
    private @WebApkUpdateReason List<Integer> mUpdateReasons;

    /** The WebappDataStorage with cached data about prior update requests. */
    private WebappDataStorage mStorage;

    private WebApkUpdateDataFetcher mFetcher;

    /** Runs failure callback if WebAPK update is not scheduled within deadline. */
    private Handler mUpdateFailureHandler;

    /** Called with update result. */
    public static interface WebApkUpdateCallback {
        @CalledByNative("WebApkUpdateCallback")
        public void onResultFromNative(@WebApkInstallResult int result, boolean relaxUpdates);
    }

    @Inject
    public WebApkUpdateManager(
            ActivityTabProvider tabProvider, ActivityLifecycleDispatcher lifecycleDispatcher) {
        mTabProvider = tabProvider;
        lifecycleDispatcher.register(this);
    }

    /**
     * Checks whether the WebAPK's Web Manifest has changed. Requests an updated WebAPK if the Web
     * Manifest has changed. Skips the check if the check was done recently.
     */
    public void updateIfNeeded(
            WebappDataStorage storage, BrowserServicesIntentDataProvider intentDataProvider) {
        mStorage = storage;
        mInfo = WebappInfo.create(intentDataProvider);

        Tab tab = mTabProvider.get();

        if (tab == null || !shouldCheckIfWebManifestUpdated(mInfo)) return;

        mFetcher = buildFetcher();
        mFetcher.start(tab, mInfo, this);
        mUpdateFailureHandler = new Handler();
        mUpdateFailureHandler.postDelayed(new Runnable() {
            @Override
            public void run() {
                onGotManifestData(null, null, null);
            }
        }, updateTimeoutMilliseconds());
    }

    @Override
    public void onDestroy() {
        destroyFetcher();
        if (mUpdateFailureHandler != null) {
            mUpdateFailureHandler.removeCallbacksAndMessages(null);
        }
    }

    public static void setUpdatesEnabledForTesting(boolean enabled) {
        sUpdatesEnabled = enabled;
    }

    @Override
    public void onGotManifestData(BrowserServicesIntentDataProvider fetchedIntentDataProvider,
            String primaryIconUrl, String splashIconUrl) {
        mFetchedPrimaryIconUrl = primaryIconUrl;
        mFetchedSplashIconUrl = splashIconUrl;
        mFetchedInfo =
                MergedWebappInfo.create(/* oldWebappInfo= */ mInfo, fetchedIntentDataProvider);

        if (mFetchedInfo != null) {
            // When only some/no app identity updates are permitted, the update
            // should still proceed with updating all the other values (not related
            // to the blocked app identity update).
            if (!nameUpdateDialogEnabled()) {
                mFetchedInfo.setUseOldName(true);
            }
            if (!iconUpdateDialogEnabled()
                    && !allowIconUpdateForShellVersion(mInfo.shellApkVersion())) {
                mFetchedInfo.setUseOldIcon(true);
                // Forces recreation of the primary icon during proto construction.
                mFetchedPrimaryIconUrl = "";
            }
        }

        mStorage.updateTimeOfLastCheckForUpdatedWebManifest();
        if (mUpdateFailureHandler != null) {
            mUpdateFailureHandler.removeCallbacksAndMessages(null);
        }

        boolean gotManifest = (mFetchedInfo != null);
        mUpdateReasons = generateUpdateReasons(
                mInfo, mFetchedInfo, mFetchedPrimaryIconUrl, mFetchedSplashIconUrl);
        boolean needsUpgrade = !mUpdateReasons.isEmpty();
        if (mStorage.shouldForceUpdate() && needsUpgrade) {
            // Add to the front of the list to designate it as the primary reason.
            mUpdateReasons.add(0, WebApkUpdateReason.MANUALLY_TRIGGERED);
        }
        Log.i(TAG, "Got Manifest: " + gotManifest);
        Log.i(TAG, "WebAPK upgrade needed: " + needsUpgrade);

        // If the Web Manifest was not found and an upgrade is requested, stop fetching Web
        // Manifests as the user navigates to avoid sending multiple WebAPK update requests. In
        // particular:
        // - A WebAPK update request on the initial load because the Shell APK version is out of
        //   date.
        // - A second WebAPK update request once the user navigates to a page which points to the
        //   correct Web Manifest URL because the Web Manifest has been updated by the Web
        //   developer.
        //
        // If the Web Manifest was not found and an upgrade is not requested, keep on fetching
        // Web Manifests as the user navigates. For instance, the WebAPK's start_url might not
        // point to a Web Manifest because start_url redirects to the WebAPK's main page.
        if (gotManifest || needsUpgrade) {
            destroyFetcher();
        }

        if (ChromeFeatureList.isEnabled(ChromeFeatureList.WEB_APK_UNIQUE_ID)
                && TextUtils.isEmpty(mInfo.manifestId())) {
            RecordHistogram.recordBooleanHistogram(
                    "WebApk.Update.UpdateEmptyUniqueId.NeedsUpgrade", needsUpgrade);
        }

        if (!needsUpgrade) {
            if (!mStorage.didPreviousUpdateSucceed() || mStorage.shouldForceUpdate()) {
                onFinishedUpdate(mStorage, WebApkInstallResult.SUCCESS, false /* relaxUpdates */);
            }
            return;
        }

        boolean iconChanging = mUpdateReasons.contains(WebApkUpdateReason.PRIMARY_ICON_HASH_DIFFERS)
                || mUpdateReasons.contains(WebApkUpdateReason.PRIMARY_ICON_MASKABLE_DIFFERS);
        boolean shortNameChanging = mUpdateReasons.contains(WebApkUpdateReason.SHORT_NAME_DIFFERS);
        boolean nameChanging = mUpdateReasons.contains(WebApkUpdateReason.NAME_DIFFERS);

        int histogramAction = CHANGING_NOTHING;
        if (mUpdateReasons.contains(WebApkUpdateReason.PRIMARY_ICON_HASH_DIFFERS)) {
            histogramAction |= CHANGING_ICON;
        }
        if (mUpdateReasons.contains(WebApkUpdateReason.PRIMARY_ICON_MASKABLE_DIFFERS)) {
            histogramAction |= CHANGING_ICON_MASK;
        }
        if (nameChanging) histogramAction |= CHANGING_APP_NAME;
        if (shortNameChanging) histogramAction |= CHANGING_SHORTNAME;

        // Use the original `primaryIconUrl` (as opposed to `mFetchedPrimaryIconUrl`) to construct
        // the hash, which ensures that we don't regress on issue crbug.com/1287447.
        String hash = getAppIdentityHash(mFetchedInfo, primaryIconUrl);
        boolean alreadyUserApproved = !hash.isEmpty()
                && TextUtils.equals(hash, mStorage.getLastWebApkUpdateHashAccepted());
        boolean showDialogForName =
                (nameChanging || shortNameChanging) && nameUpdateDialogEnabled();
        boolean showDialogForIcon = iconChanging && iconUpdateDialogEnabled()
                && !allowIconUpdateForShellVersion(mInfo.shellApkVersion());

        if ((!showDialogForName && !showDialogForIcon) || alreadyUserApproved) {
            if (alreadyUserApproved) {
                RecordHistogram.recordEnumeratedHistogram(
                        "Webapp.AppIdentityDialog.AlreadyApproved", histogramAction,
                        HISTOGRAM_SCOPE);
            } else {
                RecordHistogram.recordEnumeratedHistogram(
                        "Webapp.AppIdentityDialog.NotShowing", histogramAction, HISTOGRAM_SCOPE);
            }

            // It might seem non-obvious why the POSITIVE button is selected when we've determined
            // that App Identity updates are not enabled or when nothing meaningful is changing.
            // Keep in mind, though, that the update being processed might not be app identity
            // related and those updates must still go through. The server keeps track of whether an
            // identity update has been approved by the user, using the `appIdentityUpdateSupported`
            // flag, so the app won't be able to update its identity even if we use the POSITIVE
            // button here.
            onUserApprovedUpdate(DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
            return;
        }

        RecordHistogram.recordEnumeratedHistogram(
                "Webapp.AppIdentityDialog.Showing", histogramAction, HISTOGRAM_SCOPE);
        showIconOrNameUpdateDialog(iconChanging, shortNameChanging, nameChanging);
    }

    protected boolean iconUpdateDialogEnabled() {
        return ChromeFeatureList.isEnabled(ChromeFeatureList.PWA_UPDATE_DIALOG_FOR_ICON);
    }

    protected boolean nameUpdateDialogEnabled() {
        return ChromeFeatureList.isEnabled(ChromeFeatureList.PWA_UPDATE_DIALOG_FOR_NAME);
    }

    private boolean allowIconUpdateForShellVersion(int shellVersion) {
        return ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                       ChromeFeatureList.WEB_APK_ALLOW_ICON_UPDATA, PARAM_SHELL_VERSION, 0)
                >= shellVersion;
    }

    protected void showIconOrNameUpdateDialog(
            boolean iconChanging, boolean shortNameChanging, boolean nameChanging) {
        // Show the dialog to confirm name and/or icon update.
        ModalDialogManager dialogManager =
                mTabProvider.get().getWindowAndroid().getModalDialogManager();
        Context context = mTabProvider.get().getContext();
        WebApkIconNameUpdateDialog dialog = new WebApkIconNameUpdateDialog();
        dialog.show(context, dialogManager, mInfo.webApkPackageName(), iconChanging,
                shortNameChanging, nameChanging, mInfo.shortName(), mFetchedInfo.shortName(),
                mInfo.name(), mFetchedInfo.name(), mInfo.icon().bitmap(),
                mFetchedInfo.icon().bitmap(), mInfo.isIconAdaptive(), mFetchedInfo.isIconAdaptive(),
                this::onUserApprovedUpdate);
    }

    private String getAppIdentityHash(WebappInfo info, String primaryIconUrl) {
        if (info == null) return "";
        return info.name() + "|" + info.shortName() + "|"
                + info.iconUrlToMurmur2HashMap().get(primaryIconUrl)
                + (info.isIconAdaptive() ? "|Adaptive" : "|NotAdaptive");
    }

    protected void onUserApprovedUpdate(int dismissalCause) {
        // Set WebAPK update as having failed in case that Chrome is killed prior to
        // {@link onBuiltWebApk} being called.
        recordUpdate(mStorage, WebApkInstallResult.FAILURE, false /* relaxUpdates*/);

        // Continue if the user explicitly allows the update using the button, or isn't interested
        // in the update dialog warning (presses Back). Otherwise, they can be left in a state where
        // they always press Back and are stuck on an old version of the app forever.
        if (dismissalCause != DialogDismissalCause.POSITIVE_BUTTON_CLICKED
                && dismissalCause != DialogDismissalCause.NAVIGATE_BACK_OR_TOUCH_OUTSIDE) {
            return;
        }

        String hash = getAppIdentityHash(mFetchedInfo, mFetchedPrimaryIconUrl);
        if (!hash.isEmpty()) mStorage.updateLastWebApkUpdateHashAccepted(hash);

        if (mFetchedInfo != null) {
            buildUpdateRequestAndSchedule(mFetchedInfo, mFetchedPrimaryIconUrl,
                    mFetchedSplashIconUrl, false /* isManifestStale */,
                    nameUpdateDialogEnabled()
                            || iconUpdateDialogEnabled() /* appIdentityUpdateSupported */,
                    mUpdateReasons);
            return;
        }

        // Tell the server that the our version of the Web Manifest might be stale and to ignore
        // our Web Manifest data if the server's Web Manifest data is newer. This scenario can
        // occur if the Web Manifest is temporarily unreachable.
        buildUpdateRequestAndSchedule(mInfo, "" /* primaryIconUrl */, "" /* splashIconUrl */,
                true /* isManifestStale */,
                nameUpdateDialogEnabled()
                        || iconUpdateDialogEnabled() /* appIdentityUpdateSupported */,
                mUpdateReasons);
    }

    /**
     * Builds {@link WebApkUpdateDataFetcher}. In a separate function for the sake of tests.
     */
    @VisibleForTesting
    protected WebApkUpdateDataFetcher buildFetcher() {
        return new WebApkUpdateDataFetcher();
    }

    @VisibleForTesting
    protected long updateTimeoutMilliseconds() {
        return UPDATE_TIMEOUT_MILLISECONDS;
    }

    /** Builds proto to send to the WebAPK server. */
    private void buildUpdateRequestAndSchedule(WebappInfo info, String primaryIconUrl,
            String splashIconUrl, boolean isManifestStale, boolean appIdentityUpdateSupported,
            List<Integer> updateReasons) {
        recordWebApkUpdateUniqueIdHistogram(mInfo, mFetchedInfo);

        Callback<Boolean> callback = (success) -> {
            if (!success) {
                onFinishedUpdate(mStorage, WebApkInstallResult.FAILURE, false /* relaxUpdates*/);
                return;
            }
            scheduleUpdate();
        };
        String updateRequestPath = mStorage.createAndSetUpdateRequestFilePath(info);
        encodeIconsInBackground(updateRequestPath, info, primaryIconUrl, splashIconUrl,
                isManifestStale, appIdentityUpdateSupported, updateReasons, callback);
    }

    private void recordWebApkUpdateUniqueIdHistogram(WebappInfo oldInfo, WebappInfo fetchedInfo) {
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.WEB_APK_UNIQUE_ID)) return;
        if (fetchedInfo == null) return;

        String baseName = "WebApk.Update.UniqueId"
                + (TextUtils.isEmpty(oldInfo.manifestId()) ? "Empty" : "Same");
        RecordHistogram.recordBooleanHistogram(baseName + ".ManifestUrl",
                TextUtils.equals(oldInfo.manifestUrl(), fetchedInfo.manifestUrl()));
        RecordHistogram.recordBooleanHistogram(baseName + ".StartUrl",
                TextUtils.equals(oldInfo.manifestStartUrl(), fetchedInfo.manifestStartUrl()));
    }

    /** Schedules update for when WebAPK is not running. */
    @VisibleForTesting
    protected void scheduleUpdate() {
        WebApkUmaRecorder.recordUpdateRequestQueued(UpdateRequestQueued.TWICE);
        TaskInfo updateTask;
        if (mStorage.shouldForceUpdate()) {
            // Start an update task ASAP for forced updates.
            updateTask =
                    TaskInfo.createOneOffTask(TaskIds.WEBAPK_UPDATE_JOB_ID, 0 /* windowEndTimeMs */)
                            .setUpdateCurrent(true)
                            .setIsPersisted(true)
                            .build();
            mStorage.setUpdateScheduled(true);
            mStorage.setShouldForceUpdate(false);
        } else {
            // The task deadline should be before {@link WebappDataStorage#RETRY_UPDATE_DURATION}
            updateTask = TaskInfo.createOneOffTask(TaskIds.WEBAPK_UPDATE_JOB_ID,
                                         DateUtils.HOUR_IN_MILLIS, DateUtils.HOUR_IN_MILLIS * 23)
                                 .setRequiredNetworkType(TaskInfo.NetworkType.UNMETERED)
                                 .setUpdateCurrent(true)
                                 .setIsPersisted(true)
                                 .setRequiresCharging(true)
                                 .build();
        }

        BackgroundTaskSchedulerFactory.getScheduler().schedule(
                ContextUtils.getApplicationContext(), updateTask);
    }

    /** Sends update request to the WebAPK Server. Should be called when WebAPK is not running. */
    public static void updateWhileNotRunning(
            final WebappDataStorage storage, final Runnable callback) {
        Log.i(TAG, "Update now");
        WebApkUpdateCallback callbackRunner = (result, relaxUpdates) -> {
            onFinishedUpdate(storage, result, relaxUpdates);
            callback.run();
        };

        WebApkUmaRecorder.recordUpdateRequestSent(
                WebApkUmaRecorder.UpdateRequestSent.WHILE_WEBAPK_CLOSED);
        WebApkUpdateManagerJni.get().updateWebApkFromFile(
                storage.getPendingUpdateRequestPath(), callbackRunner);
    }

    /**
     * Destroys {@link mFetcher}. In a separate function for the sake of tests.
     */
    protected void destroyFetcher() {
        if (mFetcher != null) {
            mFetcher.destroy();
            mFetcher = null;
        }
    }

    private static int webApkTargetShellVersion() {
        if (sWebApkTargetShellVersion == 0) {
            sWebApkTargetShellVersion = WebApkUpdateManagerJni.get().getWebApkTargetShellVersion();
        }
        return sWebApkTargetShellVersion;
    }

    /**
     * Whether there is a new version of the //chrome/android/webapk/shell_apk code.
     */
    private static boolean isShellApkVersionOutOfDate(WebappInfo info) {
        return info.shellApkVersion() < webApkTargetShellVersion();
    }

    /**
     * Returns whether the Web Manifest should be refetched to check whether it has been updated.
     * TODO: Make this method static once there is a static global clock class.
     * @param info Meta data from WebAPK's Android Manifest.
     * True if there has not been any update attempts.
     */
    private boolean shouldCheckIfWebManifestUpdated(WebappInfo info) {
        if (!sUpdatesEnabled) return false;

        if (CommandLine.getInstance().hasSwitch(
                    ChromeSwitches.CHECK_FOR_WEB_MANIFEST_UPDATE_ON_STARTUP)) {
            return true;
        }

        if (!info.webApkPackageName().startsWith(WEBAPK_PACKAGE_PREFIX)) return false;

        if (isShellApkVersionOutOfDate(info)
                && webApkTargetShellVersion() > mStorage.getLastRequestedShellApkVersion()) {
            return true;
        }

        return mStorage.shouldCheckForUpdate();
    }

    /**
     * Updates {@link WebappDataStorage} with the time of the latest WebAPK update and whether the
     * WebAPK update succeeded. Also updates the last requested "shell APK version".
     */
    private static void recordUpdate(
            WebappDataStorage storage, @WebApkInstallResult int result, boolean relaxUpdates) {
        // Update the request time and result together. It prevents getting a correct request time
        // but a result from the previous request.
        storage.updateTimeOfLastWebApkUpdateRequestCompletion();
        storage.updateDidLastWebApkUpdateRequestSucceed(result == WebApkInstallResult.SUCCESS);
        storage.setRelaxedUpdates(relaxUpdates);
        storage.updateLastRequestedShellApkVersion(webApkTargetShellVersion());
    }

    /**
     * Callback for when WebAPK update finishes or succeeds. Unlike {@link #recordUpdate()}
     * cannot be called while update is in progress.
     */
    private static void onFinishedUpdate(
            WebappDataStorage storage, @WebApkInstallResult int result, boolean relaxUpdates) {
        storage.setShouldForceUpdate(false);
        storage.setUpdateScheduled(false);
        recordUpdate(storage, result, relaxUpdates);
        storage.deletePendingUpdateRequestFile();
    }

    private static boolean shortcutsDiffer(List<WebApkExtras.ShortcutItem> oldShortcuts,
            List<WebApkExtras.ShortcutItem> fetchedShortcuts) {
        assert oldShortcuts != null;
        assert fetchedShortcuts != null;

        if (fetchedShortcuts.size() != oldShortcuts.size()) {
            return true;
        }

        for (int i = 0; i < oldShortcuts.size(); i++) {
            if (!TextUtils.equals(oldShortcuts.get(i).name, fetchedShortcuts.get(i).name)
                    || !TextUtils.equals(
                            oldShortcuts.get(i).shortName, fetchedShortcuts.get(i).shortName)
                    || !TextUtils.equals(
                            oldShortcuts.get(i).launchUrl, fetchedShortcuts.get(i).launchUrl)
                    || !TextUtils.equals(
                            oldShortcuts.get(i).iconHash, fetchedShortcuts.get(i).iconHash)) {
                return true;
            }
        }

        return false;
    }

    /**
     * Returns a list of reasons why the WebAPK needs to be updated.
     * @param oldInfo        Data extracted from WebAPK manifest.
     * @param fetchedInfo    Fetched data for Web Manifest.
     * @param primaryIconUrl The icon URL in {@link fetchedInfo#iconUrlToMurmur2HashMap()} best
     *                       suited for use as the launcher icon on this device.
     * @param splashIconUrl  The icon URL in {@link fetchedInfo#iconUrlToMurmur2HashMap()} best
     *                       suited for use as the splash icon on this device.
     * @return reasons that an update is needed or an empty list if an update is not needed.
     */
    private static List<Integer> generateUpdateReasons(WebappInfo oldInfo, WebappInfo fetchedInfo,
            String primaryIconUrl, String splashIconUrl) {
        List<Integer> updateReasons = new ArrayList<>();

        if (isShellApkVersionOutOfDate(oldInfo)) {
            updateReasons.add(WebApkUpdateReason.OLD_SHELL_APK);
        }
        if (fetchedInfo == null) {
            return updateReasons;
        }

        // We should have computed the Murmur2 hashes for the bitmaps at the primary icon URL and
        // the splash icon for {@link fetchedInfo} (but not the other icon URLs.)
        String fetchedPrimaryIconMurmur2Hash = fetchedInfo.iconUrlToMurmur2HashMap()
                .get(primaryIconUrl);
        String primaryIconMurmur2Hash = findMurmur2HashForUrlIgnoringFragment(
                oldInfo.iconUrlToMurmur2HashMap(), primaryIconUrl);
        String fetchedSplashIconMurmur2Hash =
                fetchedInfo.iconUrlToMurmur2HashMap().get(splashIconUrl);
        String splashIconMurmur2Hash = findMurmur2HashForUrlIgnoringFragment(
                oldInfo.iconUrlToMurmur2HashMap(), splashIconUrl);

        if (!TextUtils.equals(primaryIconMurmur2Hash, fetchedPrimaryIconMurmur2Hash)) {
            updateReasons.add(WebApkUpdateReason.PRIMARY_ICON_HASH_DIFFERS);
        }
        if (!TextUtils.equals(splashIconMurmur2Hash, fetchedSplashIconMurmur2Hash)) {
            updateReasons.add(WebApkUpdateReason.SPLASH_ICON_HASH_DIFFERS);
        }
        if (!UrlUtilities.urlsMatchIgnoringFragments(oldInfo.scopeUrl(), fetchedInfo.scopeUrl())) {
            updateReasons.add(WebApkUpdateReason.SCOPE_DIFFERS);
        }
        if (!UrlUtilities.urlsMatchIgnoringFragments(
                    oldInfo.manifestStartUrl(), fetchedInfo.manifestStartUrl())) {
            updateReasons.add(WebApkUpdateReason.START_URL_DIFFERS);
        }
        if (!TextUtils.equals(oldInfo.shortName(), fetchedInfo.shortName())) {
            updateReasons.add(WebApkUpdateReason.SHORT_NAME_DIFFERS);
        }
        if (!TextUtils.equals(oldInfo.name(), fetchedInfo.name())) {
            updateReasons.add(WebApkUpdateReason.NAME_DIFFERS);
        }
        if (oldInfo.backgroundColor() != fetchedInfo.backgroundColor()) {
            updateReasons.add(WebApkUpdateReason.BACKGROUND_COLOR_DIFFERS);
        }
        if (oldInfo.toolbarColor() != fetchedInfo.toolbarColor()) {
            updateReasons.add(WebApkUpdateReason.THEME_COLOR_DIFFERS);
        }
        if (oldInfo.orientation() != fetchedInfo.orientation()) {
            updateReasons.add(WebApkUpdateReason.ORIENTATION_DIFFERS);
        }
        if (oldInfo.displayMode() != fetchedInfo.displayMode()) {
            updateReasons.add(WebApkUpdateReason.DISPLAY_MODE_DIFFERS);
        }
        if (!WebApkShareTarget.equals(oldInfo.shareTarget(), fetchedInfo.shareTarget())) {
            updateReasons.add(WebApkUpdateReason.WEB_SHARE_TARGET_DIFFERS);
        }
        if (oldInfo.isIconAdaptive() != fetchedInfo.isIconAdaptive()
                && (!fetchedInfo.isIconAdaptive()
                        || WebappsIconUtils.doesAndroidSupportMaskableIcons())) {
            updateReasons.add(WebApkUpdateReason.PRIMARY_ICON_MASKABLE_DIFFERS);
        }
        if (shortcutsDiffer(oldInfo.shortcutItems(), fetchedInfo.shortcutItems())) {
            updateReasons.add(WebApkUpdateReason.SHORTCUTS_DIFFER);
        }
        return updateReasons;
    }

    /**
     * Returns the Murmur2 hash for entry in {@link iconUrlToMurmur2HashMap} whose canonical
     * representation, ignoring fragments, matches {@link iconUrlToMatch}.
     */
    private static String findMurmur2HashForUrlIgnoringFragment(
            Map<String, String> iconUrlToMurmur2HashMap, String iconUrlToMatch) {
        for (Map.Entry<String, String> entry : iconUrlToMurmur2HashMap.entrySet()) {
            if (UrlUtilities.urlsMatchIgnoringFragments(entry.getKey(), iconUrlToMatch)) {
                return entry.getValue();
            }
        }
        return null;
    }

    // Encode the icons in a background process since encoding is expensive.
    protected void encodeIconsInBackground(String updateRequestPath, WebappInfo info,
            String primaryIconUrl, String splashIconUrl, boolean isManifestStale,
            boolean isAppIdentityUpdateSupported, List<Integer> updateReasons,
            Callback<Boolean> callback) {
        new AsyncTask<Pair<String, String>>() {
            @Override
            protected Pair<String, String> doInBackground() {
                String primaryIconData = info.icon().encoded();
                String splashIconData = info.splashIcon().encoded();
                return Pair.create(primaryIconData, splashIconData);
            }

            @Override
            protected void onPostExecute(Pair<String, String> result) {
                storeWebApkUpdateRequestToFile(updateRequestPath, info, primaryIconUrl,
                        result.first, splashIconUrl, result.second, isManifestStale,
                        isAppIdentityUpdateSupported, updateReasons, callback);
            }
        }.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
    }

    protected void storeWebApkUpdateRequestToFile(String updateRequestPath, WebappInfo info,
            String primaryIconUrl, String primaryIconData, String splashIconUrl,
            String splashIconData, boolean isManifestStale, boolean isAppIdentityUpdateSupported,
            List<Integer> updateReasons, Callback<Boolean> callback) {
        int versionCode = info.webApkVersionCode();
        int size = info.iconUrlToMurmur2HashMap().size();
        String[] iconUrls = new String[size];
        String[] iconHashes = new String[size];
        int i = 0;
        for (Map.Entry<String, String> entry : info.iconUrlToMurmur2HashMap().entrySet()) {
            iconUrls[i] = entry.getKey();
            String iconHash = entry.getValue();
            iconHashes[i] = (iconHash != null) ? iconHash : "";
            i++;
        }

        String maniefstId =
                TextUtils.isEmpty(info.manifestId()) ? info.manifestStartUrl() : info.manifestId();

        String[][] shortcuts = new String[info.shortcutItems().size()][];
        byte[][] shortcutIconData = new byte[info.shortcutItems().size()][];
        for (int j = 0; j < info.shortcutItems().size(); j++) {
            WebApkExtras.ShortcutItem shortcut = info.shortcutItems().get(j);
            shortcuts[j] = new String[] {shortcut.name, shortcut.shortName, shortcut.launchUrl,
                    shortcut.iconUrl, shortcut.iconHash};
            shortcutIconData[j] = shortcut.icon.data();
        }

        String shareTargetAction = "";
        String shareTargetParamTitle = "";
        String shareTargetParamText = "";
        boolean shareTargetIsMethodPost = false;
        boolean shareTargetIsEncTypeMultipart = false;
        String[] shareTargetParamFileNames = new String[0];
        String[][] shareTargetParamAccepts = new String[0][];
        WebApkShareTarget shareTarget = info.shareTarget();
        if (shareTarget != null) {
            shareTargetAction = shareTarget.getAction();
            shareTargetParamTitle = shareTarget.getParamTitle();
            shareTargetParamText = shareTarget.getParamText();
            shareTargetIsMethodPost = shareTarget.isShareMethodPost();
            shareTargetIsEncTypeMultipart = shareTarget.isShareEncTypeMultipart();
            shareTargetParamFileNames = shareTarget.getFileNames();
            shareTargetParamAccepts = shareTarget.getFileAccepts();
        }

        int[] updateReasonsArray = new int[updateReasons.size()];
        for (int j = 0; j < updateReasons.size(); j++) {
            updateReasonsArray[j] = updateReasons.get(j);
        }

        WebApkUpdateManagerJni.get().storeWebApkUpdateRequestToFile(updateRequestPath,
                info.manifestStartUrl(), info.scopeUrl(), info.name(), info.shortName(), maniefstId,
                info.appKey(), primaryIconUrl, primaryIconData, info.isIconAdaptive(),
                splashIconUrl, splashIconData, info.isSplashIconMaskable(), iconUrls, iconHashes,
                info.displayMode(), info.orientation(), info.toolbarColor(), info.backgroundColor(),
                shareTargetAction, shareTargetParamTitle, shareTargetParamText,
                shareTargetIsMethodPost, shareTargetIsEncTypeMultipart, shareTargetParamFileNames,
                shareTargetParamAccepts, shortcuts, shortcutIconData, info.manifestUrl(),
                info.webApkPackageName(), versionCode, isManifestStale,
                isAppIdentityUpdateSupported, updateReasonsArray, callback);
    }

    @NativeMethods
    interface Natives {
        public void storeWebApkUpdateRequestToFile(String updateRequestPath, String startUrl,
                String scope, String name, String shortName, String manifestId, String appKey,
                String primaryIconUrl, String primaryIconData, boolean isPrimaryIconMaskable,
                String splashIconUrl, String splashIconData, boolean isSplashIconMaskable,
                String[] iconUrls, String[] iconHashes, @DisplayMode.EnumType int displayMode,
                int orientation, long themeColor, long backgroundColor, String shareTargetAction,
                String shareTargetParamTitle, String shareTargetParamText,
                boolean shareTargetParamIsMethodPost, boolean shareTargetParamIsEncTypeMultipart,
                String[] shareTargetParamFileNames, Object[] shareTargetParamAccepts,
                String[][] shortcuts, byte[][] shortcutIconData, String manifestUrl,
                String webApkPackage, int webApkVersion, boolean isManifestStale,
                boolean isAppIdentityUpdateSupported, int[] updateReasons,
                Callback<Boolean> callback);
        public void updateWebApkFromFile(String updateRequestPath, WebApkUpdateCallback callback);
        public int getWebApkTargetShellVersion();
    }
}
