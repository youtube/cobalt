// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.metrics;

import static org.hamcrest.Matchers.greaterThan;
import static org.hamcrest.Matchers.is;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertThat;
import static org.junit.Assert.fail;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;

import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;

import java.time.Duration;
import java.time.Instant;
import java.util.List;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicIntegerArray;
import java.util.concurrent.locks.Lock;
import java.util.concurrent.locks.ReentrantLock;

/** Unit tests for {@link CachingUmaRecorderTest}. */
@RunWith(BaseRobolectricTestRunner.class)
@SuppressWarnings("DoNotMock") // Ok to mock UmaRecorder since this is testing metrics.
public final class CachingUmaRecorderTest {
    @Mock
    UmaRecorder mUmaRecorder;

    @Before
    public void initMocks() {
        MockitoAnnotations.initMocks(this);
    }

    @Test
    public void testSetDelegateWithEmptyCache() {
        CachingUmaRecorder cachingUmaRecorder = new CachingUmaRecorder();

        cachingUmaRecorder.setDelegate(mUmaRecorder);

        verifyNoMoreInteractions(mUmaRecorder);
    }

    @Test
    public void testRecordBooleanHistogramGetsFlushed() {
        CachingUmaRecorder cachingUmaRecorder = new CachingUmaRecorder();

        cachingUmaRecorder.recordBooleanHistogram(
                "cachingUmaRecorderTest.recordBooleanHistogram", true);
        cachingUmaRecorder.recordBooleanHistogram(
                "cachingUmaRecorderTest.recordBooleanHistogram", true);
        cachingUmaRecorder.recordBooleanHistogram(
                "cachingUmaRecorderTest.recordBooleanHistogram", false);

        assertEquals(3,
                cachingUmaRecorder.getHistogramTotalCountForTesting(
                        "cachingUmaRecorderTest.recordBooleanHistogram"));
        assertEquals(2,
                cachingUmaRecorder.getHistogramValueCountForTesting(
                        "cachingUmaRecorderTest.recordBooleanHistogram", 1));
        assertEquals(1,
                cachingUmaRecorder.getHistogramValueCountForTesting(
                        "cachingUmaRecorderTest.recordBooleanHistogram", 0));

        cachingUmaRecorder.setDelegate(mUmaRecorder);

        verify(mUmaRecorder, times(2))
                .recordBooleanHistogram("cachingUmaRecorderTest.recordBooleanHistogram", true);
        verify(mUmaRecorder)
                .recordBooleanHistogram("cachingUmaRecorderTest.recordBooleanHistogram", false);
    }

    @Test
    public void testRecordExponentialHistogramGetsFlushed() {
        CachingUmaRecorder cachingUmaRecorder = new CachingUmaRecorder();

        cachingUmaRecorder.recordExponentialHistogram(
                "cachingUmaRecorderTest.recordExponentialHistogram", 72, 1, 1000, 50);
        assertEquals(1,
                cachingUmaRecorder.getHistogramTotalCountForTesting(
                        "cachingUmaRecorderTest.recordExponentialHistogram"));
        assertEquals(1,
                cachingUmaRecorder.getHistogramValueCountForTesting(
                        "cachingUmaRecorderTest.recordExponentialHistogram", 72));
        cachingUmaRecorder.setDelegate(mUmaRecorder);

        verify(mUmaRecorder)
                .recordExponentialHistogram(
                        "cachingUmaRecorderTest.recordExponentialHistogram", 72, 1, 1000, 50);
    }

    @Test
    public void testRecordLinearHistogramGetsFlushed() {
        CachingUmaRecorder cachingUmaRecorder = new CachingUmaRecorder();

        cachingUmaRecorder.recordLinearHistogram(
                "cachingUmaRecorderTest.recordLinearHistogram", 72, 1, 1000, 50);
        assertEquals(1,
                cachingUmaRecorder.getHistogramTotalCountForTesting(
                        "cachingUmaRecorderTest.recordLinearHistogram"));
        assertEquals(1,
                cachingUmaRecorder.getHistogramValueCountForTesting(
                        "cachingUmaRecorderTest.recordLinearHistogram", 72));

        cachingUmaRecorder.setDelegate(mUmaRecorder);

        verify(mUmaRecorder)
                .recordLinearHistogram(
                        "cachingUmaRecorderTest.recordLinearHistogram", 72, 1, 1000, 50);
    }

