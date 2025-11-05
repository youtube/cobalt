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

import android.content.Context;
import android.content.Intent;
import android.content.pm.ResolveInfo;
import android.net.Uri;
import dev.cobalt.util.Log;
import java.util.List;
import org.chromium.base.Callback;
import org.chromium.base.supplier.Supplier;
import org.chromium.components.external_intents.ExternalNavigationDelegate;
import org.chromium.content_public.browser.Visibility;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;

/**
 * Cobalt's implementation of the {@link ExternalNavigationDelegate}.
 */
public class ExternalNavigationDelegateImpl implements ExternalNavigationDelegate {
    private WebContents mWebContents;
    private Context mContext;

    public ExternalNavigationDelegateImpl(WebContents webContents, Context context) {
        assert webContents != null;
        mWebContents = webContents;
        mContext = context;
    }

    @Override
    public Context getContext() {
        return mContext;
    }

    @Override
    public boolean willAppHandleIntent(Intent intent) {
        return mContext.getPackageManager().resolveActivity(intent, 0) != null;
    }

    /**
     * Handles a URL that is not a standard web URL (http or https).
     * This can be called from the Activity on startup or from the navigation throttle.
     * @return {@code true} if the URL was handled.
     */
    public static boolean handleExternalURL(Context context, GURL url) {
        final String urlSpec = url.getSpec();
        Intent intent;
        try {
            if (urlSpec.startsWith("intent:")) {
                // This is a complex intent URI, parse it fully.
                intent = Intent.parseUri(urlSpec, Intent.URI_INTENT_SCHEME);
            } else {
                // This is a simple URI scheme (e.g., "nflx://").
                intent = new Intent(Intent.ACTION_VIEW, Uri.parse(urlSpec));
            }
            intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

            // Try to launch the intent as-is (with package if specified).
            if (context.getPackageManager().resolveActivity(intent, 0) != null) {
                context.startActivity(intent);
                return true; // Handled
            }

            // If that failed, try again without the package.
            if (intent.getPackage() != null) {
                intent.setPackage(null);
                if (context.getPackageManager().resolveActivity(intent, 0) != null) {
                    context.startActivity(intent);
                    return true; // Handled
                }
            }

            // If all launch attempts failed, use the fallback URL (only for intent:// URIs).
            if (urlSpec.startsWith("intent:")) {
                String fallbackUrl = intent.getStringExtra("browser_fallback_url");
                if (fallbackUrl != null && !fallbackUrl.isEmpty()) {
                    Intent fallbackIntent = new Intent(Intent.ACTION_VIEW, Uri.parse(fallbackUrl));
                    fallbackIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
                    try {
                        context.startActivity(fallbackIntent);
                    } catch (android.content.ActivityNotFoundException e) {
                        Log.e(TAG, "Could not launch fallback URL: " + fallbackUrl, e);
                    }
                    return true; // Handled
                }
            }
        } catch (java.net.URISyntaxException e) {
            Log.e(TAG, "Bad URI " + urlSpec, e);
        }
        // If we reach here, the intent was not handled, but we return true to prevent the
        // browser from trying to load it as a web page.
        return true;
    }

    @Override
    public boolean shouldDisableExternalIntentRequestsForUrl(GURL url) {
        final String scheme = url.getScheme();

        // Let the browser handle standard web schemes.
        if (scheme.equals("http") || scheme.equals("https")) {
            return false;
        }

        // For any other scheme, we will try to handle it ourselves and stop the browser from trying.
        return handleExternalURL(mContext, url);
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

    @Override
    public void reportIntentToSafeBrowsing(Intent intent) {}

    @Override
    public boolean shouldReturnAsActivityResult(GURL url) {
        return false;
    }

    @Override
    public void returnAsActivityResult(GURL url) {}

    @Override
    public void maybeRecordExternalNavigationSchemeHistogram(GURL url) {}

    @Override
    public void notifyCctPasswordSavingRecorderOfExternalNavigation() {}

    @Override
    public boolean shouldDisableAllExternalIntents() {
        return false;
    }

    @Override
    public String getSelfScheme() {
        return null;
    }
}
