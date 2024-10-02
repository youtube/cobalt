// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.accounts.Account;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.components.signin.test.util.FakeAccountManagerDelegate;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.TimeoutException;

/**
 * Tests for {@link AccountManagerFacade}. See also {@link AccountManagerFacadeImplTest}.
 */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class AccountManagerFacadeTest {
    private static final ExecutorService WORKER = Executors.newSingleThreadExecutor();

    private static class CustomAccountManagerDelegate extends FakeAccountManagerDelegate {
        private final CallbackHelper mBlockGetAccounts = new CallbackHelper();

        @Override
        public Account[] getAccounts() {
            // Blocks thread that's trying to get accounts from the delegate.
            try {
                mBlockGetAccounts.waitForFirst();
            } catch (TimeoutException e) {
                throw new RuntimeException(e);
            }
            return super.getAccounts();
        }

        void unblockGetAccounts() {
            // Unblock the getAccountsSync() from a different thread to avoid deadlock on UI thread
            WORKER.execute(mBlockGetAccounts::notifyCalled);
        }
    }

    private final CustomAccountManagerDelegate mDelegate = new CustomAccountManagerDelegate();

    @Before
    public void setUp() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            AccountManagerFacadeProvider.setInstanceForTests(
                    new AccountManagerFacadeImpl(mDelegate));
        });
    }

    @After
    public void tearDown() {
        AccountManagerFacadeProvider.resetInstanceForTests();
    }

    @Test
    @SmallTest
    public void testIsCachePopulated() throws InterruptedException {
        // Cache shouldn't be populated until getAccountsSync is unblocked.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            assertFalse(AccountManagerFacadeProvider.getInstance().getAccounts().isFulfilled());
        });

        mDelegate.unblockGetAccounts();
        CountDownLatch countDownLatch = new CountDownLatch(1);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            AccountManagerFacadeProvider.getInstance().getAccounts().then(
                    accounts -> { countDownLatch.countDown(); });
        });
        // Wait for cache population to finish.
        countDownLatch.await();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            assertTrue(AccountManagerFacadeProvider.getInstance().getAccounts().isFulfilled());
        });
    }

    @Test
    @SmallTest
    public void testRunAfterCacheIsPopulated() throws InterruptedException {
        CountDownLatch firstCounter = new CountDownLatch(1);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // Add callback. This should be done on the main thread.
            AccountManagerFacadeProvider.getInstance().getAccounts().then(
                    accounts -> { firstCounter.countDown(); });
        });
        assertEquals("Callback shouldn't be invoked until cache is populated", 1,
                firstCounter.getCount());

        mDelegate.unblockGetAccounts();
        // Cache should be populated & callback should be invoked
        firstCounter.await();

        CountDownLatch secondCounter = new CountDownLatch(1);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            AccountManagerFacadeProvider.getInstance().getAccounts().then(
                    accounts -> { secondCounter.countDown(); });
            assertEquals("Callback should be posted on UI thread, not executed synchronously", 1,
                    secondCounter.getCount());
        });
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        assertEquals(
                "Callback should be posted to UI thread right away", 0, secondCounter.getCount());
    }
}
