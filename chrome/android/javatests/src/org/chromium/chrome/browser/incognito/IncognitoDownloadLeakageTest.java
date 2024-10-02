// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.incognito;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import android.os.Environment;
import android.view.View;

import androidx.test.InstrumentationRegistry;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.LargeTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.params.ParameterAnnotations.UseMethodParameter;
import org.chromium.base.test.params.ParameterAnnotations.UseRunnerDelegate;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.customtabs.IncognitoCustomTabActivityTestRule;
import org.chromium.chrome.browser.download.DownloadItem;
import org.chromium.chrome.browser.download.DownloadManagerService;
import org.chromium.chrome.browser.download.DownloadPromptStatus;
import org.chromium.chrome.browser.download.items.OfflineContentAggregatorFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileKey;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.components.offline_items_collection.ContentId;
import org.chromium.components.offline_items_collection.OfflineContentProvider;
import org.chromium.components.offline_items_collection.OfflineItem;
import org.chromium.components.offline_items_collection.OfflineItemState;
import org.chromium.components.offline_items_collection.UpdateDelta;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.browser.test.util.TouchCommon;
import org.chromium.net.test.EmbeddedTestServer;

import java.io.File;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;

/**
 * This test class checks download entries leakage between all different pairs of Activity types
 * with a constraint that one of the interacting activity must be either Incognito Tab or Incognito
 * CCT.
 */
@RunWith(ParameterizedRunner.class)
@UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@EnableFeatures({ChromeFeatureList.CCT_INCOGNITO})
// TODO(crbug.com/1360906) remove INCOGNITO_DOWNLOADS_WARNING from the disabled features and fix
// the test accordingly
@DisableFeatures({ChromeFeatureList.INCOGNITO_DOWNLOADS_WARNING})
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        "enable-features=UseDownloadOfflineContentProvider"})
public class IncognitoDownloadLeakageTest {
    private String mDownloadTestPage;
    private final String mDownloadedFileName = "test.gzip";

