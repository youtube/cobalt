// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import android.Manifest.permission;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.download.DownloadCollectionBridge;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.permissions.AndroidPermissionDelegate;
import org.chromium.url.GURL;

/**
 * Java counterpart of android DownloadController. Owned by native.
 */
public class DownloadController {
    /**
     * Class for notifying download events to other classes.
     */
    public interface Observer {
        /**
         * Notify the host application that a download is finished.
         * @param downloadInfo Information about the completed download.
         */
        void onDownloadCompleted(final DownloadInfo downloadInfo);

        /**
         * Notify the host application that a download is in progress.
         * @param downloadInfo Information about the in-progress download.
         */
        void onDownloadUpdated(final DownloadInfo downloadInfo);

        /**
         * Notify the host application that a download is cancelled.
         * @param downloadInfo Information about the cancelled download.
         */
        void onDownloadCancelled(final DownloadInfo downloadInfo);

        /**
         * Notify the host application that a download is interrupted.
         * @param downloadInfo Information about the completed download.
         * @param isAutoResumable Download can be auto resumed when network becomes available.
         */
        void onDownloadInterrupted(final DownloadInfo downloadInfo, boolean isAutoResumable);
    }

    private static Observer sObserver;

    public static void setDownloadNotificationService(Observer observer) {
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.DOWNLOAD_OFFLINE_CONTENT_PROVIDER)) {
            return;
        }

        sObserver = observer;
    }

    /**
     * Notifies the download delegate that a download completed and passes along info about the
     * download. This can be either a POST download or a GET download with authentication.
     */
    @CalledByNative
    private static void onDownloadCompleted(DownloadInfo downloadInfo) {
        DownloadMetrics.recordDownloadDirectoryType(downloadInfo.getFilePath());
        MediaStoreHelper.addImageToGalleryOnSDCard(
                downloadInfo.getFilePath(), downloadInfo.getMimeType());

        if (sObserver == null) return;
        sObserver.onDownloadCompleted(downloadInfo);
    }

    /**
     * Notifies the download delegate that a download completed and passes along info about the
     * download. This can be either a POST download or a GET download with authentication.
     */
    @CalledByNative
    private static void onDownloadInterrupted(DownloadInfo downloadInfo, boolean isAutoResumable) {
        if (sObserver == null) return;
        sObserver.onDownloadInterrupted(downloadInfo, isAutoResumable);
    }

    /**
     * Called when a download was cancelled.
     */
    @CalledByNative
    private static void onDownloadCancelled(DownloadInfo downloadInfo) {
        if (sObserver == null) return;
        sObserver.onDownloadCancelled(downloadInfo);
    }

    /**
     * Notifies the download delegate about progress of a download. Downloads that use Chrome
     * network stack use custom notification to display the progress of downloads.
     */
    @CalledByNative
    private static void onDownloadUpdated(DownloadInfo downloadInfo) {
        if (sObserver == null) return;
        sObserver.onDownloadUpdated(downloadInfo);
    }

    /**
     * Returns whether file access is allowed.
     *
     * @return true if allowed, or false otherwise.
     */
    @CalledByNative
    private static boolean hasFileAccess(WindowAndroid windowAndroid) {
        if (DownloadCollectionBridge.supportsDownloadCollection()) return true;
        AndroidPermissionDelegate delegate = windowAndroid;
        return delegate == null ? false : delegate.hasPermission(permission.WRITE_EXTERNAL_STORAGE);
    }

    /**
     * Requests the storage permission. This should be called from the native code.
     * @param callbackId ID of native callback to notify the result.
     * @param windowAndroid The {@link WindowAndroid} associated with the tab.
     */
    @CalledByNative
    private static void requestFileAccess(final long callbackId, WindowAndroid windowAndroid) {
        if (windowAndroid == null) {
            DownloadControllerJni.get().onAcquirePermissionResult(
                    callbackId, /*granted=*/false, /*permissionToUpdate=*/null);
            return;
        }
        FileAccessPermissionHelper.requestFileAccessPermissionHelper(windowAndroid, result -> {
            DownloadControllerJni.get().onAcquirePermissionResult(
                    callbackId, result.first, result.second);
        });
    }

    /**
     * Enqueue a request to download a file using Android DownloadManager.
     * @param url Url to download.
     * @param userAgent User agent to use.
     * @param contentDisposition Content disposition of the request.
     * @param mimeType MIME type.
     * @param cookie Cookie to use.
     * @param referrer Referrer to use.
     */
    @CalledByNative
    private static void enqueueAndroidDownloadManagerRequest(GURL url, String userAgent,
            String fileName, String mimeType, String cookie, GURL referrer) {
        DownloadInfo downloadInfo = new DownloadInfo.Builder()
                .setUrl(url)
                .setUserAgent(userAgent)
                .setFileName(fileName)
                .setMimeType(mimeType)
                .setCookie(cookie)
                .setReferrer(referrer)
                .setIsGETRequest(true)
                .build();
        enqueueDownloadManagerRequest(downloadInfo);
    }

    /**
     * Enqueue a request to download a file using Android DownloadManager.
     *
     * @param info Download information about the download.
     */
    static void enqueueDownloadManagerRequest(final DownloadInfo info) {
        DownloadManagerService.getDownloadManagerService().enqueueNewDownload(
                new DownloadItem(true, info), true);
    }

    @CalledByNative
    private static void onDownloadStarted() {}

    @NativeMethods
    interface Natives {
        void onAcquirePermissionResult(long callbackId, boolean granted, String permissionToUpdate);
    }
}
