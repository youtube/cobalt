// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webengine.test;

import static org.chromium.content_public.browser.test.util.TestThreadUtils.runOnUiThreadBlocking;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.view.View;

import androidx.annotation.Nullable;
import androidx.test.InstrumentationRegistry;

import org.junit.Assert;

import org.chromium.webengine.Navigation;
import org.chromium.webengine.NavigationObserver;
import org.chromium.webengine.Tab;
import org.chromium.webengine.TabListObserver;
import org.chromium.webengine.WebEngine;
import org.chromium.webengine.WebFragment;
import org.chromium.webengine.WebSandbox;
import org.chromium.webengine.test.instrumentation_test_apk.InstrumentationActivity;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.atomic.AtomicReference;

/**
 * ActivityTestRule for InstrumentationActivity.
 *
 * Test can use this ActivityTestRule to launch or get InstrumentationActivity.
 */
public class InstrumentationActivityTestRule
        extends WebEngineActivityTestRule<InstrumentationActivity> {
    @Nullable
    private WebFragment mFragment;

    public InstrumentationActivityTestRule() {
        super(InstrumentationActivity.class);
    }

    /**
     * Starts the WebEngine Instrumentation activity.
     */
    public void launchShell() {
        Intent intent = new Intent(Intent.ACTION_MAIN);
        intent.addCategory(Intent.CATEGORY_LAUNCHER);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.setComponent(getShellComponentName());
        launchActivity(intent);
    }

    public ComponentName getShellComponentName() {
        return new ComponentName(InstrumentationRegistry.getInstrumentation().getTargetContext(),
                InstrumentationActivity.class);
    }

    public void finish() {
        Assert.assertNotNull(getActivity());
        getActivity().finish();
    }

    public WebSandbox getWebSandbox() throws Exception {
        return getActivity().getWebSandboxFuture().get();
    }

    /**
     * Attaches a fragment to the container in the activity. If a fragment is already present, it
     * will detach it first.
     */
    public void attachFragment(WebFragment fragment) {
        if (mFragment != null) {
            detachFragment(mFragment);
        }

        getActivity().attachFragment(fragment);
        mFragment = fragment;
    }

    /**
     * Return the current fragment attached to the fragment container in the activity.
     */
    public WebFragment getFragment() {
        Assert.assertNotNull(mFragment);
        return mFragment;
    }

    public void detachFragment(WebFragment fragment) {
        getActivity().detachFragment(fragment);
    }

    public Tab getActiveTab() throws Exception {
        return getFragment().getWebEngine().getTabManager().getActiveTab();
    }

    public View getFragmentContainerView() {
        return getActivity().getFragmentContainerView();
    }

    /**
     * Creates a new WebEngine and attaches its Fragment before navigating.
     */
    public WebEngine createWebEngineAttachThenNavigateAndWait(String path) throws Exception {
        WebSandbox sandbox = getWebSandbox();
        WebEngine webEngine = runOnUiThreadBlocking(() -> sandbox.createWebEngine()).get();

        runOnUiThreadBlocking(() -> attachFragment(webEngine.getFragment()));

        Tab activeTab = webEngine.getTabManager().getActiveTab();

        navigateAndWait(activeTab, path);
        return webEngine;
    }

    /**
     * Navigates Tab to new path and waits for navigation to complete.
     */
    public void navigateAndWait(Tab tab, String url) throws Exception {
        CountDownLatch navigationCompleteLatch = new CountDownLatch(1);
        AtomicReference<Navigation> navigationFailure = new AtomicReference();
        runOnUiThreadBlocking(() -> {
            tab.getNavigationController().registerNavigationObserver(new NavigationObserver() {
                @Override
                public void onNavigationCompleted(Tab tab, Navigation navigation) {
                    navigationCompleteLatch.countDown();
                }
                @Override
                public void onNavigationFailed(Tab tab, Navigation navigation) {
                    navigationFailure.set(navigation);
                    navigationCompleteLatch.countDown();
                }
            });
            tab.getNavigationController().navigate(url);
        });
        navigationCompleteLatch.await();

        if (navigationFailure.get() != null) {
            throw new NavigationFailureException(navigationFailure.get());
        }
    }

    public void setTabActiveAndWait(WebEngine webEngine, Tab tab) throws Exception {
        CountDownLatch setActiveLatch = new CountDownLatch(1);
        runOnUiThreadBlocking(() -> {
            webEngine.getTabManager().registerTabListObserver(new TabListObserver() {
                @Override
                public void onActiveTabChanged(WebEngine webEngine, Tab activeTab) {
                    setActiveLatch.countDown();
                }
            });
            tab.setActive();
        });
        setActiveLatch.await();
    }

    public Context getContext() {
        return getActivity();
    }

    public String getPackageName() {
        return getActivity().getPackageName();
    }

    /**
     * Thrown by navigateAndWait if the navigation failed.
     */
    public class NavigationFailureException extends RuntimeException {
        private final Navigation mNavigation;
        NavigationFailureException(Navigation navigation) {
            super("Navigation failed.");
            mNavigation = navigation;
        }

        public Navigation getNavigation() {
            return mNavigation;
        }
    }
}
