// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import android.accounts.AccountManager;
import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.content.res.Configuration;
import android.graphics.Color;
import android.os.Build;
import android.view.View;
import android.view.Window;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.IntentUtils;
import org.chromium.base.Promise;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.back_press.SecondaryActivityBackPressUma;
import org.chromium.chrome.browser.device_lock.DeviceLockActivityLauncherImpl;
import org.chromium.chrome.browser.firstrun.FirstRunActivityBase;
import org.chromium.chrome.browser.init.ActivityProfileProvider;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.privacy.settings.PrivacyPreferencesManagerImpl;
import org.chromium.chrome.browser.profiles.OtrProfileId;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.signin.services.SigninMetricsUtils;
import org.chromium.chrome.browser.signin.services.SigninMetricsUtils.State;
import org.chromium.chrome.browser.ui.signin.BottomSheetSigninAndHistorySyncCoordinator;
import org.chromium.chrome.browser.ui.signin.BottomSheetSigninAndHistorySyncCoordinator.NoAccountSigninMode;
import org.chromium.chrome.browser.ui.signin.BottomSheetSigninAndHistorySyncCoordinator.WithAccountSigninMode;
import org.chromium.chrome.browser.ui.signin.DialogWhenLargeContentLayout;
import org.chromium.chrome.browser.ui.signin.FullscreenSigninAndHistorySyncConfig;
import org.chromium.chrome.browser.ui.signin.FullscreenSigninAndHistorySyncCoordinator;
import org.chromium.chrome.browser.ui.signin.SigninAndHistorySyncCoordinator;
import org.chromium.chrome.browser.ui.signin.SigninUtils;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerBottomSheetStrings;
import org.chromium.chrome.browser.ui.signin.history_sync.HistorySyncConfig;
import org.chromium.chrome.browser.ui.system.StatusBarColorController;
import org.chromium.components.browser_ui.modaldialog.AppModalPresenter;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.SigninFeatureMap;
import org.chromium.components.signin.SigninFeatures;
import org.chromium.components.signin.base.CoreAccountId;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.ui.UiUtils;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;

/**
 * The activity that host post-UNO sign-in flows. This activity is semi-transparent, and views for
 * different sign-in flow steps will be hosted by it, according to the account's state and the flow
 * type.
 *
 * <p>For most cases, the flow is contains a sign-in bottom sheet, and a history sync opt-in dialog
 * shown after sign-in completion.
 *
 * <p>The activity may also hold the re-FRE which consists of a fullscreen sign-in dialog followed
 * by the history sync opt-in. This is why the dependency on {@link FirstRunActivityBase} is needed.
 */
