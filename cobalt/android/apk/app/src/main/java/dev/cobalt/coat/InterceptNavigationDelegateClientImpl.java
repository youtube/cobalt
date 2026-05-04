// Copyright 2025 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

package dev.cobalt.coat;

import static dev.cobalt.util.Log.TAG;

import android.app.Activity;
import dev.cobalt.util.Log;
import org.chromium.components.external_intents.ExternalNavigationHandler;
import org.chromium.components.external_intents.InterceptNavigationDelegateClient;
import org.chromium.components.external_intents.InterceptNavigationDelegateImpl;
import org.chromium.components.external_intents.RedirectHandler;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;

/**
 * Class that provides embedder-level information to InterceptNavigationDelegateImpl based off a
 * Tab.
 */
public class InterceptNavigationDelegateClientImpl implements InterceptNavigationDelegateClient {
    private WebContents mWebContents;
    private RedirectHandler mRedirectHandler;
    private WebContentsObserver mWebContentsObserver;
    private InterceptNavigationDelegateImpl mInterceptNavigationDelegate;
    private Activity mActivity;

    InterceptNavigationDelegateClientImpl(WebContents webContents, Activity activity) {
        mWebContents = webContents;
        mActivity = activity;
        mRedirectHandler = RedirectHandler.create();

        mInterceptNavigationDelegate = new InterceptNavigationDelegateImpl(this);
        mInterceptNavigationDelegate.associateWithWebContents(mWebContents);
    }

    public void destroy() {
        if (mWebContentsObserver != null) {
            mWebContentsObserver.observe(null);
        }
        mInterceptNavigationDelegate.associateWithWebContents(null);
        mInterceptNavigationDelegate = null;
    }

    @Override
    public WebContents getWebContents() {
        return mWebContents;
    }

    @Override
    public ExternalNavigationHandler createExternalNavigationHandler() {
        return new ExternalNavigationHandler(
                new ExternalNavigationDelegateImpl(mWebContents, mActivity));
    }

    @Override
    public RedirectHandler getOrCreateRedirectHandler() {
        return mRedirectHandler;
    }

    @Override
    public boolean isIncognito() {
        return mWebContents.isIncognito();
    }

    @Override
    public Activity getActivity() {
        return mActivity;
    }

    @Override
    public boolean wasTabLaunchedFromExternalApp() {
        return false;
    }

    @Override
    public boolean wasTabLaunchedFromLongPressInBackground() {
        return false;
    }

    @Override
    public void closeTab() {
      // TODO
    }
    @Override
    public void loadUrlIfPossible(LoadUrlParams loadUrlParams) {
        // This method is called when a navigation is not handled by an external intent.
        // In our case, we want to prevent these from loading, so we do nothing.
        Log.w(TAG, "Navigation was not handled by external intent: " + loadUrlParams.getUrl());
    }

    @Override
    public boolean isTabInPWA() {
        return false;
    }

    @Override
    public boolean isTabInBrowser() {
        return true;
    }

    @Override
    public boolean isInDesktopWindowingMode() {
        return false;
    }

    @Override
    public void startReparentingTask() {}
}
