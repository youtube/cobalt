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
import android.os.SystemClock;

import dev.cobalt.util.Log;
import org.chromium.base.ContextUtils;
import org.chromium.components.external_intents.ExternalNavigationHandler;
import org.chromium.components.external_intents.ExternalNavigationHandler.OverrideUrlLoadingAsyncActionType;
import org.chromium.components.external_intents.ExternalNavigationHandler.OverrideUrlLoadingResult;
import org.chromium.components.external_intents.ExternalNavigationHandler.OverrideUrlLoadingResultType;
import org.chromium.components.external_intents.InterceptNavigationDelegateClient;
import org.chromium.components.external_intents.InterceptNavigationDelegateImpl;
import org.chromium.components.external_intents.RedirectHandler;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.NavigationHandle;
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
    InterceptNavigationDelegateClientImpl(WebContents webContents) {
        mWebContents = webContents;
        mRedirectHandler = RedirectHandler.create();
        mWebContentsObserver = new WebContentsObserver() {
          @Override
          public void didFinishNavigationInPrimaryMainFrame(NavigationHandle navigationHandle) {
            Log.e(TAG, "Miguelao didFinishNavigationInPrimaryMainFrame ");
            mInterceptNavigationDelegate.onNavigationFinishedInPrimaryMainFrame(navigationHandle);
          }
        };

      mInterceptNavigationDelegate = new InterceptNavigationDelegateImpl(this);
      mWebContents.addObserver(mWebContentsObserver);
    }

    public void destroy() {
        mWebContents.removeObserver(mWebContentsObserver);
        mInterceptNavigationDelegate.associateWithWebContents(null);
        mInterceptNavigationDelegate = null;
    }

    @Override
    public WebContents getWebContents() {
        return mWebContents;
    }

    @Override
    public ExternalNavigationHandler createExternalNavigationHandler() {
        return new ExternalNavigationHandler(new ExternalNavigationDelegateImpl(mWebContents));
    }

    @Override
    public long getLastUserInteractionTime() {
        return 0;
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
    public boolean areIntentLaunchesAllowedInHiddenTabsForNavigation(
            NavigationHandle navigationHandle) {
        return false;
    }

    @Override
    public Activity getActivity() {
        return null;//ContextUtils.activityFromContext(mWebContents);
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
    public void onNavigationStarted(NavigationHandle navigationHandle) {
        //if (navigationHandle.hasUserGesture()) {
        //    mLastNavigationWithUserGestureTime = SystemClock.elapsedRealtime();
        //}
    }

    @Override
    public void onDecisionReachedForNavigation(
            NavigationHandle navigationHandle, OverrideUrlLoadingResult overrideUrlLoadingResult) {
        //NavigationImpl navigation =
        //        mWebContents.getNavigationControllerImpl().getNavigationImplFromId(
        //                navigationHandle.getNavigationId());

        // As the navigation is still ongoing at this point there should be a NavigationImpl
        // instance for it.
        //assert navigation != null;

        switch (overrideUrlLoadingResult.getResultType()) {
            case OverrideUrlLoadingResultType.OVERRIDE_WITH_EXTERNAL_INTENT:
                Log.e(TAG, "Miguelao intent");
                //navigation.setIntentLaunched();
                break;
            case OverrideUrlLoadingResultType.OVERRIDE_WITH_ASYNC_ACTION:
                Log.e(TAG, "Miguelao async thing");
                if (overrideUrlLoadingResult.getAsyncActionType()
                        == OverrideUrlLoadingAsyncActionType.UI_GATING_INTENT_LAUNCH) {
                    //navigation.setIsUserDecidingIntentLaunch();
                }
                break;
            case OverrideUrlLoadingResultType.OVERRIDE_WITH_NAVIGATE_TAB:
            case OverrideUrlLoadingResultType.NO_OVERRIDE:
            default:
                Log.e(TAG, "Miguelao normal stuff");
                break;
        }
    }
    @Override
    public void loadUrlIfPossible(LoadUrlParams loadUrlParams) {
        Log.e(TAG, "Miguelao loadUrlIfPossible");
    }
}