    @Test
    public void testRecordSparseHistogramGetsFlushed() {
        CachingUmaRecorder cachingUmaRecorder = new CachingUmaRecorder();

        cachingUmaRecorder.recordSparseHistogram(
                "cachingUmaRecorderTest.recordSparseHistogram", 72);
        assertEquals(1,
                cachingUmaRecorder.getHistogramTotalCountForTesting(
                        "cachingUmaRecorderTest.recordSparseHistogram"));
        assertEquals(1,
                cachingUmaRecorder.getHistogramValueCountForTesting(
                        "cachingUmaRecorderTest.recordSparseHistogram", 72));
        cachingUmaRecorder.setDelegate(mUmaRecorder);

        verify(mUmaRecorder)
                .recordSparseHistogram("cachingUmaRecorderTest.recordSparseHistogram", 72);
    }

    @Test
    public void testRecordUserActionGetsFlushed() {
        CachingUmaRecorder cachingUmaRecorder = new CachingUmaRecorder();

        cachingUmaRecorder.recordUserAction("FlushedActionTested", 72);
        cachingUmaRecorder.setDelegate(mUmaRecorder);

        verify(mUmaRecorder).recordUserAction("FlushedActionTested", 72);
    }

    @Test
    public void testRecordUserActionGetsMultipleFlushed() {
        CachingUmaRecorder cachingUmaRecorder = new CachingUmaRecorder();

        cachingUmaRecorder.recordUserAction("FlushedActionTested.A", 111);
        cachingUmaRecorder.recordUserAction("FlushedActionTested.B", 222);
        cachingUmaRecorder.recordUserAction("FlushedActionTested.B", 333);
        cachingUmaRecorder.setDelegate(mUmaRecorder);

        verify(mUmaRecorder).recordUserAction("FlushedActionTested.A", 111);
        verify(mUmaRecorder).recordUserAction("FlushedActionTested.B", 222);
        verify(mUmaRecorder).recordUserAction("FlushedActionTested.B", 333);
    }

    @Test
    public void testRecordBooleanHistogramDelegated() {
        CachingUmaRecorder cachingUmaRecorder = new CachingUmaRecorder();
        cachingUmaRecorder.setDelegate(mUmaRecorder);

        cachingUmaRecorder.recordBooleanHistogram(
                "CachingUmaRecorderTest.recordBooleanHistogram", true);

        verify(mUmaRecorder)
                .recordBooleanHistogram("CachingUmaRecorderTest.recordBooleanHistogram", true);
        verifyNoMoreInteractions(mUmaRecorder);
    }

    @Test
    public void testRecordExponentialHistogramDelegated() {
        CachingUmaRecorder cachingUmaRecorder = new CachingUmaRecorder();
        cachingUmaRecorder.setDelegate(mUmaRecorder);

        cachingUmaRecorder.recordExponentialHistogram(
                "CachingUmaRecorderTest.recordExponentialHistogram", 1, 2, 10, 5);

        verify(mUmaRecorder)
                .recordExponentialHistogram(
                        "CachingUmaRecorderTest.recordExponentialHistogram", 1, 2, 10, 5);
        verifyNoMoreInteractions(mUmaRecorder);
    }

    @Test
    public void testRecordLinearHistogramDelegated() {
        CachingUmaRecorder cachingUmaRecorder = new CachingUmaRecorder();
        cachingUmaRecorder.setDelegate(mUmaRecorder);

        cachingUmaRecorder.recordLinearHistogram(
                "CachingUmaRecorderTest.recordLinearHistogram", 1, 2, 10, 5);

        verify(mUmaRecorder)
                .recordLinearHistogram("CachingUmaRecorderTest.recordLinearHistogram", 1, 2, 10, 5);
        verifyNoMoreInteractions(mUmaRecorder);
    }

    @Test
    public void testRecordSparseHistogramDelegated() {
        CachingUmaRecorder cachingUmaRecorder = new CachingUmaRecorder();
        cachingUmaRecorder.setDelegate(mUmaRecorder);

        cachingUmaRecorder.recordSparseHistogram(
                "CachingUmaRecorderTest.recordSparseHistogram", 72);

        verify(mUmaRecorder)
                .recordSparseHistogram("CachingUmaRecorderTest.recordSparseHistogram", 72);
        verifyNoMoreInteractions(mUmaRecorder);
    }

    @Test
    public void testRecordUserActionDelegated() {
        CachingUmaRecorder cachingUmaRecorder = new CachingUmaRecorder();
        cachingUmaRecorder.setDelegate(mUmaRecorder);

        cachingUmaRecorder.recordUserAction("ActionTested", 72);

        verify(mUmaRecorder).recordUserAction("ActionTested", 72);
        verifyNoMoreInteractions(mUmaRecorder);
    }

