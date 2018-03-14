// Copyright 2017 Google Inc. All Rights Reserved.
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

package dev.cobalt.account;

import static android.Manifest.permission.GET_ACCOUNTS;
import static dev.cobalt.util.Log.TAG;

import android.accounts.Account;
import android.accounts.AccountManager;
import android.accounts.OnAccountsUpdateListener;
import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.PackageManager;
import android.os.Handler;
import android.os.Looper;
import android.support.v4.app.ActivityCompat;
import android.support.v4.content.ContextCompat;
import android.text.TextUtils;
import android.util.Log;
import android.widget.Toast;
import com.google.android.gms.auth.GoogleAuthException;
import com.google.android.gms.auth.GoogleAuthUtil;
import com.google.android.gms.common.AccountPicker;
import dev.cobalt.coat.R;
import dev.cobalt.util.Holder;
import dev.cobalt.util.UsedByNative;
import java.io.IOException;

/**
 * Java side implementation for starboard::android::shared::cobalt::AndroidUserAuthorizer.
 *
 * This implements the following business logic:
 * First run...
 * - if there are no accounts, just be be signed-out
 * - if there is one account, sign-in without any UI
 * - if there are more than one accounts, prompt to choose account
 * Subsequent runs...
 * - sign-in to the same account last used to sign-in
 * - if previously signed-out stay signed-out
 * When user clicks 'sign-in' in the UI...
 * - if there are no accounts, allow user to add an account
 * - if there is one account, sign-in without any UI
 * - if there are more than one accounts, prompt to choose account
 * If the last signed-in account is deleted...
 * - kill the app if stopped in the background to prompt next time it starts
 * - at the next app start, show a toast that the account isn't available
 * - if there are no accounts left, just be be signed-out
 * - if there are one or more accounts left, prompt to choose account
 */
public class UserAuthorizerImpl implements OnAccountsUpdateListener, UserAuthorizer {

  /** Pseudo account indicating the user chose to be signed-out. */
  public static final Account SIGNED_OUT_ACCOUNT = new Account("-", "-");

  /** Pseudo account indicating a saved account no longer exists. */
  private static final Account MISSING_ACCOUNT = new Account("!", "!");

  /** Foreshortened expiry of Google OAuth token, which typically lasts 1 hour. */
  private static final long DEFAULT_EXPIRY_SECONDS = 5 * 60;

  private static final String GOOGLE_ACCOUNT_TYPE = "com.google";
  private static final String[] OAUTH_SCOPES = {
      "https://www.googleapis.com/auth/youtube"
  };

  private static final String SHARED_PREFS_NAME = "user_auth";
  private static final String ACCOUNT_NAME_PREF_KEY = "signed_in_account";

  /** The thread on which the current request is running, or null if none. */
  private volatile Thread requestThread;

  private final Context appContext;
  private final Holder<Activity> activityHolder;
  private final Runnable stopRequester;
  private final Handler mainHandler;

  private Account currentAccount = null;
  private AccessToken currentToken = null;

  // Result from the account picker UI lands here.
  private String chosenAccountName;

  private volatile boolean waitingForPermission;
  private volatile boolean permissionGranted;

  public UserAuthorizerImpl(
      Context appContext, Holder<Activity> activityHolder, Runnable stopRequester) {
    this.appContext = appContext;
    this.activityHolder = activityHolder;
    this.stopRequester = stopRequester;
    this.mainHandler = new Handler(Looper.getMainLooper());
    addOnAccountsUpdatedListener(this);
  }

  @Override
  public void shutdown() {
    removeOnAccountsUpdatedListener(this);
  }

  @Override
  @SuppressWarnings("unused")
  @UsedByNative
  public void interrupt() {
    Thread t = requestThread;
    if (t != null) {
      t.interrupt();
    }
  }

  @Override
  @SuppressWarnings("unused")
  @UsedByNative
  public AccessToken authorizeUser() {
    ensureBackgroundThread();
    requestThread = Thread.currentThread();
    // Let the user choose an account, or add one if there are none to choose.
    // However, if there's only one account just choose it without any prompt.
    currentAccount = autoSelectOrAddAccount();
    writeAccountPref(currentAccount);
    AccessToken accessToken = refreshCurrentToken();
    requestThread = null;
    return accessToken;
  }

