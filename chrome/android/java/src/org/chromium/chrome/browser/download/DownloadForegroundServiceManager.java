// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import static org.chromium.chrome.browser.download.DownloadSnackbarController.INVALID_NOTIFICATION_ID;

import android.app.Notification;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.os.Build.VERSION;
import android.os.Build.VERSION_CODES;
import android.os.Handler;
import android.os.IBinder;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.chrome.browser.download.DownloadNotificationService.DownloadStatus;
import org.chromium.chrome.browser.notifications.NotificationUmaTracker;
import org.chromium.chrome.browser.notifications.NotificationWrapperBuilderFactory;
import org.chromium.chrome.browser.notifications.channels.ChromeChannelDefinitions;
import org.chromium.components.browser_ui.notifications.NotificationMetadata;
import org.chromium.components.browser_ui.notifications.NotificationWrapperBuilder;

import java.util.HashMap;
import java.util.Iterator;
import java.util.Map;

/**
 * Manager to stop and start the foreground service associated with downloads.
 */
public class DownloadForegroundServiceManager {
    protected static class DownloadUpdate {
        int mNotificationId;
        Notification mNotification;
        @DownloadNotificationService.DownloadStatus
        int mDownloadStatus;
        Context mContext;

        DownloadUpdate(int notificationId, @Nullable Notification notification,
                @DownloadNotificationService.DownloadStatus int downloadStatus, Context context) {
            mNotificationId = notificationId;
            mNotification = notification;
            mDownloadStatus = downloadStatus;
            mContext = context;
        }
    }

    private static final String TAG = "DownloadFg";
    // Delay to ensure start/stop foreground doesn't happen too quickly (b/74236718).
    private static final int WAIT_TIME_MS = 200;

    // Variables used to ensure start/stop foreground doesn't happen too quickly (b/74236718).
    private final Handler mHandler = new Handler();
    private final Runnable mMaybeStopServiceRunnable = new Runnable() {
        @Override
        public void run() {
            Log.w(TAG, "Checking if delayed stopAndUnbindService needs to be resolved.");
            mStopServiceDelayed = false;
            processDownloadUpdateQueue(false /* not isProcessingPending */);
            mHandler.removeCallbacks(mMaybeStopServiceRunnable);
            Log.w(TAG, "Done checking if delayed stopAndUnbindService needs to be resolved.");
        }
    };
    private boolean mStopServiceDelayed;

    private int mPinnedNotificationId = INVALID_NOTIFICATION_ID;

    // This is true when context.bindService has been called and before context.unbindService.
    private boolean mIsServiceBound;

    // Whether startForeground() is called. startForeground() must be called in 5 seconds after the
    // service is started.
    private boolean mStartForegroundCalled;

    // This is non-null when onServiceConnected has been called (aka service is active).
    private DownloadForegroundServiceImpl mBoundService;

    @VisibleForTesting
    final Map<Integer, DownloadUpdate> mDownloadUpdateQueue = new HashMap<>();

    private static <T> void checkNotNull(T reference) {
        if (reference == null) {
            throw new NullPointerException();
        }
    }

    public DownloadForegroundServiceManager() {}

    /**
     * Updates download notification status. In progress notification will have a foreground service
     * associated. If all notifications are not in progress, foreground service will stop.
     * @param context Android {@link Context}.
     * @param downloadStatus Download status. In progress notification will have a foreground
     *         service.
     * @param notificationId The notification id.
     * @param notification The notification associated with the id. Can be null if {@link
     *         DownloadNotificationService} tries to cancel a notification.
     */
    public void updateDownloadStatus(Context context,
            @DownloadNotificationService.DownloadStatus int downloadStatus, int notificationId,
            @Nullable Notification notification) {
        if (downloadStatus != DownloadNotificationService.DownloadStatus.IN_PROGRESS) {
            Log.w(TAG,
                    "updateDownloadStatus status: " + downloadStatus + ", id: " + notificationId);
        }
        mDownloadUpdateQueue.put(notificationId,
                new DownloadUpdate(notificationId, notification, downloadStatus, context));
        processDownloadUpdateQueue(false /* not isProcessingPending */);
    }

