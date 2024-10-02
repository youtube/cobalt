// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.verification;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.StrictModeContext;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.components.content_relationship_verification.VerificationResultStore;

import java.util.HashSet;
import java.util.Set;

/**
 * ChromeVerificationResultStore stores relationships to SharedPreferences which are therefore
 * persisted across Chrome launches.
 */
public class ChromeVerificationResultStore extends VerificationResultStore {
    // If we constructed this lazily (creating a new instance in getInstance, that would open us
    // up to a possible race condition if getInstance is called on multiple threads. We could solve
    // this with an AtomicReference, but it seems simpler to just eagerly create the instance.
    private static final ChromeVerificationResultStore sInstance =
            new ChromeVerificationResultStore();

    private ChromeVerificationResultStore() {}

    public static ChromeVerificationResultStore getInstance() {
        return sInstance;
    }

    @Override
    @VisibleForTesting
    public Set<String> getRelationships() {
        // In case we're called on the UI thread and Preferences haven't been read before.
        try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
            // From the official docs, modifying the result of a SharedPreferences.getStringSet can
            // cause bad things to happen including exceptions or ruining the data.
            return new HashSet<>(SharedPreferencesManager.getInstance().readStringSet(
                    ChromePreferenceKeys.VERIFIED_DIGITAL_ASSET_LINKS));
        }
    }

    @Override
    @VisibleForTesting
    public void setRelationships(Set<String> relationships) {
        SharedPreferencesManager.getInstance().writeStringSet(
                ChromePreferenceKeys.VERIFIED_DIGITAL_ASSET_LINKS, relationships);
    }

    @VisibleForTesting
    public static ChromeVerificationResultStore getInstanceForTesting() {
        return getInstance();
    }
}
