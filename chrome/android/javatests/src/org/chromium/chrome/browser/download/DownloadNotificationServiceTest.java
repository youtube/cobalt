// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;

import org.chromium.base.test.UiThreadTest;
import org.chromium.base.test.params.ParameterAnnotations.ClassParameter;
import org.chromium.base.test.params.ParameterAnnotations.UseRunnerDelegate;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.profiles.OTRProfileID;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.offline_items_collection.ContentId;
import org.chromium.components.offline_items_collection.LegacyHelpers;
import org.chromium.components.offline_items_collection.OfflineItem.Progress;
import org.chromium.components.offline_items_collection.OfflineItemProgressUnit;
import org.chromium.components.offline_items_collection.PendingState;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.url.GURL;

import java.util.Arrays;
import java.util.List;
import java.util.UUID;

/**
 * Tests of {@link DownloadNotificationService}.
 */
@RunWith(ParameterizedRunner.class)
@UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@Features.DisableFeatures({ChromeFeatureList.DOWNLOAD_OFFLINE_CONTENT_PROVIDER})
@Batch(Batch.UNIT_TESTS)
public class DownloadNotificationServiceTest {
    private static final ContentId ID1 =
            LegacyHelpers.buildLegacyContentId(false, UUID.randomUUID().toString());
    private static final ContentId ID2 =
            LegacyHelpers.buildLegacyContentId(false, UUID.randomUUID().toString());
    private static final ContentId ID3 =
            LegacyHelpers.buildLegacyContentId(false, UUID.randomUUID().toString());

    @ClassParameter
    private static List<ParameterSet> sClassParams = Arrays.asList(
            new ParameterSet().value(false, false).name("GenericStatus"),
            new ParameterSet().value(true, false).name("EnableDescriptivePendingStatusOnly"),
            new ParameterSet().value(false, true).name("EnableDescriptiveFailStatusOnly"),
            new ParameterSet().value(true, true).name("EnableDescriptivePendingAndFailStatus"));

    @Rule
    public TestRule mFeaturesProcessor = new Features.JUnitProcessor();

    private MockDownloadNotificationService mDownloadNotificationService;
    private DownloadForegroundServiceManagerTest
            .MockDownloadForegroundServiceManager mDownloadForegroundServiceManager;
    private DownloadSharedPreferenceHelper mDownloadSharedPreferenceHelper;

    private final boolean mEnableOfflinePagesDescriptivePendingStatus;
    private final boolean mEnableOfflinePagesDescriptiveFailStatus;
    private OTRProfileID mPrimaryOTRProfileID = OTRProfileID.getPrimaryOTRProfileID();

    public DownloadNotificationServiceTest(boolean enableOfflinePagesDescriptivePendingStatus,
            boolean enableOfflinePagesDescriptiveFailStatus) {
        mEnableOfflinePagesDescriptivePendingStatus = enableOfflinePagesDescriptivePendingStatus;
        mEnableOfflinePagesDescriptiveFailStatus = enableOfflinePagesDescriptiveFailStatus;
    }

    private static DownloadSharedPreferenceEntry buildEntryStringWithGuid(ContentId contentId,
            int notificationId, String fileName, boolean metered, boolean autoResume) {
        return new DownloadSharedPreferenceEntry(
                contentId, notificationId, null, metered, fileName, autoResume, false);
    }