    /**
     * Process the notification queue for all cases and initiate any needed actions.
     * In the happy path, the logic should be:
     * bindAndStartService -> startOrUpdateForegroundService -> stopAndUnbindService.
     * @param isProcessingPending Whether the call was made to process pending notifications that
     *                            have accumulated in the queue during the startup process or if it
     *                            was made based on during a basic update.
     */
    @VisibleForTesting
    void processDownloadUpdateQueue(boolean isProcessingPending) {
        DownloadUpdate downloadUpdate = findInterestingDownloadUpdate();
        if (downloadUpdate == null) return;
        // If foreground service is disallowed, don't handle active download updates. Only stopping
        // the foreground service is allowed.
        if (isActive(downloadUpdate.mDownloadStatus) && !canStartForeground()) return;

        // When nothing has been initialized, just bind the service.
        if (!mIsServiceBound) {
            // If the download update is not active at the onset, don't even start the service!
            if (!isActive(downloadUpdate.mDownloadStatus)) {
                cleanDownloadUpdateQueue();
                return;
            }
            startAndBindService(downloadUpdate.mContext);
            return;
        }

        // Skip everything that happens while waiting for startup.
        if (mBoundService == null) return;

        // In the pending case, start foreground with specific notificationId and notification.
        if (isProcessingPending) {
            Log.w(TAG, "Starting service with type " + downloadUpdate.mDownloadStatus);
            startOrUpdateForegroundService(downloadUpdate);

            // Post a delayed task to eventually check to see if service needs to be stopped.
            postMaybeStopServiceRunnable();
        }

        // If the selected downloadUpdate is not active, there are no active downloads left.
        // Stop the foreground service.
        // In the pending case, this will stop the foreground immediately after it was started.
        if (!isActive(downloadUpdate.mDownloadStatus)) {
            // Only stop the service if not waiting for delay (ie. WAIT_TIME_MS has transpired).
            if (!mStopServiceDelayed) {
                stopAndUnbindService(downloadUpdate.mDownloadStatus);
                cleanDownloadUpdateQueue();
            } else {
                Log.w(TAG, "Delaying call to stopAndUnbindService.");
            }
            return;
        }

        // Make sure the pinned notification is still active, if not, update.
        if (mDownloadUpdateQueue.get(mPinnedNotificationId) == null
                || !isActive(mDownloadUpdateQueue.get(mPinnedNotificationId).mDownloadStatus)) {
            startOrUpdateForegroundService(downloadUpdate);
        }

        // Clear out inactive download updates in queue if there is at least one active download.
        cleanDownloadUpdateQueue();
    }

    /** Helper code to process download update queue. */

    @Nullable
    private DownloadUpdate findInterestingDownloadUpdate() {
        Iterator<Map.Entry<Integer, DownloadUpdate>> entries =
                mDownloadUpdateQueue.entrySet().iterator();
        while (entries.hasNext()) {
            Map.Entry<Integer, DownloadUpdate> entry = entries.next();
            // Return an active entry if possible.
            if (isActive(entry.getValue().mDownloadStatus)) return entry.getValue();
            // If there are no active entries, just return the last entry.
            if (!entries.hasNext()) return entry.getValue();
        }
        // If there's no entries, return null.
        return null;
    }

    private boolean isActive(@DownloadNotificationService.DownloadStatus int downloadStatus) {
        return downloadStatus == DownloadNotificationService.DownloadStatus.IN_PROGRESS;
    }

    private void cleanDownloadUpdateQueue() {
        Iterator<Map.Entry<Integer, DownloadUpdate>> entries =
                mDownloadUpdateQueue.entrySet().iterator();
        while (entries.hasNext()) {
            Map.Entry<Integer, DownloadUpdate> entry = entries.next();
            // Remove entry that is not active or pinned.
            if (!isActive(entry.getValue().mDownloadStatus)
                    && entry.getValue().mNotificationId != mPinnedNotificationId) {
                entries.remove();
            }
        }
    }

    /** Helper code to bind service. */

    @VisibleForTesting
    void startAndBindService(Context context) {
        Log.w(TAG, "startAndBindService");
        mIsServiceBound = true;
        mStartForegroundCalled = false;
        startAndBindServiceInternal(context);
    }

    @VisibleForTesting
    void startAndBindServiceInternal(Context context) {
        DownloadForegroundServiceImpl.startDownloadForegroundService(context);
        context.bindService(new Intent(context, DownloadForegroundService.class), mConnection,
                Context.BIND_AUTO_CREATE);
    }

    private final ServiceConnection mConnection = new ServiceConnection() {
        @Override
        public void onServiceConnected(ComponentName className, IBinder service) {
            Log.w(TAG, "onServiceConnected");
            if (!(service instanceof DownloadForegroundServiceImpl.LocalBinder)) {
                Log.w(TAG,
                        "Not from DownloadNotificationService, do not connect."
                                + " Component name: " + className);
                return;
            }
            mBoundService = ((DownloadForegroundServiceImpl.LocalBinder) service).getService();
            DownloadForegroundServiceObservers.addObserver(
                    DownloadNotificationServiceObserver.class);
            processDownloadUpdateQueue(true /* isProcessingPending */);
        }

        @Override
        public void onServiceDisconnected(ComponentName componentName) {
            Log.w(TAG, "onServiceDisconnected");
            mBoundService = null;
        }
    };

    /** Helper code to start or update foreground service. */

