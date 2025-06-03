// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.crash;

import android.annotation.SuppressLint;
import android.app.job.JobInfo;
import android.app.job.JobScheduler;
import android.app.job.JobWorkItem;
import android.content.Context;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.StreamUtil;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.AdvancedMockContext;
import org.chromium.components.background_task_scheduler.TaskIds;
import org.chromium.components.crash.LogcatCrashExtractor;
import org.chromium.components.crash.MinidumpLogcatPrepender;
import org.chromium.components.minidump_uploader.CrashFileManager;
import org.chromium.components.minidump_uploader.CrashTestRule;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileReader;
import java.io.FileWriter;
import java.io.IOException;
import java.util.Arrays;
import java.util.List;

/** Unittests for {@link LogcatExtractionRunnable}. */
@RunWith(BaseJUnit4ClassRunner.class)
public class LogcatExtractionRunnableTest {
    @Rule public CrashTestRule mTestRule = new CrashTestRule();

    private File mCrashDir;

    private static final String BOUNDARY = "boundary";
    private static final String MINIDUMP_CONTENTS = "important minidump contents";
    private static final List<String> LOGCAT =
            Arrays.asList("some random log content", "some more deterministic log content");

    private static class TestLogcatCrashExtractor extends LogcatCrashExtractor {
        @Override
        protected List<String> getLogcat() {
            return LOGCAT;
        }
    }
    ;

    private static class TestJobScheduler extends JobScheduler {
        TestJobScheduler() {}

        @Override
        public void cancel(int jobId) {}

        @Override
        public void cancelAll() {}

        @Override
        @SuppressLint("WrongConstant")
        public int enqueue(JobInfo job, JobWorkItem work) {
            return 0;
        }

        @Override
        public List<JobInfo> getAllPendingJobs() {
            return null;
        }

        @Override
        public JobInfo getPendingJob(int jobId) {
            return null;
        }

        @Override
        public int schedule(JobInfo job) {
            Assert.assertEquals(TaskIds.CHROME_MINIDUMP_UPLOADING_JOB_ID, job.getId());
            Assert.assertEquals(
                    ChromeMinidumpUploadJobService.class.getName(),
                    job.getService().getClassName());
            return JobScheduler.RESULT_SUCCESS;
        }
    }
    ;

    // Responsible for verifying that the correct intent is fired after the logcat is extracted.
    private class TestContext extends AdvancedMockContext {
        TestContext(Context realContext) {
            super(realContext);
        }

        @Override
        public Object getSystemService(String name) {
            if (Context.JOB_SCHEDULER_SERVICE.equals(name)) {
                return new TestJobScheduler();
            }

            return super.getSystemService(name);
        }
    }

    @Before
    public void setUp() {
        mCrashDir = new CrashFileManager(mTestRule.getCacheDir()).getCrashDirectory();
    }

    /**
     * Creates a simple fake minidump file for testing.
     *
     * @param filename The name of the file to create.
     */
    private File createMinidump(String filename) throws IOException {
        File minidump = new File(mCrashDir, filename);
        FileWriter writer = null;
        try {
            writer = new FileWriter(minidump);
            writer.write(BOUNDARY + "\n");
            writer.write(MINIDUMP_CONTENTS + "\n");
        } finally {
            StreamUtil.closeQuietly(writer);
        }
        return minidump;
    }

    /**
     * Verifies that the contents of the {@param filename} are the expected ones.
     *
     * @param filename The name of the file containing the concatenated logcat and minidump output.
     */
    private void verifyMinidumpWithLogcat(String filename) throws IOException {
        BufferedReader input = null;
        try {
            File minidumpWithLogcat = new File(mCrashDir, filename);
            Assert.assertTrue(
                    "Should have created a file containing the logcat and minidump contents",
                    minidumpWithLogcat.exists());

            input = new BufferedReader(new FileReader(minidumpWithLogcat));
            Assert.assertEquals(
                    "The first line should be the boundary line.", BOUNDARY, input.readLine());
            Assert.assertEquals(
                    "The second line should be the content dispoistion.",
                    MinidumpLogcatPrepender.LOGCAT_CONTENT_DISPOSITION,
                    input.readLine());
            Assert.assertEquals(
                    "The third line should be the content type.",
                    MinidumpLogcatPrepender.LOGCAT_CONTENT_TYPE,
                    input.readLine());
            Assert.assertEquals(
                    "The fourth line should be blank, for padding.", "", input.readLine());
            for (String expected : LOGCAT) {
                Assert.assertEquals("The logcat contents should match", expected, input.readLine());
            }
            Assert.assertEquals(
                    "The logcat should be followed by the boundary line.",
                    BOUNDARY,
                    input.readLine());
            Assert.assertEquals(
                    "The minidump contents should follow.", MINIDUMP_CONTENTS, input.readLine());
            Assert.assertNull("There should be nothing else in the file", input.readLine());
        } finally {
            StreamUtil.closeQuietly(input);
        }
    }

    @Test
    @MediumTest
    public void testSimpleExtraction() throws IOException {
        final File minidump = createMinidump("test.dmp");
        Context testContext = new TestContext(ApplicationProvider.getApplicationContext());

        LogcatExtractionRunnable runnable =
                new LogcatExtractionRunnable(minidump, new TestLogcatCrashExtractor());
        runnable.run();

        verifyMinidumpWithLogcat("test.dmp.try0");
    }
}
