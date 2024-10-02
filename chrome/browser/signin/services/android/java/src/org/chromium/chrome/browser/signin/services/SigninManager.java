// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin.services;

import android.accounts.Account;

import androidx.annotation.MainThread;
import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.components.signin.base.CoreAccountId;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.signin.metrics.SignoutReason;

/**
 * Android wrapper of the SigninManager which provides access from the Java layer.
 * <p/>
 * This class handles common paths during the sign-in and sign-out flows.
 * <p/>
 * Only usable from the UI thread as the native SigninManager requires its access to be in the
 * UI thread.
 * <p/>
 * See chrome/browser/android/signin/signin_manager_android.h for more details.
 */
public interface SigninManager {
    /**
     * A SignInStateObserver is notified when the user signs in to or out of Chrome.
     */
    interface SignInStateObserver {
        /**
         * Invoked when the user has signed in to Chrome.
         */
        default void onSignedIn() {}

        /**
         * Invoked when the user has signed out of Chrome.
         */
        default void onSignedOut() {}

        /**
         * Invoked once all startup checks are done and signing-in becomes allowed, or disallowed.
         */
        default void onSignInAllowedChanged() {}

        /** Notifies observers when {@link #isSignOutAllowed()} value changes. */
        default void onSignOutAllowedChanged() {}
    }

    /**
     * Callbacks for the sign-in flow.
     */
    interface SignInCallback {
        /**
         * Invoked after sign-in is completed successfully.
         */
        void onSignInComplete();

        /**
         * Invoked if the sign-in processes does not complete for any reason.
         */
        void onSignInAborted();
    }

    /**
     * Callbacks for the sign-out flow.
     */
    interface SignOutCallback {
        /**
         * Called before the data wiping is started.
         */
        default void preWipeData() {}

        /**
         * Called after the data is wiped.
         */
        void signOutComplete();
    }

    /**
     * Extracts the domain name of a given account's email.
     */
    String extractDomainName(String accountEmail);

    /**
     * @return IdentityManager used by SigninManager.
     */
    IdentityManager getIdentityManager();

    /**
     * Returns true if sign in can be started now.
     */
    boolean isSigninAllowed();

    /**
     * Returns true if sync opt in can be started now.
     */
    boolean isSyncOptInAllowed();

    /**
     * Returns true if signin is disabled by policy.
     */
    boolean isSigninDisabledByPolicy();

    /**
     * Returns whether the user can sign-in (maybe after an update to Google Play services).
     * @param requireUpdatedPlayServices Indicates whether an updated version of play services is
     *         required or not.
     */
    boolean isSigninSupported(boolean requireUpdatedPlayServices);

    /**
     * @return Whether force sign-in is enabled by policy.
     */
    boolean isForceSigninEnabled();

    /**
     * Adds a {@link SignInStateObserver} to be notified when the user signs in or out of Chrome.
     */
    void addSignInStateObserver(SignInStateObserver observer);

    /**
     * Removes a {@link SignInStateObserver} to be notified when the user signs in or out of Chrome.
     */
    void removeSignInStateObserver(SignInStateObserver observer);

    /**
     * Starts the sign-in flow, and executes the callback when finished.
     *
     * The sign-in flow goes through the following steps:
     *
     *   - Wait for AccountTrackerService to be seeded.
     *   - Complete sign-in with the native IdentityManager.
     *   - Call the callback if provided.
     *  @param account The account to sign in to.
     * @param accessPoint {@link SigninAccessPoint} that initiated the sign-in flow.
     * @param callback Optional callback for when the sign-in process is finished.
     */
    void signin(
            Account account, @SigninAccessPoint int accessPoint, @Nullable SignInCallback callback);

    /**
     * Starts the sign-in flow, and executes the callback when finished.
     *
     * The sign-in flow goes through the following steps:
     *
     *   - Wait for AccountTrackerService to be seeded.
     *   - Wait for policy to be checked for the account.
     *   - If managed, wait for the policy to be fetched.
     *   - Complete sign-in with the native IdentityManager.
     *   - Call the callback if provided.
     *  @param account The account to sign in to.
     * @param accessPoint {@link SigninAccessPoint} that initiated the sign-in flow.
     * @param callback Optional callback for when the sign-in process is finished.
     */
    void signinAndEnableSync(
            Account account, @SigninAccessPoint int accessPoint, @Nullable SignInCallback callback);

    /**
     * Schedules the runnable to be invoked after all sign-in, sign-out, or sync data wipe operation
     * is finished. If there's no operation is progress, posts the callback to the UI thread right
     * away.
     */
    @MainThread
    void runAfterOperationInProgress(Runnable runnable);

    /**
     * Revokes sync consent (which disables the sync feature). This method should only be called
     * for child accounts.
     *
     * @param signoutSource describes the event driving disabling sync (e.g.
     *         {@link SignoutReason.USER_CLICKED_TURN_OFF_SYNC_SETTINGS}).
     * @param signOutCallback Callback to notify about progress.
     * @param forceWipeUserData Whether user selected to wipe all device data.
     */
    void revokeSyncConsent(@SignoutReason int signoutSource, SignOutCallback signOutCallback,
            boolean forceWipeUserData);

    /**
     * Returns true if sign out can be started now.
     */
    boolean isSignOutAllowed();

    /**
     * Invokes signOut with no callback.
     */
    default void signOut(@SignoutReason int signoutSource) {
        signOut(signoutSource, null, false);
    }

    /**
     * Signs out of Chrome. This method clears the signed-in username, stops sync and sends out a
     * sign-out notification on the native side.
     *
     * @param signoutSource describes the event driving the signout (e.g.
     *         {@link SignoutReason#USER_CLICKED_SIGNOUT_SETTINGS}).
     * @param signOutCallback Callback to notify about the sign-out progress.
     * @param forceWipeUserData Whether user selected to wipe all device data.
     */
    void signOut(@SignoutReason int signoutSource, SignOutCallback signOutCallback,
            boolean forceWipeUserData);

    /**
     * Returns the management domain if the signed in account is managed, otherwise returns null.
     */
    String getManagementDomain();

    /**
     * Verifies if the account is managed. Callback may be called either
     * synchronously or asynchronously depending on the availability of the
     * result.
     * TODO(crbug.com/1002408) Update API to use CoreAccountInfo instead of email
     *
     * @param email An email of the account.
     * @param callback The callback that will receive true if the account is managed, false
     *                 otherwise.
     */
    void isAccountManaged(String email, Callback<Boolean> callback);

    /**
     * Reloads all the accounts from the system within the {@link IdentityManager}.
     * @param primaryAccountId {@link CoreAccountId} of the primary account.
     */
    void reloadAllAccountsFromSystem(CoreAccountId primaryAccountId);

    /**
     * Wipes the user's bookmarks and sync data.
     *
     * Callers should make this call within a runAfterOperationInProgress() call in order to ensure
     * serialization of wipe operations.
     *
     * @param wipeDataCallback A callback which will be called once the data is wiped.
     */
    void wipeSyncUserData(Runnable wipeDataCallback);
}
