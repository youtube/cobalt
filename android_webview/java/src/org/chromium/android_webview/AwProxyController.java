// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import org.chromium.base.annotations.CalledByNativeUnchecked;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;

import java.util.concurrent.Executor;

/**
 * Manages proxy override functionality in WebView.
 */
@JNINamespace("android_webview")
public class AwProxyController {

    public AwProxyController() {}

    public void setProxyOverride(String[][] proxyRules, String[] bypassRules, Runnable listener,
            Executor executor, boolean reverseBypass) {
        int length = (proxyRules == null ? 0 : proxyRules.length);
        String[] urlSchemes = new String[length];
        String[] proxyUrls = new String[length];
        for (int i = 0; i < length; i++) {
            String urlSchemeFilter = proxyRules[i][0];
            String proxyUrl = proxyRules[i][1];

            if (urlSchemeFilter == null) {
                urlSchemeFilter = "*";
            }
            urlSchemes[i] = urlSchemeFilter;

            if (proxyUrl == null) {
                throw new IllegalArgumentException("Proxy rule " + i + " has a null url");
            }
            proxyUrls[i] = proxyUrl;
        }
        length = (bypassRules == null ? 0 : bypassRules.length);
        for (int i = 0; i < length; i++) {
            if (bypassRules[i] == null) {
                throw new IllegalArgumentException("Bypass rule " + i + " is null");
            }
        }
        if (executor == null) {
            throw new IllegalArgumentException("Executor must not be null");
        }

        String result = AwProxyControllerJni.get().setProxyOverride(AwProxyController.this,
                urlSchemes, proxyUrls, bypassRules, listener, executor, reverseBypass);
        if (!result.isEmpty()) {
            throw new IllegalArgumentException(result);
        }
    }

    public void clearProxyOverride(Runnable listener, Executor executor) {
        if (executor == null) {
            throw new IllegalArgumentException("Executor must not be null");
        }

        AwProxyControllerJni.get().clearProxyOverride(AwProxyController.this, listener, executor);
    }

    @CalledByNativeUnchecked
    private void proxyOverrideChanged(Runnable listener, Executor executor) {
        if (listener == null) return;
        executor.execute(listener);
    }

    @NativeMethods
    interface Natives {
        String setProxyOverride(AwProxyController caller, String[] urlSchemes, String[] proxyUrls,
                String[] bypassRules, Runnable listener, Executor executor, boolean reverseBypass);
        void clearProxyOverride(AwProxyController caller, Runnable listener, Executor executor);
    }
}