  @Override
  @SuppressWarnings("unused")
  @UsedByNative
  public boolean deauthorizeUser() {
    ensureBackgroundThread();
    requestThread = Thread.currentThread();
    currentAccount = SIGNED_OUT_ACCOUNT;
    writeAccountPref(currentAccount);
    clearCurrentToken();
    requestThread = null;
    return true;
  }

  @Override
  @SuppressWarnings("unused")
  @UsedByNative
  public AccessToken refreshAuthorization() {
    ensureBackgroundThread();
    requestThread = Thread.currentThread();

    // If we haven't yet determined which account to use, check preferences for a saved account.
    if (currentAccount == null) {
      Account savedAccount = readAccountPref();
      if (savedAccount == null) {
        // No saved account, so this is the first ever run of the app.
        currentAccount = autoSelectAccount();
      } else if (savedAccount.equals(MISSING_ACCOUNT)) {
        // The saved account got deleted.
        currentAccount = forceSelectAccount();
      } else {
        // Use the saved account.
        currentAccount = savedAccount;
      }
      writeAccountPref(currentAccount);
    }

    AccessToken accessToken = refreshCurrentToken();
    requestThread = null;
    return accessToken;
  }

  private static void ensureBackgroundThread() {
    if (Looper.myLooper() == Looper.getMainLooper()) {
      throw new UnsupportedOperationException("UserAuthorizer can't be called on main thread");
    }
  }

  private void showToast(int resId, Object... formatArgs) {
    final String msg = appContext.getResources().getString(resId, formatArgs);
    mainHandler.post(new Runnable() {
      @Override
      public void run() {
        Toast.makeText(appContext, msg, Toast.LENGTH_LONG).show();
      }
    });
  }

  private Account readAccountPref() {
    String savedAccountName = loadSignedInAccountName();
    if (TextUtils.isEmpty(savedAccountName)) {
      return null;
    } else if (savedAccountName.equals(SIGNED_OUT_ACCOUNT.name)) {
      // Don't request permissions or look for a device account if we were signed-out.
      return SIGNED_OUT_ACCOUNT;
    } else if (!checkPermission()) {
      // We won't be able to get the account without permission, so warn the user and be signed-out.
      showToast(R.string.starboard_missing_account, savedAccountName);
      return SIGNED_OUT_ACCOUNT;
    } else {
      // Find the saved account name among all accounts on the device.
      for (Account account : getAccounts()) {
        if (account.name.equals(savedAccountName)) {
          return account;
        }
      }
      showToast(R.string.starboard_missing_account, savedAccountName);
      return MISSING_ACCOUNT;
    }
  }

  private void writeAccountPref(Account account) {
    if (account == null) {
      return;
    }
    // Always write the account name, even if it's the signed-out pseudo account.
    saveSignedInAccountName(account.name);
  }

  private void clearCurrentToken() {
    if (currentToken != null) {
      clearToken(currentToken.getTokenValue());
      currentToken = null;
    }
  }

  private AccessToken refreshCurrentToken() {
    clearCurrentToken();
    if (currentAccount == null || SIGNED_OUT_ACCOUNT.equals(currentAccount)) {
      return null;
    }
    String tokenValue = getToken(currentAccount);
    if (tokenValue == null) {
      showToast(R.string.starboard_account_auth_error);
      tokenValue = "";
    }
    // TODO: Get the token details and use the actual expiry.
    long expiry = System.currentTimeMillis() / 1000 + DEFAULT_EXPIRY_SECONDS;
    currentToken = new AccessToken(tokenValue, expiry);
    return currentToken;
  }

  /**
   * Prompts the user to select an account, or to add an account if there are none. The prompt is
   * skipped if there is exactly one account to choose from.
   */
  private Account autoSelectOrAddAccount() {
    if (!checkPermission()) {
      return SIGNED_OUT_ACCOUNT;
    }
    Account[] accounts = getAccounts();
    if (accounts.length == 1) {
      return accounts[0];
    }
    return selectOrAddAccount();
  }

