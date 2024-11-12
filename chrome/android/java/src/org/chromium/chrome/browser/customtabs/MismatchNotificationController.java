// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import android.content.Context;
import android.content.Intent;

import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.SigninAndHistorySyncActivityLauncherImpl;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.ui.signin.BottomSheetSigninAndHistorySyncCoordinator;
import org.chromium.chrome.browser.ui.signin.SigninAndHistorySyncActivityLauncher;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerBottomSheetStrings;
import org.chromium.chrome.browser.ui.signin.history_sync.HistorySyncConfig;
import org.chromium.components.messages.DismissReason;
import org.chromium.components.messages.MessageBannerProperties;
import org.chromium.components.messages.MessageDispatcher;
import org.chromium.components.messages.MessageDispatcherProvider;
import org.chromium.components.messages.PrimaryActionClickBehavior;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.AccountUtils;
import org.chromium.components.signin.AccountsChangeObserver;
import org.chromium.components.signin.base.CoreAccountId;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;

/** A controller for the account mismatched notice message. */
public class MismatchNotificationController
        implements SigninManager.SignInStateObserver, AccountsChangeObserver {
    private final WindowAndroid mWindowAndroid;
    private final Profile mProfile;
    private final String mAppAccountEmail;
    private final CoreAccountId mAppAccountId;
    private final AccountManagerFacade mAccountManagerFacade;
    private final SigninManager mSigninManager;

    private PropertyModel mMessageProperties;

    @Nullable
    private static MismatchNotificationController sMismatchNotificationControllerForTesting;

    public static MismatchNotificationController get(
            WindowAndroid windowAndroid, Profile profile, String appAccountName) {
        if (sMismatchNotificationControllerForTesting != null) {
            return sMismatchNotificationControllerForTesting;
        }
        return new MismatchNotificationController(windowAndroid, profile, appAccountName);
    }

    private MismatchNotificationController(
            WindowAndroid windowAndroid, Profile profile, String appAccountName) {
        mWindowAndroid = windowAndroid;
        mProfile = profile;
        mAppAccountEmail = appAccountName;
        mAppAccountId =
                IdentityServicesProvider.get()
                        .getIdentityManager(profile)
                        .findExtendedAccountInfoByEmailAddress(appAccountName)
                        .getId();
        mSigninManager = IdentityServicesProvider.get().getSigninManager(mProfile);
        mSigninManager.addSignInStateObserver(this);
        mAccountManagerFacade = AccountManagerFacadeProvider.getInstance();
        mAccountManagerFacade.addObserver(this);
    }

    public void showSignedOutMessage(Context context) {
        // This method will be removed in favor of the override version below.
        showSignedOutMessage(context, null);
    }

    public void showSignedOutMessage(Context context, Callback<Integer> onClose) {
        // TODO(crbug.com/369564573): Hook up |onClose| to return the user action (or lack thereof)
        // with which the notification is closed.
        mMessageProperties =
                new PropertyModel.Builder(MessageBannerProperties.ALL_KEYS)
                        .with(
                                MessageBannerProperties.PRIMARY_BUTTON_TEXT,
                                ContextUtils.getApplicationContext()
                                        .getString(R.string.custom_tabs_signed_out_message_button))
                        .with(
                                MessageBannerProperties.TITLE,
                                ContextUtils.getApplicationContext()
                                        .getString(R.string.custom_tabs_signed_out_message_title))
                        .with(
                                MessageBannerProperties.DESCRIPTION,
                                ContextUtils.getApplicationContext()
                                        .getString(
                                                R.string.custom_tabs_signed_out_message_subtitle))
                        // TODO(crbug.com/369562441): Update the image once specs are finalized.
                        .with(MessageBannerProperties.ICON_RESOURCE_ID, R.drawable.fre_product_logo)
                        .with(
                                MessageBannerProperties.ICON_TINT_COLOR,
                                MessageBannerProperties.TINT_NONE)
                        .with(
                                MessageBannerProperties.ON_PRIMARY_ACTION,
                                () -> handlePrimaryAction(context))
                        .build();

        MessageDispatcher dispatcher = MessageDispatcherProvider.from(mWindowAndroid);
        dispatcher.enqueueWindowScopedMessage(mMessageProperties, /* highPriority= */ false);
    }

    private @PrimaryActionClickBehavior int handlePrimaryAction(Context context) {
        SigninAndHistorySyncActivityLauncher signinAndHistorySyncActivityLauncher =
                SigninAndHistorySyncActivityLauncherImpl.get();

        // TODO(crbug.com/369562441): Update these strings once specs are finalized.
        AccountPickerBottomSheetStrings bottomSheetStrings =
                new AccountPickerBottomSheetStrings.Builder(
                                org.chromium.chrome.browser.ui.signin.R.string
                                        .signin_account_picker_bottom_sheet_title)
                        .setSubtitleStringId(
                                org.chromium.chrome.browser.ui.signin.R.string
                                        .signin_account_picker_bottom_sheet_benefits_subtitle)
                        .build();

        @Nullable
        Intent intent =
                signinAndHistorySyncActivityLauncher.createBottomSheetSigninIntentOrShowError(
                        context,
                        mProfile,
                        bottomSheetStrings,
                        BottomSheetSigninAndHistorySyncCoordinator.NoAccountSigninMode.NO_SIGNIN,
                        BottomSheetSigninAndHistorySyncCoordinator.WithAccountSigninMode
                                .DEFAULT_ACCOUNT_BOTTOM_SHEET,
                        HistorySyncConfig.OptInMode.NONE,
                        SigninAccessPoint.CCT_ACCOUNT_MISMATCH_NOTIFICATION,
                        mAppAccountId);
        if (intent != null) {
            context.startActivity(intent);
        }

        return PrimaryActionClickBehavior.DISMISS_IMMEDIATELY;
    }

    @Override
    public void onSignedIn() {
        dismissMessage();
    }

    @Override
    public void onCoreAccountInfosChanged() {
        assert mAccountManagerFacade.getCoreAccountInfos().isFulfilled();
        final List<CoreAccountInfo> coreAccountInfos =
                mAccountManagerFacade.getCoreAccountInfos().getResult();
        if (AccountUtils.findCoreAccountInfoByEmail(coreAccountInfos, mAppAccountEmail) == null) {
            dismissMessage();
        }
    }

    private void dismissMessage() {
        MessageDispatcher dispatcher = MessageDispatcherProvider.from(mWindowAndroid);
        dispatcher.dismissMessage(mMessageProperties, DismissReason.DISMISSED_BY_FEATURE);
        mAccountManagerFacade.removeObserver(this);
        mSigninManager.removeSignInStateObserver(this);
    }

    public static void setInstanceForTesting(
            MismatchNotificationController mismatchNotificationController) {
        sMismatchNotificationControllerForTesting = mismatchNotificationController;
    }
}