public class SigninAndHistorySyncActivity extends FirstRunActivityBase
        implements BottomSheetSigninAndHistorySyncCoordinator.Delegate,
                FullscreenSigninAndHistorySyncCoordinator.Delegate {
    private static final String ARGUMENT_BOTTOM_SHEET_STRINGS_TITLE =
            "SigninAndHistorySyncActivity.BottomSheetStringsTitle";
    private static final String ARGUMENT_BOTTOM_SHEET_STRINGS_SUBTITLE =
            "SigninAndHistorySyncActivity.BottomSheetStringsSubtitle";
    private static final String ARGUMENT_BOTTOM_SHEET_STRINGS_DISMISS =
            "SigninAndHistorySyncActivity.BottomSheetStringsDismiss";
    // TODO(crbug.com/375392859): Remove this with PUT_PARCELABLE_SIGNIN_CONFIG_IN_EXTRA if no crash
    // related to the use of FullscreenSigninAndHistorySyncConfig as Parcelable extra is observed.
    private static final String ARGUMENT_FULLSCREEN_SIGNIN_CONFIG_TITLE =
            "SigninAndHistorySyncActivity.FullscreenSigninConfigTitle";
    private static final String ARGUMENT_FULLSCREEN_SIGNIN_CONFIG_SUBTITLE =
            "SigninAndHistorySyncActivity.FullscreenSigninConfigSubtitle";
    private static final String ARGUMENT_FULLSCREEN_SIGNIN_CONFIG_DISMISS_TEXT =
            "SigninAndHistorySyncActivity.FullscreenSigninConfigDismissText";
    private static final String ARGUMENT_FULLSCREEN_SIGNIN_CONFIG_LOGO =
            "SigninAndHistorySyncActivity.FullscreenSigninConfigLogo";
    private static final String ARGUMENT_HISTORY_SYNC_CONFIG_TITLE =
            "SigninAndHistorySyncActivity.HistorySyncConfigTitle";
    private static final String ARGUMENT_HISTORY_SYNC_CONFIG_SUBTITLE =
            "SigninAndHistorySyncActivity.HistorySyncConfigSubtitle";
    private static final String ARGUMENT_HISTORY_SYNC_CONFIG_HISTORY_OPT_IN_MODE =
            "SigninAndHistorySyncActivity.HistorySyncConfigHistoryOptInMode";

    private static final String ARGUMENT_ACCESS_POINT = "SigninAndHistorySyncActivity.AccessPoint";
    private static final String ARGUMENT_NO_ACCOUNT_SIGNIN_MODE =
            "SigninAndHistorySyncActivity.NoAccountSigninMode";
    private static final String ARGUMENT_WITH_ACCOUNT_SIGNIN_MODE =
            "SigninAndHistorySyncActivity.WithAccountSigninMode";
    private static final String ARGUMENT_HISTORY_OPT_IN_MODE =
            "SigninAndHistorySyncActivity.HistoryOptInMode";
    private static final String ARGUMENT_IS_FULLSCREEN_SIGNIN =
            "SigninAndHistorySyncActivity.IsFullscreenSignin";
    private static final String ARGUMENT_SELECTED_CORE_ACCOUNT_ID =
            "SigninAndHistorySyncActivity.SelectedCoreAccountId";
    private static final String ARGUMENT_FULLSCREEN_SIGNIN_CONFIG =
            "SigninAndHistoryOptInActivity.FullscreenSigninAndHistorySyncConfig";

    private static final int ADD_ACCOUNT_REQUEST_CODE = 1;

    private final OneshotSupplierImpl<Profile> mProfileSupplier = new OneshotSupplierImpl<>();
    // TODO(crbug.com/349787455): Move this to FirstRunActivityBase.
    private final Promise<Void> mNativeInitializationPromise = new Promise<>();
    private SigninAndHistorySyncCoordinator mCoordinator;

    // Set to true when the add account activity is started, and is not persisted in saved instance
    // state. Therefore when onActivityResultWithNavitve is called with the add account activity's
    // result, if this boolean is false, it means that the activity is killed when the add account
    // activity was in the foreground.
    private boolean mIsWaitingForAddAccountResult;

    @Override
    protected void onPreCreate() {
        super.onPreCreate();
        // Temporarily ensure that the native is initialized before calling super.onCreate().
        // TODO(crbug.com/41493758): Handle the case where the UI is shown before the end of
        // native initialization.
        ChromeBrowserInitializer.getInstance().handleSynchronousStartup();
    }

    @Override
    public void triggerLayoutInflation() {
        super.triggerLayoutInflation();

        Intent intent = getIntent();
        int signinAccessPoint = intent.getIntExtra(ARGUMENT_ACCESS_POINT, SigninAccessPoint.MAX);
        assert signinAccessPoint != SigninAccessPoint.MAX : "Cannot find SigninAccessPoint!";

        if (intent.getBooleanExtra(ARGUMENT_IS_FULLSCREEN_SIGNIN, false)) {
            updateSystemUiForFullscreenSignin();
            FullscreenSigninAndHistorySyncConfig config;
            if (SigninFeatureMap.isEnabled(SigninFeatures.PUT_PARCELABLE_SIGNIN_CONFIG_IN_EXTRA)) {
                config = intent.getParcelableExtra(ARGUMENT_FULLSCREEN_SIGNIN_CONFIG);
            } else {
                int signinTitleId =
                        intent.getIntExtra(
                                ARGUMENT_FULLSCREEN_SIGNIN_CONFIG_TITLE, R.string.signin_fre_title);
                int signinSubtitleId =
                        intent.getIntExtra(
                                ARGUMENT_FULLSCREEN_SIGNIN_CONFIG_SUBTITLE,
                                R.string.signin_fre_subtitle);
                int signinDismissText =
                        intent.getIntExtra(
                                ARGUMENT_FULLSCREEN_SIGNIN_CONFIG_DISMISS_TEXT,
                                R.string.signin_fre_dismiss_button);
                int signinLogoId =
                        intent.getIntExtra(
                                ARGUMENT_FULLSCREEN_SIGNIN_CONFIG_LOGO,
                                R.drawable.fre_product_logo);
                int historySyncTitleId =
                        intent.getIntExtra(
                                ARGUMENT_HISTORY_SYNC_CONFIG_TITLE, R.string.history_sync_title);
                int historySyncSubtitleId =
                        intent.getIntExtra(
                                ARGUMENT_HISTORY_SYNC_CONFIG_SUBTITLE,
                                R.string.history_sync_subtitle);
                @HistorySyncConfig.OptInMode
                int historyOptInMode =
                        intent.getIntExtra(
                                ARGUMENT_HISTORY_SYNC_CONFIG_HISTORY_OPT_IN_MODE,
                                HistorySyncConfig.OptInMode.OPTIONAL);
                config =
                        new FullscreenSigninAndHistorySyncConfig.Builder()
                                .signinTitleId(signinTitleId)
                                .signinSubtitleId(signinSubtitleId)
                                .signinDismissTextId(signinDismissText)
                                .signinLogoId(signinLogoId)
                                .historySyncTitleId(historySyncTitleId)
                                .historySyncSubtitleId(historySyncSubtitleId)
                                .historyOptInMode(historyOptInMode)
                                .build();
            }
            mCoordinator =
                    new FullscreenSigninAndHistorySyncCoordinator(
                            this,
                            getModalDialogManager(),
                            getProfileProviderSupplier(),
                            PrivacyPreferencesManagerImpl.getInstance(),
                            config,
                            signinAccessPoint,
                            this);

            setInitialContentView(mCoordinator.getView());
            onInitialLayoutInflationComplete();
            return;
        }

        setStatusBarColor(Color.TRANSPARENT);
        int titleStringId = intent.getIntExtra(ARGUMENT_BOTTOM_SHEET_STRINGS_TITLE, 0);
        int subtitleStringId = intent.getIntExtra(ARGUMENT_BOTTOM_SHEET_STRINGS_SUBTITLE, 0);
        int dismissStringId = intent.getIntExtra(ARGUMENT_BOTTOM_SHEET_STRINGS_DISMISS, 0);
        AccountPickerBottomSheetStrings bottomSheetStrings =
                new AccountPickerBottomSheetStrings.Builder(titleStringId)
                        .setSubtitleStringId(subtitleStringId)
                        .setDismissButtonStringId(dismissStringId)
                        .build();

        @NoAccountSigninMode
        int noAccountSigninMode =
                intent.getIntExtra(
                        ARGUMENT_NO_ACCOUNT_SIGNIN_MODE, NoAccountSigninMode.BOTTOM_SHEET);
        @WithAccountSigninMode
        int withAccountSigninMode =
                intent.getIntExtra(
                        ARGUMENT_WITH_ACCOUNT_SIGNIN_MODE,
                        WithAccountSigninMode.DEFAULT_ACCOUNT_BOTTOM_SHEET);
        @HistorySyncConfig.OptInMode
        int historyOptInMode =
                intent.getIntExtra(
                        ARGUMENT_HISTORY_OPT_IN_MODE, HistorySyncConfig.OptInMode.OPTIONAL);
        @Nullable String accountId = intent.getStringExtra(ARGUMENT_SELECTED_CORE_ACCOUNT_ID);

        mCoordinator =
                new BottomSheetSigninAndHistorySyncCoordinator(
                        getWindowAndroid(),
                        this,
                        this,
                        DeviceLockActivityLauncherImpl.get(),
                        mProfileSupplier,
                        getModalDialogManagerSupplier(),
                        bottomSheetStrings,
                        noAccountSigninMode,
                        withAccountSigninMode,
                        historyOptInMode,
                        signinAccessPoint,
                        accountId == null ? null : new CoreAccountId(accountId));

        setInitialContentView(mCoordinator.getView());
        onInitialLayoutInflationComplete();
    }

    @Override
    protected ModalDialogManager createModalDialogManager() {
        return new ModalDialogManager(new AppModalPresenter(this), ModalDialogType.APP);
    }

    @Override
    protected OneshotSupplier<ProfileProvider> createProfileProvider() {
        ActivityProfileProvider profileProvider =
                new ActivityProfileProvider(getLifecycleDispatcher()) {
                    @Nullable
                    @Override
                    protected OtrProfileId createOffTheRecordProfileId() {
                        throw new IllegalStateException(
                                "Attempting to access incognito in the sign-in & history sync"
                                        + " opt-in flow");
                    }
                };

        profileProvider.onAvailable(
                (provider) -> {
                    mProfileSupplier.set(profileProvider.get().getOriginalProfile());
                });
        return profileProvider;
    }

    @Override
    protected ActivityWindowAndroid createWindowAndroid() {
        return new ActivityWindowAndroid(
                this,
                /* listenToActivityState= */ true,
                getIntentRequestTracker(),
                getInsetObserver());
    }

    @Override
    public boolean shouldStartGpuProcess() {
        return false;
    }

    @Override
    public void finishNativeInitialization() {
        super.finishNativeInitialization();
        mNativeInitializationPromise.fulfill(null);
    }

    /**
     * Implements {@link BottomSheetSigninAndHistorySyncCoordinator.Delegate} and {@link
     * FullscreenSigninAndHistorySyncCoordinator.Delegate}.
     */
    @Override
    public void onFlowComplete(@SigninAndHistorySyncCoordinator.Result int result) {
        int resultCode =
                result == SigninAndHistorySyncCoordinator.Result.COMPLETED
                        ? Activity.RESULT_OK
                        : Activity.RESULT_CANCELED;
        setResult(resultCode);
        finish();
        // Override activity animation to avoid visual glitches due to the semi-transparent
        // background.
        overridePendingTransition(0, R.anim.fast_fade_out);
    }

    /** Implements {@link BottomSheetSigninAndHistorySyncCoordinator.Delegate}. */
    @Override
    public boolean isHistorySyncShownFullScreen() {
        return !isTablet();
    }

    /** Implements {@link BottomSheetSigninAndHistorySyncCoordinator.Delegate}. */
    @Override
    public void setStatusBarColor(int statusBarColor) {
        StatusBarColorController.setStatusBarColor(getWindow(), statusBarColor);
    }

    @Override
    public void performOnConfigurationChanged(Configuration newConfig) {
        super.performOnConfigurationChanged(newConfig);
        mCoordinator.onConfigurationChange();
    }

    @Override
    public boolean onActivityResultWithNative(int requestCode, int resultCode, Intent data) {
        if (super.onActivityResultWithNative(requestCode, resultCode, data)) {
            return true;
        }

        if (requestCode != ADD_ACCOUNT_REQUEST_CODE) {
            return false;
        }

        // If mIsWaitingForAddAccountResult is false, it means that the add account activity was not
        // started in this instance of Activity, and that the sign-in activity was killed during the
        // add account flow.
        if (!mIsWaitingForAddAccountResult) {
            SigninMetricsUtils.logAddAccountStateHistogram(State.ACTIVITY_DESTROYED);
        } else {
            SigninMetricsUtils.logAddAccountStateHistogram(State.ACTIVITY_SURVIVED);
        }

        mIsWaitingForAddAccountResult = false;
        onAddAccountResult(resultCode, data);
        return true;
    }

    @Override
    protected void onDestroy() {
        mCoordinator.destroy();
        super.onDestroy();
    }

    /** Implements {@link FirstRunActivityBase} */
    @Override
    public @BackPressResult int handleBackPress() {
        return mCoordinator.handleBackPress();
    }

    @Override
    public @SecondaryActivityBackPressUma.SecondaryActivity int getSecondaryActivity() {
        return SecondaryActivityBackPressUma.SecondaryActivity.SIGNIN_AND_HISTORY_SYNC;
    }

    public static @NonNull Intent createIntent(
            @NonNull Context context,
            @NonNull AccountPickerBottomSheetStrings bottomSheetStrings,
            @NoAccountSigninMode int noAccountSigninMode,
            @WithAccountSigninMode int withAccountSigninMode,
            @HistorySyncConfig.OptInMode int historyOptInMode,
            @SigninAccessPoint int signinAccessPoint,
            @Nullable CoreAccountId selectedCoreAccountId) {
        assert bottomSheetStrings != null;

        Intent intent = new Intent(context, SigninAndHistorySyncActivity.class);
        intent.putExtra(ARGUMENT_BOTTOM_SHEET_STRINGS_TITLE, bottomSheetStrings.titleStringId);
        intent.putExtra(
                ARGUMENT_BOTTOM_SHEET_STRINGS_SUBTITLE, bottomSheetStrings.subtitleStringId);
        intent.putExtra(
                ARGUMENT_BOTTOM_SHEET_STRINGS_DISMISS, bottomSheetStrings.dismissButtonStringId);
        intent.putExtra(ARGUMENT_NO_ACCOUNT_SIGNIN_MODE, noAccountSigninMode);
        intent.putExtra(ARGUMENT_WITH_ACCOUNT_SIGNIN_MODE, withAccountSigninMode);
        intent.putExtra(ARGUMENT_HISTORY_OPT_IN_MODE, historyOptInMode);
        intent.putExtra(ARGUMENT_ACCESS_POINT, signinAccessPoint);
        if (selectedCoreAccountId != null) {
            intent.putExtra(ARGUMENT_SELECTED_CORE_ACCOUNT_ID, selectedCoreAccountId.getId());
        }
        return intent;
    }

    public static @NonNull Intent createIntentForFullscreenSignin(
            Context context,
            FullscreenSigninAndHistorySyncConfig config,
            @SigninAccessPoint int signinAccessPoint) {
        Intent intent = new Intent(context, SigninAndHistorySyncActivity.class);
        intent.putExtra(ARGUMENT_IS_FULLSCREEN_SIGNIN, true);
        if (SigninFeatureMap.isEnabled(SigninFeatures.PUT_PARCELABLE_SIGNIN_CONFIG_IN_EXTRA)) {
            intent.putExtra(ARGUMENT_FULLSCREEN_SIGNIN_CONFIG, config);
        } else {
            intent.putExtra(ARGUMENT_FULLSCREEN_SIGNIN_CONFIG_TITLE, config.signinConfig.titleId);
            intent.putExtra(
                    ARGUMENT_FULLSCREEN_SIGNIN_CONFIG_SUBTITLE, config.signinConfig.subtitleId);
            intent.putExtra(
                    ARGUMENT_FULLSCREEN_SIGNIN_CONFIG_DISMISS_TEXT,
                    config.signinConfig.dismissTextId);
            intent.putExtra(ARGUMENT_FULLSCREEN_SIGNIN_CONFIG_LOGO, config.signinConfig.logoId);
            intent.putExtra(ARGUMENT_HISTORY_SYNC_CONFIG_TITLE, config.historySyncConfig.titleId);
            intent.putExtra(
                    ARGUMENT_HISTORY_SYNC_CONFIG_SUBTITLE, config.historySyncConfig.subtitleId);
            intent.putExtra(
                    ARGUMENT_HISTORY_SYNC_CONFIG_HISTORY_OPT_IN_MODE, config.historyOptInMode);
        }
        intent.putExtra(ARGUMENT_ACCESS_POINT, signinAccessPoint);
        return intent;
    }

    /**
     * Implements {@link FullscreenSigninAndHistorySyncCoordinator.Delegate} and {@link
     * BottomSheetSigninAndHistorySyncCoordinator.Delegate}
     */
    @Override
    public void addAccount() {
        SigninMetricsUtils.logAddAccountStateHistogram(State.REQUESTED);
        AccountManagerFacadeProvider.getInstance()
                .createAddAccountIntent(
                        intent -> {
                            final ActivityWindowAndroid windowAndroid = getWindowAndroid();
                            if (windowAndroid == null) {
                                // The activity was shut down. Do nothing.
                                SigninMetricsUtils.logAddAccountStateHistogram(State.FAILED);
                                return;
                            }
                            if (intent == null) {
                                // AccountManagerFacade couldn't create the intent, use SigninUtils
                                // to open settings instead.
                                SigninMetricsUtils.logAddAccountStateHistogram(State.FAILED);
                                SigninUtils.openSettingsForAllAccounts(this);
                                return;
                            }
                            SigninMetricsUtils.logAddAccountStateHistogram(State.STARTED);
                            mIsWaitingForAddAccountResult = true;
                            startActivityForResult(intent, ADD_ACCOUNT_REQUEST_CODE);
                        });
    }

    /** Implements {@link FullscreenSigninAndHistorySyncCoordinator.Delegate} */
    @Override
    public Promise<Void> getNativeInitializationPromise() {
        return mNativeInitializationPromise;
    }

    private void setInitialContentView(View view) {
        assert view.getParent() == null;

        Intent intent = getIntent();
        if (intent.getBooleanExtra(ARGUMENT_IS_FULLSCREEN_SIGNIN, false)) {
            // Identically to the FRE, wrap the fullscreen sign-in flow UI inside a custom layout
            // which mimic DialogWhenLarge theme behavior.
            super.setContentView(SigninUtils.wrapInDialogWhenLargeLayout(view));
            return;
        }

        super.setContentView(view);
    }

    private void updateSystemUiForFullscreenSignin() {
        if (DialogWhenLargeContentLayout.shouldShowAsDialog(this)) {
            // Set status bar and navigation bar to dark if the promo is shown as a dialog.
            setStatusBarColor(Color.BLACK);
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
                // Use dark navigation bar.
                Window window = getWindow();
                window.setNavigationBarColor(Color.BLACK);
                window.setNavigationBarDividerColor(Color.BLACK);
                UiUtils.setNavigationBarIconColor(window.getDecorView().getRootView(), false);
            }
        } else {
            // Set the status bar color to the fullsceen sign-in background color.
            setStatusBarColor(SemanticColorUtils.getDefaultBgColor(this));
        }
    }

    private void onAddAccountResult(int resultCode, Intent data) {
        final String accountEmail =
                data == null
                        ? null
                        : IntentUtils.safeGetStringExtra(data, AccountManager.KEY_ACCOUNT_NAME);

        if (resultCode != Activity.RESULT_OK || accountEmail == null) {
            mCoordinator.onAddAccountCanceled();

            // Record NULL_ACCOUNT_NAME if the add account activity successfully returns but
            // contains a null account name.
            if (resultCode == Activity.RESULT_OK && accountEmail == null) {
                SigninMetricsUtils.logAddAccountStateHistogram(State.NULL_ACCOUNT_NAME);
            } else {
                SigninMetricsUtils.logAddAccountStateHistogram(State.CANCELLED);
            }
            return;
        }

        SigninMetricsUtils.logAddAccountStateHistogram(State.SUCCEEDED);
        mCoordinator.onAccountAdded(accountEmail);
    }
}