    @Test
    public void testSetDelegateStopsOldDelegation() {
        CachingUmaRecorder cachingUmaRecorder = new CachingUmaRecorder();
        cachingUmaRecorder.setDelegate(mUmaRecorder);

        cachingUmaRecorder.setDelegate(new NoopUmaRecorder());
        cachingUmaRecorder.recordSparseHistogram("CachingUmaRecorderTest.stopOldDelegation", 72);

        verifyNoMoreInteractions(mUmaRecorder);
    }

    @Test
    public void testSetDelegateStartsNewDelegation() {
        CachingUmaRecorder cachingUmaRecorder = new CachingUmaRecorder();
        cachingUmaRecorder.setDelegate(new NoopUmaRecorder());

        cachingUmaRecorder.setDelegate(mUmaRecorder);
        cachingUmaRecorder.recordSparseHistogram("CachingUmaRecorderTest.startNewDelegation", 72);

        verify(mUmaRecorder).recordSparseHistogram("CachingUmaRecorderTest.startNewDelegation", 72);
        verifyNoMoreInteractions(mUmaRecorder);
    }

    /**
     * {@link UmaRecorder} that has a public lock which can be used to block {@link
     * #recordSparseHistogram(String, int)} }.
     */
    private static class BlockingUmaRecorder extends NoopUmaRecorder {
        public Lock lock = new ReentrantLock();

        @SuppressWarnings("LockNotBeforeTry")
        @Override
        public void recordSparseHistogram(String name, int sample) {
            lock.lock();
            lock.unlock();
        }
    }

    @Test
    public void testSetDelegateBlocksUntilRecordingDone() throws Exception {
        CachingUmaRecorder cachingUmaRecorder = new CachingUmaRecorder();
        BlockingUmaRecorder blockingUmaRecorder = new BlockingUmaRecorder();
        cachingUmaRecorder.setDelegate(blockingUmaRecorder);

        Thread swappingThread;
        Thread recordingThread;

        blockingUmaRecorder.lock.lock();
        try {
            recordingThread = new Thread(() -> {
                cachingUmaRecorder.recordSparseHistogram(
                        "CachingUmaRecorderTest.blockUntilRecordingDone", 16);
            });
            recordingThread.start();
            awaitThreadBlocked(recordingThread, Duration.ofSeconds(1));

            swappingThread = new Thread(() -> { cachingUmaRecorder.setDelegate(mUmaRecorder); });
            swappingThread.start();
            awaitThreadBlocked(swappingThread, Duration.ofSeconds(1));
        } finally {
            blockingUmaRecorder.lock.unlock();
        }

        recordingThread.join();
        swappingThread.join();
        verifyNoMoreInteractions(mUmaRecorder);
    }

    @SuppressWarnings("ThreadPriorityCheck")
    private static void awaitThreadBlocked(Thread thread, Duration timeLimit) {
        final Instant start = Instant.now();

        while (true) {
            assertThat(timeLimit, greaterThan(Duration.between(start, Instant.now())));
            switch (thread.getState()) {
                case BLOCKED:
                case WAITING:
                case TIMED_WAITING:
                    return;
                case NEW:
                case RUNNABLE: {
                    Thread.yield();
                    continue;
                }
                case TERMINATED:
                    fail("Thread unexpectedly terminated.");
            }
        }
    }

    private static class HistogramTestingUmaRecorder implements UmaRecorder {
        public final AtomicIntegerArray recordedSamples;

        HistogramTestingUmaRecorder(int sampleRange) {
            recordedSamples = new AtomicIntegerArray(sampleRange);
        }

        @Override
        public void recordBooleanHistogram(String name, boolean sample) {
            throw new UnsupportedOperationException();
        }

        @Override
        public void recordExponentialHistogram(
                String name, int sample, int min, int max, int numBuckets) {
            // Ignore internal cache metrics.
            if (name.startsWith("UMA.JavaCachingRecorder")) return;
            throw new UnsupportedOperationException();
        }

        @Override
        public void recordLinearHistogram(
                String name, int sample, int min, int max, int numBuckets) {
            throw new UnsupportedOperationException();
        }

        @Override
        public void recordSparseHistogram(String name, int sample) {
            recordedSamples.addAndGet(sample, 1);
        }

        @Override
        public void recordUserAction(String name, long elapsedRealtimeMillis) {
            throw new UnsupportedOperationException();
        }

        @Override
        public int getHistogramValueCountForTesting(String name, int sample) {
            throw new UnsupportedOperationException();
        }

