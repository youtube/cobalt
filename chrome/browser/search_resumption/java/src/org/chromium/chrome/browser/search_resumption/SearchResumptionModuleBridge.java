// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_resumption;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.url.GURL;

/**
 * Bridge, providing access to the native-side functionalities to fetch search suggestions.
 */
@JNINamespace("search_resumption_module")
public class SearchResumptionModuleBridge {
    interface OnSuggestionsReceivedCallback {
        void onSuggestionsReceived(String[] suggestionTexts, GURL[] suggestionUrls);
    }
    private long mSearchResumptionModuleBridge;
    private OnSuggestionsReceivedCallback mCallback;

    public SearchResumptionModuleBridge(Profile profile) {
        mSearchResumptionModuleBridge = SearchResumptionModuleBridgeJni.get().create(
                SearchResumptionModuleBridge.this, profile);
    }

    void fetchSuggestions(String url, OnSuggestionsReceivedCallback callback) {
        if (mSearchResumptionModuleBridge == 0) return;

        mCallback = callback;
        SearchResumptionModuleBridgeJni.get().fetchSuggestions(
                mSearchResumptionModuleBridge, SearchResumptionModuleBridge.this, url);
    }

    void destroy() {
        if (mSearchResumptionModuleBridge != 0) {
            SearchResumptionModuleBridgeJni.get().destroy(
                    mSearchResumptionModuleBridge, SearchResumptionModuleBridge.this);
            mSearchResumptionModuleBridge = 0;
        }
    }

    @CalledByNative
    void onSuggestionsReceived(String[] suggestionTexts, GURL[] suggestionUrls) {
        mCallback.onSuggestionsReceived(suggestionTexts, suggestionUrls);
    }

    @NativeMethods
    interface Natives {
        long create(SearchResumptionModuleBridge caller, Profile profile);
        void fetchSuggestions(long nativeSearchResumptionModuleBridge,
                SearchResumptionModuleBridge caller, String url);
        void destroy(long nativeSearchResumptionModuleBridge, SearchResumptionModuleBridge caller);
    }
}
