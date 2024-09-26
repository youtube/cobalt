// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test.devui.util;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.contains;
import static org.hamcrest.Matchers.containsInAnyOrder;

import static org.chromium.android_webview.test.OnlyRunIn.ProcessMode.SINGLE_PROCESS;
import static org.chromium.android_webview.test.common.crash.CrashInfoEqualityMatcher.equalsTo;
import static org.chromium.android_webview.test.common.crash.CrashInfoTest.createCrashInfo;
import static org.chromium.android_webview.test.common.crash.CrashInfoTest.createHiddenCrashInfo;

import androidx.test.filters.SmallTest;

import org.json.JSONException;
import org.junit.After;
import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.android_webview.common.crash.CrashInfo;
import org.chromium.android_webview.common.crash.CrashInfo.UploadState;
import org.chromium.android_webview.common.crash.SystemWideCrashDirectories;
import org.chromium.android_webview.devui.util.CrashInfoLoader;
import org.chromium.android_webview.devui.util.WebViewCrashInfoCollector;
import org.chromium.android_webview.devui.util.WebViewCrashInfoCollector.CrashInfoLoadersFactory;
import org.chromium.android_webview.devui.util.WebViewCrashLogParser;
import org.chromium.android_webview.test.AwJUnit4ClassRunner;
import org.chromium.android_webview.test.OnlyRunIn;
import org.chromium.base.FileUtils;
import org.chromium.base.test.util.Batch;

import java.io.File;
import java.io.FileWriter;
import java.io.IOException;
import java.util.Arrays;
import java.util.List;

/**
 * Unit tests for WebViewCrashInfoCollector.
 */
@RunWith(AwJUnit4ClassRunner.class)
@OnlyRunIn(SINGLE_PROCESS) // These are unit tests
@Batch(Batch.UNIT_TESTS)
public class WebViewCrashInfoCollectorTest {
    private static class CrashInfoLoadersTestFactory extends CrashInfoLoadersFactory {
        private final List<CrashInfo> mCrashInfoList;

        public CrashInfoLoadersTestFactory(List<CrashInfo> crashInfoList) {
            mCrashInfoList = crashInfoList;
        }

        @Override
        public CrashInfoLoader[] create() {
            CrashInfoLoader loader = new CrashInfoLoader() {
                @Override
                public List<CrashInfo> loadCrashesInfo() {
                    return mCrashInfoList;
                }
            };
            return new CrashInfoLoader[] {loader};
        }
    }

    @After
    public void tearDown() {
        FileUtils.recursivelyDeleteFile(SystemWideCrashDirectories.getWebViewCrashLogDir(), null);
    }

    private static File writeJsonLogFile(CrashInfo crashInfo) throws IOException {
        File dir = SystemWideCrashDirectories.getOrCreateWebViewCrashLogDir();
        File jsonFile = File.createTempFile(crashInfo.localId, ".json", dir);
        FileWriter writer = new FileWriter(jsonFile);
        writer.write(crashInfo.serializeToJson());
        writer.close();
        return jsonFile;
    }

    private static CrashInfo getCrashFromJsonLogFile(String localId)
            throws IOException, JSONException {
        File logDir = SystemWideCrashDirectories.getOrCreateWebViewCrashLogDir();
        File[] logFiles = logDir.listFiles();
        for (File logFile : logFiles) {
            if (!logFile.isFile() || !logFile.getName().endsWith(".json")) continue;
            if (!logFile.getName().contains(localId)) continue;
            String jsonObject = WebViewCrashLogParser.readEntireFile(logFile);
            return CrashInfo.readFromJsonString(jsonObject);
        }
        return null;
    }

    /**
     * Test that merging {@code CrashInfo} that has the same {@code localID} works correctly.
     */
    @Test
    @SmallTest
    public void testMergeDuplicates() {
        List<CrashInfo> testList = Arrays.asList(
                createCrashInfo("xyz123", 112233445566L, null, -1, null, UploadState.PENDING),
                createCrashInfo(
                        "def789", -1, "55667788", 123344556677L, null, UploadState.UPLOADED),
                createCrashInfo("abc456", -1, null, -1, null, UploadState.PENDING),
                createCrashInfo("xyz123", 112233445566L, null, -1, "com.test.package", null),
                createCrashInfo("abc456", 445566778899L, null, -1, "org.test.package", null),
                createCrashInfo("abc456", -1, null, -1, null, null),
                createCrashInfo(
                        "xyz123", -1, "11223344", 223344556677L, null, UploadState.UPLOADED));
        List<CrashInfo> uniqueList = WebViewCrashInfoCollector.mergeDuplicates(testList);
        Assert.assertThat(uniqueList,
                containsInAnyOrder(equalsTo(createCrashInfo("abc456", 445566778899L, null, -1,
                                           "org.test.package", UploadState.PENDING)),
                        equalsTo(createCrashInfo("xyz123", 112233445566L, "11223344", 223344556677L,
                                "com.test.package", UploadState.UPLOADED)),
                        equalsTo(createCrashInfo("def789", -1, "55667788", 123344556677L, null,
                                UploadState.UPLOADED))));
    }

