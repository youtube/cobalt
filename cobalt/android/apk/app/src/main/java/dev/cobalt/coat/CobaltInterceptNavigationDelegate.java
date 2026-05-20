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

import static dev.cobalt.util.Log.TAG;

import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import java.lang.ref.WeakReference;
import org.chromium.ui.base.WindowAndroid;
import dev.cobalt.util.Log;
import org.chromium.base.RequiredCallback;
import org.chromium.components.navigation_interception.InterceptNavigationDelegate;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.url.GURL;

/**
 * Intercepts navigations in the WebView to handle non-standard web schemes
 * (e.g., "nflx://", "intent://") and routes them to external Android applications.
 *
 * Lifetime and Ownership: This delegate is associated with and owned by the
 * {@link org.chromium.content_public.browser.WebContents} lifecycle. It is registered
 * via {@link CobaltContentBrowserClient#associateInterceptNavigationDelegate}.
 *
 * Threading Model: This class is UI thread-affine. All methods, including
 * {@link #shouldIgnoreNavigation}, are expected to be called on the Android UI thread,
 * as they perform UI-related operations like starting activities.
 */
public class CobaltInterceptNavigationDelegate extends InterceptNavigationDelegate {
    private final Context mApplicationContext;
    private final WeakReference<Context> mFallbackContextRef;

    public CobaltInterceptNavigationDelegate(Context context) {
        mApplicationContext = context.getApplicationContext();
        mFallbackContextRef = new WeakReference<>(context);
    }

    private Context getActiveContext(NavigationHandle navigationHandle) {
        if (navigationHandle != null && navigationHandle.getWebContents() != null) {
            WindowAndroid window = navigationHandle.getWebContents().getTopLevelNativeWindow();
            if (window != null && window.getContext() != null) {
                Context context = window.getContext().get();
                if (context != null) {
                    return context;
                }
            }
        }
        return mFallbackContextRef.get();
    }

    private enum OverrideResult {
        NO_OVERRIDE,
        OVERRIDE_WITH_EXTERNAL_INTENT
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
        Context context = getActiveContext(navigationHandle);

        // Fallback for Application Context if no Activity context is active
        if (context == null) {
            context = mApplicationContext;
        }

        OverrideResult result = shouldOverrideUrlLoading(context, escapedUrl, scheme);
        onShouldIgnoreNavigationResult(context, result, escapedUrl, resultCallback);
    }

    private OverrideResult shouldOverrideUrlLoading(Context context, GURL escapedUrl, String scheme) {
        if (scheme.equals("http") || scheme.equals("https")) {
            return OverrideResult.NO_OVERRIDE;
        }

        return OverrideResult.OVERRIDE_WITH_EXTERNAL_INTENT;
    }

    private void onShouldIgnoreNavigationResult(
            Context context,
            OverrideResult result,
            GURL escapedUrl,
            RequiredCallback<Boolean> resultCallback) {
        boolean shouldIgnore;
        switch (result) {
            case OVERRIDE_WITH_EXTERNAL_INTENT:
                // Synchronous launch
                handleExternalURL(context, escapedUrl);
                shouldIgnore = true;
                break;
            case NO_OVERRIDE:
            default:
                shouldIgnore = false;
                break;
        }
        resultCallback.onResult(shouldIgnore);
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
