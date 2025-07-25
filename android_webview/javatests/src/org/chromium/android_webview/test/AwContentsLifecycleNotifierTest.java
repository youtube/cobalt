// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.android_webview.AppState;
import org.chromium.android_webview.AwContentsLifecycleNotifier;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/** AwContentsLifecycleNotifier tests. */
@RunWith(AwJUnit4ClassRunner.class)
public class AwContentsLifecycleNotifierTest {
    @Rule public AwActivityTestRule mActivityTestRule = new AwActivityTestRule();

    private TestAwContentsClient mContentsClient = new TestAwContentsClient();

    private static class LifecycleObserver implements AwContentsLifecycleNotifier.Observer {
        public CallbackHelper mFirstWebViewCreatedCallback = new CallbackHelper();
        public CallbackHelper mLastWebViewDestroyedCallback = new CallbackHelper();

        @Override
        public void onFirstWebViewCreated() {
            mFirstWebViewCreatedCallback.notifyCalled();
        }

        @Override
        public void onLastWebViewDestroyed() {
            mLastWebViewDestroyedCallback.notifyCalled();
        }
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testNotifierCreate() throws Throwable {
        LifecycleObserver observer = new LifecycleObserver();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AwContentsLifecycleNotifier.getInstance().addObserver(observer);
                    Assert.assertFalse(
                            AwContentsLifecycleNotifier.getInstance().hasWebViewInstances());
                });

        AwTestContainerView awTestContainerView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient);
        observer.mFirstWebViewCreatedCallback.waitForCallback(0, 1);

        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertTrue(
                            AwContentsLifecycleNotifier.getInstance().hasWebViewInstances());
                    mActivityTestRule.getActivity().removeAllViews();
                });
        mActivityTestRule.destroyAwContentsOnMainSync(awTestContainerView.getAwContents());
        observer.mLastWebViewDestroyedCallback.waitForCallback(0, 1);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertFalse(
                            AwContentsLifecycleNotifier.getInstance().hasWebViewInstances());
                });
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testAppState() throws Throwable {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertFalse(
                            AwContentsLifecycleNotifier.getInstance().hasWebViewInstances());
                    Assert.assertEquals(
                            AppState.DESTROYED,
                            AwContentsLifecycleNotifier.getInstance().getAppState());
                });

        AwTestContainerView awTestContainerView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient);

        CriteriaHelper.pollUiThread(
                () -> {
                    return AwContentsLifecycleNotifier.getInstance().getAppState()
                            == AppState.FOREGROUND;
                });

        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mActivityTestRule.getActivity().removeAllViews();
                });
        mActivityTestRule.destroyAwContentsOnMainSync(awTestContainerView.getAwContents());

        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertFalse(
                            AwContentsLifecycleNotifier.getInstance().hasWebViewInstances());
                    Assert.assertEquals(
                            AppState.DESTROYED,
                            AwContentsLifecycleNotifier.getInstance().getAppState());
                });
    }
}
