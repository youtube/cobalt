// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import android.app.PendingIntent;

import org.chromium.base.Callback;

import java.util.Optional;

/**
 * Fake {@link PasswordCheckupClientHelper} to be used in integration tests.
 */
public class FakePasswordCheckupClientHelper implements PasswordCheckupClientHelper {
    private PendingIntent mPendingIntent;
    private Integer mBreachedCredentialsCount;
    private Exception mError;

    public void setIntent(PendingIntent pendingIntent) {
        mPendingIntent = pendingIntent;
    }

    public void setBreachedCredentialsCount(Integer count) {
        mBreachedCredentialsCount = count;
    }

    public void setError(Exception error) {
        mError = error;
    }

    @Override
    public void getPasswordCheckupIntent(@PasswordCheckReferrer int referrer,
            Optional<String> accountName, Callback<PendingIntent> successCallback,
            Callback<Exception> failureCallback) {
        if (mError != null) {
            failureCallback.onResult(mError);
            return;
        }
        successCallback.onResult(mPendingIntent);
    }

    @Override
    public void runPasswordCheckupInBackground(@PasswordCheckReferrer int referrer,
            Optional<String> accountName, Callback<Void> successCallback,
            Callback<Exception> failureCallback) {
        if (mError != null) {
            failureCallback.onResult(mError);
            return;
        }
        successCallback.onResult(null);
    }

    @Override
    public void getBreachedCredentialsCount(@PasswordCheckReferrer int referrer,
            Optional<String> accountName, Callback<Integer> successCallback,
            Callback<Exception> failureCallback) {
        if (mError != null) {
            failureCallback.onResult(mError);
            return;
        }
        successCallback.onResult(mBreachedCredentialsCount);
    }
}