        @Override
        public int getHistogramTotalCountForTesting(String name) {
            throw new UnsupportedOperationException();
        }

        @Override
        public List<HistogramBucket> getHistogramSamplesForTesting(String name) {
            throw new UnsupportedOperationException();
        }

        @Override
        public void addUserActionCallbackForTesting(Callback<String> callback) {
            throw new UnsupportedOperationException();
        }

        @Override
        public void removeUserActionCallbackForTesting(Callback<String> callback) {
            throw new UnsupportedOperationException();
        }
    }

    @Test
    @MediumTest
    @SuppressWarnings("ThreadPriorityCheck")
    public void testStressParallelHistograms() throws Exception {
        final int numThreads = 16;
        final int numRecorders = 16;
        final int numSamples = CachingUmaRecorder.Histogram.MAX_SAMPLE_COUNT / numThreads;
        CachingUmaRecorder cachingRecorder = new CachingUmaRecorder();

        HistogramTestingUmaRecorder[] testingRecorders =
                new HistogramTestingUmaRecorder[numRecorders];
        for (int i = 0; i < numRecorders; i++) {
            testingRecorders[i] = new HistogramTestingUmaRecorder(numThreads);
        }
        Thread[] threads = new Thread[numThreads];
        // Each thread records a different sample value.
        for (int i = 0; i < numThreads; i++) {
            threads[i] = startHistogramRecordingThread(i, numSamples, cachingRecorder);
        }
        // Change recorders while threads are recording metrics.
        for (HistogramTestingUmaRecorder recorder : testingRecorders) {
            cachingRecorder.setDelegate(recorder);
            Thread.yield();
        }
        for (Thread thread : threads) {
            thread.join();
        }
        cachingRecorder.setDelegate(mUmaRecorder);
        verifyNoMoreInteractions(mUmaRecorder);
        for (int i = 0; i < numThreads; i++) {
            int actualSamples = 0;
            for (HistogramTestingUmaRecorder recorder : testingRecorders) {
                actualSamples += recorder.recordedSamples.get(i);
            }
            assertThat(String.format("thread[%d] total samples", i), actualSamples, is(numSamples));
        }
    }

    @SuppressWarnings("ThreadPriorityCheck")
    private static Thread startHistogramRecordingThread(
            int sample, int count, UmaRecorder recorder) {
        Thread thread = new Thread(() -> {
            for (int i = 0; i < count; i++) {
                recorder.recordSparseHistogram("StressTest", sample);
                // Make it more likely this thread will be preempted.
                Thread.yield();
            }
        });
        thread.start();
        return thread;
    }

    private static class UserActionTestingUmaRecorder implements UmaRecorder {
        public final AtomicIntegerArray recordedSamples;

        UserActionTestingUmaRecorder(int sampleRange) {
            recordedSamples = new AtomicIntegerArray(sampleRange);
        }

        @Override
        public void recordBooleanHistogram(String name, boolean sample) {
            throw new UnsupportedOperationException();
        }

        @Override
        public void recordExponentialHistogram(
                String name, int sample, int min, int max, int numBuckets) {
            // Ignore internal cache metrics.
            if (name.startsWith("UMA.JavaCachingRecorder")) return;
            throw new UnsupportedOperationException();
        }

        @Override
        public void recordLinearHistogram(
                String name, int sample, int min, int max, int numBuckets) {
            throw new UnsupportedOperationException();
        }

        @Override
        public void recordSparseHistogram(String name, int sample) {
            throw new UnsupportedOperationException();
        }

        @Override
        public void recordUserAction(String name, long elapsedRealtimeMillis) {
            recordedSamples.addAndGet((int) elapsedRealtimeMillis, 1);
        }

        @Override
        public int getHistogramValueCountForTesting(String name, int sample) {
            throw new UnsupportedOperationException();
        }

        @Override
        public int getHistogramTotalCountForTesting(String name) {
            throw new UnsupportedOperationException();
        }

        @Override
        public List<HistogramBucket> getHistogramSamplesForTesting(String name) {
            throw new UnsupportedOperationException();
        }

        @Override
        public void addUserActionCallbackForTesting(Callback<String> callback) {
            throw new UnsupportedOperationException();
        }

        @Override
        public void removeUserActionCallbackForTesting(Callback<String> callback) {
            throw new UnsupportedOperationException();
        }
    }

