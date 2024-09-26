// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test.common.services;

import static org.chromium.android_webview.test.OnlyRunIn.ProcessMode.SINGLE_PROCESS;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.os.IBinder;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.android_webview.common.services.ServiceConnectionDelayRecorder;
import org.chromium.android_webview.test.AwJUnit4ClassRunner;
import org.chromium.android_webview.test.OnlyRunIn;
import org.chromium.android_webview.test.services.MockVariationsSeedServer;
import org.chromium.base.ContextUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.DoNotBatch;

/**
 * Unit tests for {@link ServiceConnectionDelayRecorder}.
 */
@RunWith(AwJUnit4ClassRunner.class)
@OnlyRunIn(SINGLE_PROCESS)
@DoNotBatch(reason = "To make sure all bound services are properly killed between tests.")
public class ServiceConnectionDelayRecorderTest {
    // Test class that increments time with multiples of 1000 ms.
    private static class TestClock implements TestServiceConnectionDelayRecorder.Clock {
        private long mInternalValue;
        private long mIncrement = 1000;

        @Override
        public long uptimeMillis() {
            mInternalValue += mIncrement;
            mIncrement += 1000;

            return mInternalValue;
        }
    }

    private static class TestServiceConnectionDelayRecorder
            extends ServiceConnectionDelayRecorder implements AutoCloseable {
        private final CallbackHelper mHelper = new CallbackHelper();
        private final TestClock mClock = new TestClock();

        @Override
        public void onServiceConnectedImpl(ComponentName name, IBinder service) {
            mHelper.notifyCalled();
        }

        @Override
        public void onServiceDisconnected(ComponentName name) {}

        public CallbackHelper getOnServiceConnectedListener() {
            return mHelper;
        }

        @Override
        public Clock getClock() {
            return mClock;
        }

        @Override
        public void close() {
            ContextUtils.getApplicationContext().unbindService(this);
        }
    }

    @Test
    @SmallTest
    public void testRecordDelayTwice() throws Throwable {
        final String expectedHistogramName =
                "Android.WebView.Startup.NonblockingServiceConnectionDelay.MockVariationsSeedServer";
        final int exepectedHistogramValue = 2000;
        final Intent intent =
                new Intent(ContextUtils.getApplicationContext(), MockVariationsSeedServer.class);

        try (TestServiceConnectionDelayRecorder recorderConnection =
                        new TestServiceConnectionDelayRecorder()) {
            CallbackHelper helper = recorderConnection.getOnServiceConnectedListener();
            int onServiceConnectedInitCount = helper.getCallCount();

            boolean result = recorderConnection.bind(
                    ContextUtils.getApplicationContext(), intent, Context.BIND_AUTO_CREATE);
            Assert.assertTrue("Failed to bind to service with " + intent, result);

            helper.waitForCallback(onServiceConnectedInitCount, 1);

            Assert.assertEquals("Histogram should be recorded once", 1,
                    RecordHistogram.getHistogramTotalCountForTesting(expectedHistogramName));
            Assert.assertEquals(
                    "Histogram should be have sample value of " + exepectedHistogramValue, 1,
                    RecordHistogram.getHistogramValueCountForTesting(
                            expectedHistogramName, exepectedHistogramValue));

            // When the connection is lost and then restablished by the system, onServiceConnected
            // will be called again. Simulate that by directly calling onServiceConnected.
            onServiceConnectedInitCount = helper.getCallCount();
            recorderConnection.onServiceConnected(null, null);
            helper.waitForCallback(onServiceConnectedInitCount, 1);

            Assert.assertEquals("Histogram should be recorded once", 1,
                    RecordHistogram.getHistogramTotalCountForTesting(expectedHistogramName));
            Assert.assertEquals(
                    "Histogram should be have sample value of " + exepectedHistogramValue, 1,
                    RecordHistogram.getHistogramValueCountForTesting(
                            expectedHistogramName, exepectedHistogramValue));
        }
    }
}
