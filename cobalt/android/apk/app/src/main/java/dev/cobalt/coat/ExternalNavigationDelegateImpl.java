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

import android.content.Context;
import android.content.Intent;
import android.content.pm.ResolveInfo;
import android.os.RemoteException;

import org.chromium.base.Callback;
import org.chromium.base.supplier.Supplier;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.external_intents.ExternalNavigationDelegate;
import org.chromium.content_public.browser.Visibility;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;

import java.util.List;

/**
 * Cobalt's implementation of the {@link ExternalNavigationDelegate}.
 */
public class ExternalNavigationDelegateImpl implements ExternalNavigationDelegate {
    private WebContents mWebContents;

    public ExternalNavigationDelegateImpl(WebContents webContents) {
        assert webContents != null;
        mWebContents = webContents;
    }

    @Override
    public Context getContext() {
        return null; // mcasas: WebContents don't have this
    }

    @Override
    public boolean willAppHandleIntent(Intent intent) {
        return false;
    }

    @Override
    public boolean shouldDisableExternalIntentRequestsForUrl(GURL url) {
        return false;
    }

    @Override
    public boolean shouldAvoidDisambiguationDialog(GURL intentDataUrl) {
        return false;
    }

    @Override
    public boolean isApplicationInForeground() {
        return mWebContents.getVisibility() == Visibility.VISIBLE;
    }

    @Override
    public void maybeSetWindowId(Intent intent) {}

    @Override
    public boolean canLoadUrlInCurrentTab() {
        return true;
    }

    @Override
    public void closeTab() {}

    @Override
    public boolean isIncognito() {
        return mWebContents.isIncognito();
    }

    @Override
    public boolean hasCustomLeavingIncognitoDialog() {
        return false;
    }

    @Override
    public void presentLeavingIncognitoModalDialog(Callback<Boolean> onUserDecision) {
    }

    @Override
    public void maybeSetRequestMetadata(
            Intent intent, boolean hasUserGesture, boolean isRendererInitiated) {}

    @Override
    public void maybeSetPendingReferrer(Intent intent, GURL referrerUrl) {}

    @Override
    public void maybeSetPendingIncognitoUrl(Intent intent) {}

    @Override
    public WindowAndroid getWindowAndroid() {
        return mWebContents.getTopLevelNativeWindow();
    }

    @Override
    public WebContents getWebContents() {
        return mWebContents;
    }

    @Override
    public boolean hasValidTab() {
        return true;
    }

    @Override
    public boolean canCloseTabOnIncognitoIntentLaunch() {
        return true;
    }

    @Override
    public boolean isForTrustedCallingApp(Supplier<List<ResolveInfo>> resolveInfoSupplier) {
        return false;
    }

    @Override
    public boolean shouldLaunchWebApksOnInitialIntent() {
        return false;
    }

    @Override
    public void setPackageForTrustedCallingApp(Intent intent) {
        assert false;
    }

    @Override
    public boolean shouldEmbedderInitiatedNavigationsStayInBrowser() {
        return false;
    }
}