    @VisibleForTesting
    void startOrUpdateForegroundService(DownloadUpdate update) {
        Log.w(TAG,
                "startOrUpdateForegroundService id: " + update.mNotificationId
                        + ", startForeground() Called: " + mStartForegroundCalled);

        int notificationId = update.mNotificationId;
        Notification notification = update.mNotification;

        // On O+, we must call startForeground or Android will crash. If the last update
        // is DownloadStatus.CANCELLED, then create an empty notification. See crbug.com/1121096.
        // Notices the empty notification will be cancelled immediately in
        // DownloadNotificationService afterward.
        if (VERSION.SDK_INT >= VERSION_CODES.O && notification == null && !mStartForegroundCalled) {
            assert update.mDownloadStatus == DownloadStatus.CANCELLED;
            notification = createEmptyNotification(notificationId, update.mContext);
        }

        if (mBoundService != null && notificationId != INVALID_NOTIFICATION_ID
                && notification != null) {
            // If there was an originally pinned notification, get its id and notification.
            DownloadUpdate downloadUpdate = mDownloadUpdateQueue.get(mPinnedNotificationId);
            Notification oldNotification =
                    (downloadUpdate == null) ? null : downloadUpdate.mNotification;

            boolean killOldNotification = downloadUpdate != null
                    && downloadUpdate.mDownloadStatus == DownloadStatus.CANCELLED;

            // Start service and handle notifications.
            mBoundService.startOrUpdateForegroundService(notificationId, notification,
                    mPinnedNotificationId, oldNotification, killOldNotification);
            mStartForegroundCalled = true;

            // After the service has been started and the notification handled, change stored id.
            mPinnedNotificationId = notificationId;
        }
    }

    // Creates an empty notification to feed to startForeground().
    private Notification createEmptyNotification(int notificationId, Context context) {
        NotificationWrapperBuilder builder =
                NotificationWrapperBuilderFactory.createNotificationWrapperBuilder(
                        ChromeChannelDefinitions.ChannelId.DOWNLOADS,
                        new NotificationMetadata(
                                NotificationUmaTracker.SystemNotificationType.DOWNLOAD_FILES, null,
                                notificationId));
        return builder.build();
    }

    /** Helper code to stop and unbind service. */

    @VisibleForTesting
    void stopAndUnbindService(@DownloadNotificationService.DownloadStatus int downloadStatus) {
        Log.w(TAG, "stopAndUnbindService status: " + downloadStatus);
        checkNotNull(mBoundService);
        mIsServiceBound = false;

        @DownloadForegroundServiceImpl.StopForegroundNotification
        int stopForegroundNotification;
        if (downloadStatus == DownloadNotificationService.DownloadStatus.CANCELLED) {
            stopForegroundNotification =
                    DownloadForegroundServiceImpl.StopForegroundNotification.KILL;
        } else {
            stopForegroundNotification =
                    DownloadForegroundServiceImpl.StopForegroundNotification.DETACH;
        }

        DownloadUpdate downloadUpdate = mDownloadUpdateQueue.get(mPinnedNotificationId);
        Notification oldNotification =
                (downloadUpdate == null) ? null : downloadUpdate.mNotification;

        stopAndUnbindServiceInternal(
                stopForegroundNotification, mPinnedNotificationId, oldNotification);

        mBoundService = null;
        mStartForegroundCalled = false;

        mPinnedNotificationId = INVALID_NOTIFICATION_ID;
    }

    @VisibleForTesting
    void stopAndUnbindServiceInternal(
            @DownloadForegroundServiceImpl.StopForegroundNotification int stopForegroundStatus,
            int pinnedNotificationId, Notification pinnedNotification) {
        mBoundService.stopDownloadForegroundService(
                stopForegroundStatus, pinnedNotificationId, pinnedNotification);
        ContextUtils.getApplicationContext().unbindService(mConnection);

        DownloadForegroundServiceObservers.removeObserver(
                DownloadNotificationServiceObserver.class);
    }

    /** Helper code for testing. */

    @VisibleForTesting
    void setBoundService(DownloadForegroundServiceImpl service) {
        mBoundService = service;
    }

    // Allow testing methods to skip posting the delayed runnable.
    @VisibleForTesting
    void postMaybeStopServiceRunnable() {
        mHandler.removeCallbacks(mMaybeStopServiceRunnable);
        mHandler.postDelayed(mMaybeStopServiceRunnable, WAIT_TIME_MS);
        mStopServiceDelayed = true;
    }

    /**
     * @return Whether startForeground() is allowed to be called.
     */
    private boolean canStartForeground() {
        if (VERSION.SDK_INT < VERSION_CODES.S) return true;
        // If foreground service is started, startForeground() must be called.
        return ApplicationStatus.hasVisibleActivities()
                || (mIsServiceBound && !mStartForegroundCalled);
    }
}
