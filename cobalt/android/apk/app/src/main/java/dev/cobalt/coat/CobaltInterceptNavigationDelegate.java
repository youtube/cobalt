// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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
import android.net.Uri;
import dev.cobalt.util.Log;
import org.chromium.base.RequiredCallback;
import org.chromium.components.navigation_interception.InterceptNavigationDelegate;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.url.GURL;

/**
 * Intercepts navigations to non-standard web schemes (e.g., "nflx://", "intent://")
 * and launches the corresponding external Android applications.
 */
public class CobaltInterceptNavigationDelegate extends InterceptNavigationDelegate {
    private static final String TAG = "CobaltInterceptNav";
    private final Context mContext;

    public CobaltInterceptNavigationDelegate(Context context) {
        mContext = context;
    }

    @Override
    public void shouldIgnoreNavigation(
            NavigationHandle navigationHandle,
            GURL escapedUrl,
            boolean hiddenCrossFrame,
            boolean isSandboxedFrame,
            boolean shouldRunAsync,
            RequiredCallback<Boolean> resultCallback) {
        String scheme = escapedUrl.getScheme();

        // Let the browser handle standard web schemes.
        if (scheme.equals("http") || scheme.equals("https")) {
            if (mContext instanceof CobaltActivity) {
                CobaltActivity activity = (CobaltActivity) mContext;
                String startupUrl = activity.getStartupUrl();
                String startDeepLink = activity.getStartDeepLink();
                String urlSpec = escapedUrl.getSpec();

                // Allow navigation if navigating to the app's initial startup URL or deep link.
                if ((startupUrl != null && urlSpec.equals(startupUrl)) ||
                    (startDeepLink != null && !startDeepLink.isEmpty() && urlSpec.equals(startDeepLink))) {
                    resultCallback.onResult(false);
                    return;
                }
            }
            resultCallback.onResult(false);
            return;
        }

        // For any other scheme, try to handle it ourselves and stop the browser from trying to load it.
        handleExternalURL(mContext, escapedUrl);
        resultCallback.onResult(true);
    }

    public static boolean handleExternalURL(Context context, GURL url) {
        final String urlSpec = url.getSpec();
        Intent intent;
        try {
            if (urlSpec.startsWith("intent:")) {
                intent = Intent.parseUri(urlSpec, Intent.URI_INTENT_SCHEME);
            } else {
                intent = new Intent(Intent.ACTION_VIEW, Uri.parse(urlSpec));
            }
            intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

            if (context.getPackageManager().resolveActivity(intent, 0) != null) {
                try {
                    context.startActivity(intent);
                    return true;
                } catch (Exception e) {
                    Log.e(TAG, "Failed to launch intent: " + intent, e);
                }
            }

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
                    return true;
                }
            }
        } catch (java.net.URISyntaxException e) {
            Log.e(TAG, "Bad URI " + urlSpec, e);
        }
        return true;
    }
}
