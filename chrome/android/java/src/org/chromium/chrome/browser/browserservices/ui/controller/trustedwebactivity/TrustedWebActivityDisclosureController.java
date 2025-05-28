// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.ui.controller.trustedwebactivity;

import org.chromium.chrome.browser.browserservices.BrowserServicesStore;
import org.chromium.chrome.browser.browserservices.metrics.TrustedWebActivityUmaRecorder;
import org.chromium.chrome.browser.browserservices.ui.TrustedWebActivityModel;
import org.chromium.chrome.browser.browserservices.ui.controller.CurrentPageVerifier;
import org.chromium.chrome.browser.browserservices.ui.controller.DisclosureController;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;

/**
 * Controls when Trusted Web Activity disclosure should be shown and hidden, reacts to interaction
 * with it.
 */
public class TrustedWebActivityDisclosureController extends DisclosureController {
    private final ClientPackageNameProvider mClientPackageNameProvider;

    public TrustedWebActivityDisclosureController(
            TrustedWebActivityModel model,
            ActivityLifecycleDispatcher lifecycleDispatcher,
            CurrentPageVerifier currentPageVerifier,
            ClientPackageNameProvider clientPackageNameProvider) {
        super(model, lifecycleDispatcher, currentPageVerifier, clientPackageNameProvider.get());
        mClientPackageNameProvider = clientPackageNameProvider;
    }

    @Override
    public void onDisclosureAccepted() {
        TrustedWebActivityUmaRecorder.recordDisclosureAccepted();
        BrowserServicesStore.setUserAcceptedTwaDisclosureForPackage(
                mClientPackageNameProvider.get());
        super.onDisclosureAccepted();
    }

    @Override
    public void onDisclosureShown() {
        TrustedWebActivityUmaRecorder.recordDisclosureShown();
        BrowserServicesStore.setUserSeenTwaDisclosureForPackage(mClientPackageNameProvider.get());
        super.onDisclosureShown();
    }

    @Override
    protected boolean shouldShowDisclosure() {
        /* Has a disclosure been dismissed for this client package before? */
        return !BrowserServicesStore.hasUserAcceptedTwaDisclosureForPackage(
                mClientPackageNameProvider.get());
    }

    @Override
    protected boolean isFirstTime() {
        return !BrowserServicesStore.hasUserSeenTwaDisclosureForPackage(
                mClientPackageNameProvider.get());
    }
}