  /**
   * Prompts the user to select an account. The prompt is skipped if there are zero or one accounts
   * to choose from.
   */
  private Account autoSelectAccount() {
    if (!checkPermission()) {
      return SIGNED_OUT_ACCOUNT;
    }
    Account[] accounts = getAccounts();
    if (accounts.length == 0) {
      return SIGNED_OUT_ACCOUNT;
    } else if (accounts.length == 1) {
      return accounts[0];
    }
    return selectOrAddAccount();
  }

  /**
   * Prompts the user to select an account, even if there's only one to choose from. The prompt is
   * skipped if there are zero accounts to choose from.
   */
  private Account forceSelectAccount() {
    // We don't check permissions before calling selectOrAddAccount() because if the account is
    // missing, readAccountPref() must have just checked, and we don't want to show permission
    // prompt or rationale to the user twice.
    Account[] accounts = getAccounts();
    if (accounts.length == 0) {
      return SIGNED_OUT_ACCOUNT;
    }
    return selectOrAddAccount();
  }

  /**
   * Prompts the user to select an account, even if there's only one to choose from. If there are
   * zero accounts to choose from, the user is prompted to add one.
   *
   * The caller should ensure permissions are granted before calling this method to avoid showing
   * a picker with accounts that we can't access.
   */
  private Account selectOrAddAccount() {
    String accountName = showAccountPicker();
    // If user cancelled the picker stay signed-out.
    if (TextUtils.isEmpty(accountName)) {
      return SIGNED_OUT_ACCOUNT;
    }
    // Get the accounts after the picker in case one was added in the account picker.
    for (Account account : getAccounts()) {
      if (account.name.equals(accountName)) {
        return account;
      }
    }
    // This shouldn't happen, but if it does let the user know we're still signed-out.
    Log.e(TAG, "Selected account is missing");
    showToast(R.string.starboard_missing_account, accountName);
    return SIGNED_OUT_ACCOUNT;
  }

  private synchronized String showAccountPicker() {
    Activity activity = activityHolder.get();
    Intent chooseAccountIntent = newChooseAccountIntent(currentAccount);
    if (activity == null || chooseAccountIntent == null) {
      return "";
    }
    chosenAccountName = null;
    activity.startActivityForResult(chooseAccountIntent, R.id.rc_choose_account);

    // Block until the account picker activity returns its result.
    while (chosenAccountName == null) {
      try {
        wait();
      } catch (InterruptedException e) {
        Log.e(TAG, "Account picker interrupted");
        // Return empty string, as if the picker was cancelled.
        return "";
      }
    }
    return chosenAccountName;
  }

  @Override
  public void onActivityResult(int requestCode, int resultCode, Intent data) {
    if (requestCode == R.id.rc_choose_account) {
      String accountName = null;
      if (resultCode == Activity.RESULT_OK) {
        accountName = data.getStringExtra(AccountManager.KEY_ACCOUNT_NAME);
      } else if (resultCode != Activity.RESULT_CANCELED) {
        Log.e(TAG, "Account picker error " + resultCode);
        showToast(R.string.starboard_account_picker_error);
      }

      // Notify showAccountPicker() which account was chosen.
      synchronized (this) {
        // Return empty string if the picker is cancelled or there's an unexpected result.
        chosenAccountName = (accountName == null) ? "" : accountName;
        notifyAll();
      }
    }
  }

  @Override
  public void onAccountsUpdated(Account[] unused) {
    if (currentAccount == null || SIGNED_OUT_ACCOUNT.equals(currentAccount)) {
      // We're not signed-in; the update doesn't affect us.
      return;
    }
    // Call getAccounts() since the param may not match the accounts we can access.
    for (Account account : getAccounts()) {
      if (account.name.equals(currentAccount.name)) {
        // The current account is still there; the update doesn't affect us.
        return;
      }
    }
    // The current account is gone; leave the app so we prompt for sign-in next time.
    // This should only happen while stopped in the background since we don't delete accounts.
    stopRequester.run();
  }

  /**
   * Calls framework AccountManager.addOnAccountsUpdatedListener().
   */
  private void addOnAccountsUpdatedListener(OnAccountsUpdateListener listener) {
    AccountManager.get(appContext).addOnAccountsUpdatedListener(listener, null, false);
  }

