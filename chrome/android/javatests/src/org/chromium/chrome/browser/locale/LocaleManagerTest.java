// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.locale;

import android.os.Build;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.Callback;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisableIf;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.search_engines.SearchEnginePromoType;
import org.chromium.chrome.browser.searchwidget.SearchActivity;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ActivityTestUtils;
import org.chromium.components.policy.test.annotations.Policies;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.concurrent.Callable;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;

/**
 * Integration tests for {@link LocaleManager}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class LocaleManagerTest {
    @Before
    public void setUp() throws ExecutionException {
        TestThreadUtils.runOnUiThreadBlocking(new Callable<Void>() {
            @Override
            public Void call() {
                ChromeBrowserInitializer.getInstance().handleSynchronousStartup();
                return null;
            }
        });
    }

    @Policies.Add({ @Policies.Item(key = "DefaultSearchProviderEnabled", string = "false") })
    @SmallTest
    @Test
    @DisableIf.Build(sdk_is_less_than = Build.VERSION_CODES.O,
            message = "Flaky on Android M and N: https://crbug.com/1170670")
    public void
    testShowSearchEnginePromoDseDisabled() throws TimeoutException {
        final CallbackHelper getShowTypeCallback = new CallbackHelper();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            LocaleManager.getInstance().setDelegateForTest(new LocaleManagerDelegate() {
                @Override
                public int getSearchEnginePromoShowType() {
                    getShowTypeCallback.notifyCalled();
                    return SearchEnginePromoType.DONT_SHOW;
                }
            });
        });

        // Launch any activity as an Activity ref is required to attempt to show the activity.
        final SearchActivity searchActivity = ActivityTestUtils.waitForActivity(
                InstrumentationRegistry.getInstrumentation(), SearchActivity.class);

        final CallbackHelper searchEnginesFinalizedCallback = new CallbackHelper();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            LocaleManager.getInstance().showSearchEnginePromoIfNeeded(
                    searchActivity, new Callback<Boolean>() {
                        @Override
                        public void onResult(Boolean result) {
                            Assert.assertTrue(result);
                            searchEnginesFinalizedCallback.notifyCalled();
                        }
                    });
        });
        searchEnginesFinalizedCallback.waitForCallback(0);
        Assert.assertEquals(0, getShowTypeCallback.getCallCount());
    }
}