    private static final File DOWNLOAD_DIRECTORY =
            Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DOWNLOADS);
    private static final String DOWNLOAD_TEST_PAGE_PATH =
            "/chrome/test/data/android/download/get.html";

    private EmbeddedTestServer mTestServer;
    private CallbackHelper mHttpDownloadFinishedCallback = new CallbackHelper();
    private CallbackHelper mRetrieveDownloadsCallback = new CallbackHelper();

    private List<DownloadItem> mOffTheRecordDownloadItems;
    private List<DownloadItem> mRegularDownloadItems;

    private DownloadManagerService.DownloadObserver mTestDownloadManagerServiceObserver =
            new DownloadManagerService.DownloadObserver() {
                @Override
                public void onAllDownloadsRetrieved(
                        List<DownloadItem> list, ProfileKey profileKey) {
                    if (profileKey.isOffTheRecord()) {
                        mOffTheRecordDownloadItems = new ArrayList<DownloadItem>(list);
                    } else {
                        mRegularDownloadItems = new ArrayList<DownloadItem>(list);
                    }
                    mRetrieveDownloadsCallback.notifyCalled();
                }

                @Override
                public void onDownloadItemCreated(DownloadItem item) {}

                @Override
                public void onDownloadItemUpdated(DownloadItem item) {}

                @Override
                public void onDownloadItemRemoved(String guid) {}

                @Override
                public void onAddOrReplaceDownloadSharedPreferenceEntry(ContentId id) {}
            };

    private OfflineContentProvider.Observer mTestDownloadBackendObserver =
            new OfflineContentProvider.Observer() {
                @Override
                public void onItemsAdded(List<OfflineItem> items) {}

                @Override
                public void onItemRemoved(ContentId id) {}

                @Override
                public void onItemUpdated(OfflineItem item, UpdateDelta updateDelta) {
                    if (item.state == OfflineItemState.COMPLETE) {
                        mHttpDownloadFinishedCallback.notifyCalled();
                    }
                }
            };

    @Rule
    public ChromeTabbedActivityTestRule mChromeActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public IncognitoCustomTabActivityTestRule mCustomTabActivityTestRule =
            new IncognitoCustomTabActivityTestRule();

    @Before
    public void setUp() throws Exception {
        mTestServer = EmbeddedTestServer.createAndStartServer(
                ApplicationProvider.getApplicationContext());
        mDownloadTestPage = mTestServer.getURL(DOWNLOAD_TEST_PAGE_PATH);

        // Ensuring native is initialized before we access the CCT_INCOGNITO feature flag.
        IncognitoDataTestUtils.fireAndWaitForCctWarmup();
        assertTrue(ChromeFeatureList.isEnabled(ChromeFeatureList.CCT_INCOGNITO));

        // Download related setUp steps.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            DownloadManagerService.getDownloadManagerService().addDownloadObserver(
                    mTestDownloadManagerServiceObserver);
            OfflineContentAggregatorFactory.get().addObserver(mTestDownloadBackendObserver);
        });

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // This skips the download prompt dialog for tests.
            UserPrefs.get(Profile.getLastUsedRegularProfile())
                    .setInteger(Pref.PROMPT_FOR_DOWNLOAD_ANDROID, DownloadPromptStatus.DONT_SHOW);
        });

        deleteFilesInDownloadDirectory(mDownloadedFileName);
    }

    @After
    public void tearDown() {
        deleteFilesInDownloadDirectory(mDownloadedFileName);
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> DownloadManagerService.getDownloadManagerService()
                                   .removeDownloadObserver(mTestDownloadManagerServiceObserver));
        TestThreadUtils.runOnUiThreadBlocking(
                () -> IncognitoDataTestUtils.closeTabs(mChromeActivityTestRule));
        mTestServer.stopAndDestroyServer();
    }

    private boolean hasFileDownloaded(String downloadedFileName) {
        File file = new File(DOWNLOAD_DIRECTORY, downloadedFileName);
        return file.exists();
    }

    public void deleteFilesInDownloadDirectory(String... filenames) {
        for (String filename : filenames) {
            final File fileToDelete = new File(DOWNLOAD_DIRECTORY, filename);
            if (fileToDelete.exists()) {
                Assert.assertTrue(
                        "Could not delete file " + filename + " that would block this test",
                        fileToDelete.delete());
            }
        }
    }

    private void waitForDownloadToFinish() throws TimeoutException {
        int currentCallCount = mHttpDownloadFinishedCallback.getCallCount();
        mHttpDownloadFinishedCallback.waitForCallback(
                currentCallCount, currentCallCount + 1, 5, TimeUnit.SECONDS);
    }

    private void startDownload(Tab tab) throws TimeoutException {
        View currentView = tab.getView();
        if (!currentView.hasFocus()) {
            TouchCommon.singleClickView(currentView);
        }
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        TouchCommon.singleClickView(currentView);
        waitForDownloadToFinish();
    }

    @Test
    @LargeTest
    @UseMethodParameter(IncognitoDataTestUtils.TestParams.IncognitoToRegular.class)
    public void testIncognitoDowloadEntriesNotVisibleInRegular(
            String incognitoActivityType, String regularActivityType) throws Exception {
        IncognitoDataTestUtils.ActivityType incognitoActivity =
                IncognitoDataTestUtils.ActivityType.valueOf(incognitoActivityType);

        IncognitoDataTestUtils.ActivityType regularActivity =
                IncognitoDataTestUtils.ActivityType.valueOf(regularActivityType);

        // Initiate download from incognito context.
        Tab incognitoTab = incognitoActivity.launchUrl(
                mChromeActivityTestRule, mCustomTabActivityTestRule, mDownloadTestPage);
        startDownload(incognitoTab);

        // Check the file is downloaded
        assertTrue(hasFileDownloaded(mDownloadedFileName));

        // Retrieve downloads from the incognito DownloadService.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Profile profile = IncognitoUtils.getIncognitoProfileFromWindowAndroid(
                    incognitoTab.getWindowAndroid());
            DownloadManagerService.getDownloadManagerService().getAllDownloads(
                    profile.getOTRProfileID());
        });
        mRetrieveDownloadsCallback.waitForCallback(0);

        // One download item should be visible.
        assertEquals(1, mOffTheRecordDownloadItems.size());

        // Loads "about:blank" in the regular Activity.
        regularActivity.launchUrl(
                mChromeActivityTestRule, mCustomTabActivityTestRule, "about:blank");

        // Retrieve downloads for regular Profile.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> DownloadManagerService.getDownloadManagerService().getAllDownloads(null));
        mRetrieveDownloadsCallback.waitForCallback(1);

        // No download entries should leak from incognito to regular.
        assertEquals(0, mRegularDownloadItems.size());
    }

    @Test
    @LargeTest
    @UseMethodParameter(IncognitoDataTestUtils.TestParams.IncognitoToIncognito.class)
    public void testIncognitoDowloadEntriesNotVisibleInAnotherIncognito(
            String incognitoActivityType1, String incognitoActivityType2) throws Exception {
        IncognitoDataTestUtils.ActivityType incognitoActivity1 =
                IncognitoDataTestUtils.ActivityType.valueOf(incognitoActivityType1);

        IncognitoDataTestUtils.ActivityType incognitoActivity2 =
                IncognitoDataTestUtils.ActivityType.valueOf(incognitoActivityType2);

        // Initiate download from the first incognito Activity. This returns either a CCT or a
        // Chrome incognito tab.
        Tab incognitoTab1 = incognitoActivity1.launchUrl(
                mChromeActivityTestRule, mCustomTabActivityTestRule, mDownloadTestPage);
        startDownload(incognitoTab1);

        // Check the file is downloaded
        assertTrue(hasFileDownloaded(mDownloadedFileName));

        // Retrieve downloads from the incognito DownloadService.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Profile profile = IncognitoUtils.getIncognitoProfileFromWindowAndroid(
                    incognitoTab1.getWindowAndroid());
            DownloadManagerService.getDownloadManagerService().getAllDownloads(
                    profile.getOTRProfileID());
        });
        mRetrieveDownloadsCallback.waitForCallback(0);

        // One download item should be visible.
        assertEquals(1, mOffTheRecordDownloadItems.size());

        // Load "about:blank" in the second incognito Activity.
        Tab incognitoTab2 = incognitoActivity2.launchUrl(
                mChromeActivityTestRule, mCustomTabActivityTestRule, "about:blank");

        // Retrieve downloads for the second incognito profile.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Profile profile = IncognitoUtils.getIncognitoProfileFromWindowAndroid(
                    incognitoTab2.getWindowAndroid());
            DownloadManagerService.getDownloadManagerService().getAllDownloads(
                    profile.getOTRProfileID());
        });
        mRetrieveDownloadsCallback.waitForCallback(1);

        // No download entries should leak to/from an incognito CCT.
        assertEquals(0, mOffTheRecordDownloadItems.size());
    }
}