    @Test
    @MediumTest
    @SuppressWarnings("ThreadPriorityCheck")
    public void testStressParallelUserActions() throws Exception {
        final int numThreads = 16;
        final int numRecorders = 16;
        final int numSamples = CachingUmaRecorder.MAX_USER_ACTION_COUNT / numThreads;
        CachingUmaRecorder cachingRecorder = new CachingUmaRecorder();

        UserActionTestingUmaRecorder[] testingRecorders =
                new UserActionTestingUmaRecorder[numRecorders];
        for (int i = 0; i < numRecorders; i++) {
            testingRecorders[i] = new UserActionTestingUmaRecorder(numThreads);
        }
        Thread[] threads = new Thread[numThreads];
        // Each thread records a different timestamp value.
        for (int i = 0; i < numThreads; i++) {
            threads[i] = startUserActionRecordingThread(i, numSamples, cachingRecorder);
        }
        // Change recorders while threads are recording metrics.
        for (UserActionTestingUmaRecorder recorder : testingRecorders) {
            cachingRecorder.setDelegate(recorder);
            Thread.yield();
        }
        for (Thread thread : threads) {
            thread.join();
        }
        cachingRecorder.setDelegate(mUmaRecorder);
        verifyNoMoreInteractions(mUmaRecorder);
        for (int i = 0; i < numThreads; i++) {
            int actualSamples = 0;
            for (UserActionTestingUmaRecorder recorder : testingRecorders) {
                actualSamples += recorder.recordedSamples.get(i);
            }
            assertThat(String.format("thread[%d] total samples", i), actualSamples, is(numSamples));
        }
    }

    @SuppressWarnings("ThreadPriorityCheck")
    private static Thread startUserActionRecordingThread(
            int sample, int count, UmaRecorder recorder) {
        Thread thread = new Thread(() -> {
            for (int i = 0; i < count; i++) {
                recorder.recordUserAction("StressTestUserAction." + i, sample);
                // Make it more likely this thread will be preempted.
                Thread.yield();
            }
        });
        thread.start();
        return thread;
    }

    @Test
    public void testUserActionCallbacksTriggeredWithoutDelegate() {
        CachingUmaRecorder cachingRecorder = new CachingUmaRecorder();

        AtomicInteger callbackCount = new AtomicInteger();
        Callback<String> testCallback1 = (result) -> callbackCount.incrementAndGet();
        cachingRecorder.addUserActionCallbackForTesting(testCallback1);

        // Ensure that the callback is notified if attached to the caching recorder (will not
        // be notified once a delegate is responsible).
        Assert.assertEquals(0, callbackCount.get());
        cachingRecorder.recordUserAction("TEST", 0);
        Assert.assertEquals(1, callbackCount.get());
        cachingRecorder.recordUserAction("TEST", 0);
        Assert.assertEquals(2, callbackCount.get());

        // Remove the callback and ensure it is no longer called.
        cachingRecorder.removeUserActionCallbackForTesting(testCallback1);
        Assert.assertEquals(2, callbackCount.get());
        cachingRecorder.recordUserAction("TEST", 0);
        Assert.assertEquals(2, callbackCount.get());
    }

    @Test
    public void testUserActionCallbacksSwappedBetweenDelegates() {
        CachingUmaRecorder cachingRecorder = new CachingUmaRecorder();

        Callback<String> testCallback1 = (result) -> {};
        Callback<String> testCallback2 = (result) -> {};
        cachingRecorder.addUserActionCallbackForTesting(testCallback1);
        cachingRecorder.addUserActionCallbackForTesting(testCallback2);

        // Validate that previously added callbacks are passed to the new delegate.
        UmaRecorder delegate1 = Mockito.mock(UmaRecorder.class);
        cachingRecorder.setDelegate(delegate1);
        verify(delegate1).addUserActionCallbackForTesting(testCallback1);
        verify(delegate1).addUserActionCallbackForTesting(testCallback2);

        // Validate that previously added callbacks are removed from the previous delegate, and
        // passed to the new delegate.
        UmaRecorder delegate2 = Mockito.mock(UmaRecorder.class);
        cachingRecorder.setDelegate(delegate2);
        verify(delegate1).removeUserActionCallbackForTesting(testCallback1);
        verify(delegate1).removeUserActionCallbackForTesting(testCallback2);
        verify(delegate2).addUserActionCallbackForTesting(testCallback1);
        verify(delegate2).addUserActionCallbackForTesting(testCallback2);

        // Ensure a callback added later is also added correctly.
        Callback<String> testCallback3 = (result) -> {};
        cachingRecorder.addUserActionCallbackForTesting(testCallback3);
        verifyNoMoreInteractions(delegate1);
        verify(delegate2).addUserActionCallbackForTesting(testCallback3);
    }
}