    @Before
    public void setUp() {
        if (mEnableOfflinePagesDescriptivePendingStatus) {
            Features.getInstance().enable(
                    ChromeFeatureList.OFFLINE_PAGES_DESCRIPTIVE_PENDING_STATUS);
        } else {
            Features.getInstance().disable(
                    ChromeFeatureList.OFFLINE_PAGES_DESCRIPTIVE_PENDING_STATUS);
        }
        if (mEnableOfflinePagesDescriptiveFailStatus) {
            Features.getInstance().enable(ChromeFeatureList.OFFLINE_PAGES_DESCRIPTIVE_FAIL_STATUS);
        } else {
            Features.getInstance().disable(ChromeFeatureList.OFFLINE_PAGES_DESCRIPTIVE_FAIL_STATUS);
        }
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            DownloadNotificationService.clearResumptionAttemptLeft();
            mDownloadNotificationService = new MockDownloadNotificationService();
            mDownloadForegroundServiceManager =
                    new DownloadForegroundServiceManagerTest.MockDownloadForegroundServiceManager();
            mDownloadNotificationService.setDownloadForegroundServiceManager(
                    mDownloadForegroundServiceManager);
            mDownloadSharedPreferenceHelper = DownloadSharedPreferenceHelper.getInstance();
        });
    }

    @After
    public void tearDown() {
        DownloadNotificationService.clearResumptionAttemptLeft();
        SharedPreferencesManager.getInstance().removeKey(
                ChromePreferenceKeys.DOWNLOAD_PENDING_DOWNLOAD_NOTIFICATIONS);
    }

    @Test
    @SmallTest
    @UiThreadTest
    @Feature({"Download"})
    public void testBasicDownloadFlow() {
        // Download is in-progress.
        mDownloadNotificationService.notifyDownloadProgress(ID1, "test",
                new Progress(1, 100L, OfflineItemProgressUnit.PERCENTAGE), 100L, 1L, 1L,
                mPrimaryOTRProfileID, true, false, null, null, false);
        mDownloadForegroundServiceManager.onServiceConnected();

        assertEquals(1, mDownloadNotificationService.getNotificationIds().size());
        int notificationId1 = mDownloadNotificationService.getLastNotificationId();
        assertTrue(mDownloadForegroundServiceManager.mDownloadUpdateQueue.containsKey(
                notificationId1));
        assertTrue(mDownloadNotificationService.mDownloadsInProgress.contains(ID1));

        // Download is paused.
        mDownloadNotificationService.notifyDownloadPaused(ID1, "test", true /* isResumable*/,
                false /* isAutoResumable */, mPrimaryOTRProfileID, false, null, null, false, false,
                false, PendingState.NOT_PENDING);

        assertEquals(1, mDownloadNotificationService.getNotificationIds().size());
        assertFalse(mDownloadForegroundServiceManager.mDownloadUpdateQueue.containsKey(
                notificationId1));
        assertFalse(mDownloadNotificationService.mDownloadsInProgress.contains(ID1));

        // Download is again in-progress.
        mDownloadNotificationService.notifyDownloadProgress(ID1, "test",
                new Progress(20, 100L, OfflineItemProgressUnit.PERCENTAGE), 100L, 1L, 1L,
                mPrimaryOTRProfileID, true, false, null, null, false);
        mDownloadForegroundServiceManager.onServiceConnected();

        assertEquals(1, mDownloadNotificationService.getNotificationIds().size());
        assertTrue(mDownloadForegroundServiceManager.mDownloadUpdateQueue.containsKey(
                notificationId1));
        assertTrue(mDownloadNotificationService.mDownloadsInProgress.contains(ID1));

        // Download is successful.
        mDownloadNotificationService.notifyDownloadSuccessful(ID1, "", "test", 1L,
                mPrimaryOTRProfileID, true, true, null, GURL.emptyGURL(), false, GURL.emptyGURL(),
                0);
        assertEquals(1, mDownloadNotificationService.getNotificationIds().size());
        assertFalse(mDownloadForegroundServiceManager.mDownloadUpdateQueue.containsKey(
                notificationId1));
        assertFalse(mDownloadNotificationService.mDownloadsInProgress.contains(ID1));
    }

    @Test
    @SmallTest
    @UiThreadTest
    @Feature({"Download"})
    public void testDownloadPendingAndCancelled() {
        // Download is in-progress.
        mDownloadNotificationService.notifyDownloadProgress(ID1, "test",
                new Progress(1, 100L, OfflineItemProgressUnit.PERCENTAGE), 100L, 1L, 1L,
                mPrimaryOTRProfileID, true, false, null, null, false);
        mDownloadForegroundServiceManager.onServiceConnected();

        assertEquals(1, mDownloadNotificationService.getNotificationIds().size());
        int notificationId1 = mDownloadNotificationService.getLastNotificationId();
        assertTrue(mDownloadForegroundServiceManager.mDownloadUpdateQueue.containsKey(
                notificationId1));
        assertTrue(mDownloadNotificationService.mDownloadsInProgress.contains(ID1));

        // Download is interrupted and now is pending.
        mDownloadNotificationService.notifyDownloadPaused(ID1, "test", true /* isResumable */,
                true /* isAutoResumable */, mPrimaryOTRProfileID, false, null, null, false, false,
                false, PendingState.PENDING_NETWORK);
        assertEquals(1, mDownloadNotificationService.getNotificationIds().size());
        assertTrue(mDownloadForegroundServiceManager.mDownloadUpdateQueue.containsKey(
                notificationId1));
        assertFalse(mDownloadNotificationService.mDownloadsInProgress.contains(ID1));

        // Download is cancelled.
        mDownloadNotificationService.notifyDownloadCanceled(ID1, false);

        assertEquals(0, mDownloadNotificationService.getNotificationIds().size());
        assertFalse(mDownloadForegroundServiceManager.mDownloadUpdateQueue.containsKey(
                notificationId1));
        assertFalse(mDownloadNotificationService.mDownloadsInProgress.contains(ID1));
    }

    @Test
    @SmallTest
    @UiThreadTest
    @Feature({"Download"})
    public void testDownloadInterruptedAndFailed() {
        // Download is in-progress.
        mDownloadNotificationService.notifyDownloadProgress(ID1, "test",
                new Progress(1, 100L, OfflineItemProgressUnit.PERCENTAGE), 100L, 1L, 1L,
                mPrimaryOTRProfileID, true, false, null, null, false);
        mDownloadForegroundServiceManager.onServiceConnected();

        assertEquals(1, mDownloadNotificationService.getNotificationIds().size());
        int notificationId1 = mDownloadNotificationService.getLastNotificationId();
        assertTrue(mDownloadForegroundServiceManager.mDownloadUpdateQueue.containsKey(
                notificationId1));
        assertTrue(mDownloadNotificationService.mDownloadsInProgress.contains(ID1));

        // Download is interrupted but because it is not resumable, fails.
        mDownloadNotificationService.notifyDownloadPaused(ID1, "test", false /* isResumable*/,
                true /* isAutoResumable */, mPrimaryOTRProfileID, false, null, null, false, false,
                false, PendingState.PENDING_NETWORK);
        assertEquals(1, mDownloadNotificationService.getNotificationIds().size());
        assertFalse(mDownloadForegroundServiceManager.mDownloadUpdateQueue.containsKey(
                notificationId1));
        assertFalse(mDownloadNotificationService.mDownloadsInProgress.contains(ID1));
    }
}
