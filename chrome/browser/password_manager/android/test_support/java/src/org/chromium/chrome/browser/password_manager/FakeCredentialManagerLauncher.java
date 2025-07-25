// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import android.app.PendingIntent;
import android.content.Intent;

import org.chromium.base.Callback;

/**
 * Fake {@link CredentialManagerLauncher} to be used in integration tests.
 */
public class FakeCredentialManagerLauncher implements CredentialManagerLauncher {
    private PendingIntent mPendingIntent;
    private Exception mException;

    public void setIntent(PendingIntent pendingIntent) {
        mPendingIntent = pendingIntent;
    }
    public void setCredentialManagerError(Exception exception) {
        mException = exception;
    }

    @Override
    public void getAccountCredentialManagerIntent(@ManagePasswordsReferrer int referrer,
            String accountName, Callback<PendingIntent> successCallback,
            Callback<Exception> failureCallback) {
        if (accountName == null) {
            failureCallback.onResult(new CredentialManagerBackendException(
                    "Called without an account", CredentialManagerError.NO_ACCOUNT_NAME));
            return;
        }
        getCredentialManagerLaunchIntent(successCallback, failureCallback);
    }

    @Override
    public void getLocalCredentialManagerIntent(@ManagePasswordsReferrer int referrer,
            Callback<PendingIntent> successCallback, Callback<Exception> failureCallback) {
        getCredentialManagerLaunchIntent(successCallback, failureCallback);
    }

    private void getCredentialManagerLaunchIntent(
            Callback<PendingIntent> successCallback, Callback<Exception> failureCallback) {
        if (mException != null) {
            failureCallback.onResult(mException);
            return;
        }
        successCallback.onResult(mPendingIntent);
    }

    @Override
    public void getAccountSettingsIntent(String accountName, Callback<Intent> completionCallback) {
        // This is not currently used in tests.
        assert false;
    }
}