  /**
   * Calls framework AccountManager.removeOnAccountsUpdatedListener().
   */
  private void removeOnAccountsUpdatedListener(OnAccountsUpdateListener listener) {
    AccountManager.get(appContext).removeOnAccountsUpdatedListener(listener);
  }

  /**
   * Calls framework AccountManager.getAccountsByType() for Google accounts.
   */
  private Account[] getAccounts() {
    return AccountManager.get(appContext).getAccountsByType(GOOGLE_ACCOUNT_TYPE);
  }

  /**
   * Calls GMS AccountPicker.newChooseAccountIntent().
   *
   * Returns an Intent that when started will always show the account picker even if there's just
   * one account on the device. If there are no accounts on the device it shows the UI to add one.
   */
  private Intent newChooseAccountIntent(Account defaultAccount) {
    String[] allowableAccountTypes = {GOOGLE_ACCOUNT_TYPE};
    return AccountPicker.newChooseAccountIntent(
        defaultAccount, null, allowableAccountTypes, true, null, null, null, null);
  }

  /**
   * Calls GMS GoogleAuthUtil.getToken(), without throwing any exceptions.
   *
   * Returns an empty string if no token is available for the account.
   * Returns null if there was an error getting the token.
   */
  private String getToken(Account account) {
    String joinedScopes = "oauth2:" + TextUtils.join(" ", OAUTH_SCOPES);
    try {
      return GoogleAuthUtil.getToken(appContext, account, joinedScopes);
    } catch (IOException | GoogleAuthException e) {
      Log.e(TAG, "Error getting auth token", e);
      return null;
    }
  }

  /**
   * Calls GMS GoogleAuthUtil.clearToken(), without throwing any exceptions.
   */
  private void clearToken(String tokenValue) {
    try {
      GoogleAuthUtil.clearToken(appContext, tokenValue);
    } catch (GoogleAuthException | IOException e) {
      Log.e(TAG, "Error clearing auth token", e);
    }
  }

  /**
   * Checks whether the app has necessary permissions, asking for them if needed.
   *
   * This blocks until permissions are granted/declined, and should not be called on the UI thread.
   *
   * Returns true if permissions are granted.
   */
  private synchronized boolean checkPermission() {
    if (ContextCompat.checkSelfPermission(appContext, GET_ACCOUNTS)
        == PackageManager.PERMISSION_GRANTED) {
      return true;
    }

    final Activity activity = activityHolder.get();
    if (activity == null) {
      return false;
    }

    // Check if we have previously been denied permission.
    if (ActivityCompat.shouldShowRequestPermissionRationale(activity, GET_ACCOUNTS)) {
      activity.runOnUiThread(new Runnable() {
        @Override
        public void run() {
          Toast.makeText(activity, R.string.starboard_accounts_permission, Toast.LENGTH_LONG)
              .show();
        }
      });
      return false;
    }

    // Request permission.
    waitingForPermission = true;
    permissionGranted = false;
    ActivityCompat.requestPermissions(
        activity, new String[]{GET_ACCOUNTS}, R.id.rc_get_accounts_permission);
    try {
      while (waitingForPermission) {
        wait();
      }
    } catch (InterruptedException e) {
      return false;
    }
    return permissionGranted;
  }

  /**
   * Callback pass-thru from the Activity with the result from requesting permissions.
   */
  @Override
  public void onRequestPermissionsResult(
      int requestCode, String[] permissions, int[] grantResults) {
    if (requestCode == R.id.rc_get_accounts_permission) {
      synchronized (this) {
        permissionGranted = grantResults.length > 0
            && grantResults[0] == PackageManager.PERMISSION_GRANTED;
        waitingForPermission = false;
        notifyAll();
      }
    }
  }

  /**
   * Remember the name of the signed-in account.
   */
  private void saveSignedInAccountName(String accountName) {
    getPreferences().edit().putString(ACCOUNT_NAME_PREF_KEY, accountName).commit();
  }

  /**
   * Returns the remembered name of the signed-in account.
   */
  private String loadSignedInAccountName() {
    return getPreferences().getString(ACCOUNT_NAME_PREF_KEY, "");
  }

  private SharedPreferences getPreferences() {
    return appContext.getSharedPreferences(SHARED_PREFS_NAME, 0);
  }
}