    /**
     * Test that merging hidden {@code CrashInfo} that has the same {@code localID} works correctly.
     */
    @Test
    @SmallTest
    public void testMergeDuplicatesAndIgnoreHidden() {
        List<CrashInfo> testList = Arrays.asList(
                createHiddenCrashInfo("xyz123", 112233445566L, null, -1, null, UploadState.PENDING),
                createCrashInfo(
                        "def789", -1, "55667788", 123344556677L, null, UploadState.UPLOADED),
                createHiddenCrashInfo("abc456", -1, null, -1, null, UploadState.PENDING),
                createCrashInfo("xyz123", 112233445566L, null, -1, "com.test.package", null),
                createCrashInfo("abc456", 445566778899L, null, -1, "org.test.package", null),
                createCrashInfo("abc456", -1, null, -1, null, null),
                createCrashInfo(
                        "xyz123", -1, "11223344", 223344556677L, null, UploadState.UPLOADED));
        List<CrashInfo> uniqueList = WebViewCrashInfoCollector.mergeDuplicates(testList);
        Assert.assertThat(uniqueList,
                containsInAnyOrder(equalsTo(createCrashInfo(
                        "def789", -1, "55667788", 123344556677L, null, UploadState.UPLOADED))));
    }

    /**
     * Test that sort method works correctly; it sorts by recent crashes first (capture time then
     * upload time).
     */
    @Test
    @SmallTest
    public void testSortByRecentCaptureTime() {
        List<CrashInfo> testList = Arrays.asList(
                createCrashInfo("xyz123", -1, "11223344", 123L, null, UploadState.UPLOADED),
                createCrashInfo("def789", 111L, "55667788", 100L, null, UploadState.UPLOADED),
                createCrashInfo("abc456", -1, null, -1, null, UploadState.PENDING),
                createCrashInfo("ghijkl", 112L, null, -1, "com.test.package", null),
                createCrashInfo("abc456", 112L, null, 112L, "org.test.package", null),
                createCrashInfo(null, 100, "11223344", -1, "com.test.package", null),
                createCrashInfo("abc123", 100, null, -1, null, null));
        WebViewCrashInfoCollector.sortByMostRecent(testList);
        Assert.assertThat(testList,
                contains(equalsTo(createCrashInfo(
                                 "abc456", 112L, null, 112L, "org.test.package", null)),
                        equalsTo(createCrashInfo(
                                "ghijkl", 112L, null, -1, "com.test.package", null)),
                        equalsTo(createCrashInfo(
                                "def789", 111L, "55667788", 100L, null, UploadState.UPLOADED)),
                        equalsTo(createCrashInfo(
                                null, 100, "11223344", -1, "com.test.package", null)),
                        equalsTo(createCrashInfo("abc123", 100, null, -1, null, null)),
                        equalsTo(createCrashInfo(
                                "xyz123", -1, "11223344", 123L, null, UploadState.UPLOADED)),
                        equalsTo(createCrashInfo(
                                "abc456", -1, null, -1, null, UploadState.PENDING))));
    }

    /**
     * Test loading, sort and filter crashes.
     */
    @Test
    @SmallTest
    public void testLoadCrashesInfoFilteredNoLimit() {
        List<CrashInfo> testList = Arrays.asList(
                createCrashInfo("xyz123", 112233445566L, null, -1, null, UploadState.PENDING),
                createCrashInfo(
                        "def789", -1, "55667788", 123344556677L, null, UploadState.UPLOADED),
                createCrashInfo("abc456", -1, null, -1, null, UploadState.PENDING),
                createCrashInfo("xyz123", 112233445566L, null, -1, "com.test.package", null),
                createCrashInfo("abc456", 445566778899L, null, -1, "org.test.package", null),
                createCrashInfo("abc456", -1, null, -1, null, null),
                createCrashInfo(
                        "xyz123", -1, "11223344", 223344556677L, null, UploadState.UPLOADED));

        WebViewCrashInfoCollector collector =
                new WebViewCrashInfoCollector(new CrashInfoLoadersTestFactory(testList));

        // Get crashes with UPLOAD state only.
        List<CrashInfo> result =
                collector.loadCrashesInfo(c -> c.uploadState == UploadState.UPLOADED);
        Assert.assertThat(result,
                contains(equalsTo(createCrashInfo("xyz123", 112233445566L, "11223344",
                                 223344556677L, "com.test.package", UploadState.UPLOADED)),
                        equalsTo(createCrashInfo("def789", -1, "55667788", 123344556677L, null,
                                UploadState.UPLOADED))));
    }

    /**
     * Test updating crash JSON log file with the new crash info
     */
    @Test
    @SmallTest
    public void testUpdateCrashLogFileWithNewCrashInfo() throws Throwable {
        CrashInfo oldCrashInfo = createCrashInfo("xyz123", 112233445566L, null, -1, null, null);
        assertThat("temp json log file should exist", writeJsonLogFile(oldCrashInfo).exists());

        CrashInfo newCrashInfo =
                createHiddenCrashInfo("xyz123", 112233445566L, null, -1, "com.test.package", null);
        WebViewCrashInfoCollector.updateCrashLogFileWithNewCrashInfo(newCrashInfo);

        CrashInfo resultCrashInfo = getCrashFromJsonLogFile("xyz123");
        Assert.assertThat(resultCrashInfo, equalsTo(newCrashInfo));
    }

    /**
     * Test updating non-existing crash JSON log file with the new crash info
     */
    @Test
    @SmallTest
    public void testUpdateNonExistingCrashLogFileWithNewCrashInfo() throws Throwable {
        CrashInfo newCrashInfo =
                createHiddenCrashInfo("xyz123", 112233445566L, null, -1, "com.test.package", null);
        WebViewCrashInfoCollector.updateCrashLogFileWithNewCrashInfo(newCrashInfo);

        CrashInfo resultCrashInfo = getCrashFromJsonLogFile("xyz123");
        Assert.assertThat(resultCrashInfo, equalsTo(newCrashInfo));
    }
}
