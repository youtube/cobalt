// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.identity_disc;

import android.content.Context;
import android.content.Intent;
import android.content.res.Resources;
import android.graphics.drawable.Drawable;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.base.Callback;
import org.chromium.base.ObserverList;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.NativeInitObserver;
import org.chromium.chrome.browser.ntp.NewTabPage;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.MainSettings;
import org.chromium.chrome.browser.settings.SettingsNavigationFactory;
import org.chromium.chrome.browser.signin.SigninAndHistorySyncActivityLauncherImpl;
import org.chromium.chrome.browser.signin.services.DisplayableProfileData;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.ProfileDataCache;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.browser.sync.settings.SyncSettingsUtils;
import org.chromium.chrome.browser.sync.settings.SyncSettingsUtils.SyncError;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonVariant;
import org.chromium.chrome.browser.toolbar.optional_button.ButtonData;
import org.chromium.chrome.browser.toolbar.optional_button.ButtonData.ButtonSpec;
import org.chromium.chrome.browser.toolbar.optional_button.ButtonDataImpl;
import org.chromium.chrome.browser.toolbar.optional_button.ButtonDataProvider;
import org.chromium.chrome.browser.ui.signin.BottomSheetSigninAndHistorySyncConfig;
import org.chromium.chrome.browser.ui.signin.BottomSheetSigninAndHistorySyncConfig.NoAccountSigninMode;
import org.chromium.chrome.browser.ui.signin.BottomSheetSigninAndHistorySyncConfig.WithAccountSigninMode;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerBottomSheetStrings;
import org.chromium.chrome.browser.ui.signin.history_sync.HistorySyncConfig;
import org.chromium.chrome.browser.user_education.IphCommandBuilder;
import org.chromium.chrome.browser.util.BrowserUiUtils;
import org.chromium.components.browser_ui.settings.SettingsNavigation;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.identitymanager.PrimaryAccountChangeEvent;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.sync.SyncService;
import org.chromium.components.user_prefs.UserPrefs;

/**
 * Handles displaying IdentityDisc on toolbar depending on several conditions (user sign-in state,
 * whether NTP is shown)
 */
public class IdentityDiscController
        implements NativeInitObserver,
                ProfileDataCache.Observer,
                IdentityManager.Observer,
                SyncService.SyncStateChangedListener,
                ButtonDataProvider {
    // Context is used for fetching resources and launching preferences page.
    private final Context mContext;
    private ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    private final ObservableSupplier<Profile> mProfileSupplier;
    private final Callback<Profile> mProfileSupplierObserver = this::setProfile;
    private Profile mProfile;

    // We observe IdentityManager to receive primary account state change notifications.
    private IdentityManager mIdentityManager;

    // SyncService is observed to update mIdentityError.
    private SyncService mSyncService;

    // ProfileDataCache facilitates retrieving profile picture.
    private ProfileDataCache mProfileDataCache;

    private final ButtonDataImpl mButtonData;
    private final ObserverList<ButtonDataObserver> mObservers = new ObserverList<>();
    private boolean mNativeIsInitialized;

    private boolean mIsTabNtp;

    private @SyncError int mIdentityError = SyncError.NO_ERROR;

    /**
     * @param context The Context for retrieving resources, launching preference activity, etc.
     * @param activityLifecycleDispatcher Dispatcher for activity lifecycle events, e.g. native
     *     initialization completing.
     */
    public IdentityDiscController(
            Context context,
            ActivityLifecycleDispatcher activityLifecycleDispatcher,
            ObservableSupplier<Profile> profileSupplier) {
        mContext = context;
        mActivityLifecycleDispatcher = activityLifecycleDispatcher;
        mProfileSupplier = profileSupplier;
        mActivityLifecycleDispatcher.register(this);

        mButtonData =
                new ButtonDataImpl(
                        /* canShow= */ false,
                        /* drawable= */ null,
                        /* onClickListener= */ view -> onClick(),
                        mContext.getString(R.string.accessibility_toolbar_btn_identity_disc),
                        /* supportsTinting= */ false,
                        new IphCommandBuilder(
                                mContext.getResources(),
                                FeatureConstants.IDENTITY_DISC_FEATURE,
                                R.string.iph_identity_disc_text,
                                R.string.iph_identity_disc_accessibility_text),
                        /* isEnabled= */ true,
                        AdaptiveToolbarButtonVariant.UNKNOWN,
                        /* tooltipTextResId= */ Resources.ID_NULL);
    }

    /** Registers itself to observe sign-in and sync status events. */
    @Override
    public void onFinishNativeInitialization() {
        mActivityLifecycleDispatcher.unregister(this);
        mActivityLifecycleDispatcher = null;
        mNativeIsInitialized = true;

        mProfileSupplier.addObserver(mProfileSupplierObserver);
    }

    @Override
    public void addObserver(ButtonDataObserver obs) {
        mObservers.addObserver(obs);
    }

    @Override
    public void removeObserver(ButtonDataObserver obs) {
        mObservers.removeObserver(obs);
    }

    @Override
    public ButtonData get(Tab tab) {
        mIsTabNtp = tab != null && tab.getNativePage() instanceof NewTabPage;
        if (!mIsTabNtp) {
            mButtonData.setCanShow(false);
            return mButtonData;
        }

        calculateButtonData();
        return mButtonData;
    }

    private void calculateButtonData() {
        if (!mNativeIsInitialized) {
            assert !mButtonData.canShow();
            return;
        }

        String email = CoreAccountInfo.getEmailFrom(getSignedInAccountInfo());
        ensureProfileDataCache();

        mButtonData.setButtonSpec(
                buttonSpecWithDrawableAndDescription(mButtonData.getButtonSpec(), email));
        mButtonData.setCanShow(true);
    }

    private ButtonSpec buttonSpecWithDrawableAndDescription(
            ButtonSpec buttonSpec, @Nullable String email) {
        Drawable drawable = getProfileImage(email);
        if (buttonSpec.getDrawable() == drawable) {
            return buttonSpec;
        }

        // `supportsTinting` must be false when showing the user's profile image or its placeholder,
        // to not alter the images colors in those cases.
        boolean shouldSupportTinting = email == null;
        String contentDescription = getContentDescription(email);
        return new ButtonSpec(
                drawable,
                buttonSpec.getOnClickListener(),
                /* onLongClickListener= */ null,
                contentDescription,
                shouldSupportTinting,
                buttonSpec.getIphCommandBuilder(),
                AdaptiveToolbarButtonVariant.UNKNOWN,
                buttonSpec.getActionChipLabelResId(),
                buttonSpec.getHoverTooltipTextId(),
                /* hasErrorBadge= */ mIdentityError != SyncError.NO_ERROR);
    }

    /**
     * Creates and initializes ProfileDataCache if it wasn't created previously. Subscribes
     * IdentityDiscController for profile data updates.
     */
    private void ensureProfileDataCache() {
        if (mProfileDataCache != null) return;

        mProfileDataCache =
                ProfileDataCache.createWithoutBadge(mContext, R.dimen.toolbar_identity_disc_size);
        mProfileDataCache.addObserver(this);
    }

    /**
     * Returns Profile picture Drawable. The size of the image corresponds to current visual state.
     */
    private Drawable getProfileImage(@Nullable String email) {
        return email == null
                ? AppCompatResources.getDrawable(mContext, R.drawable.account_circle)
                : mProfileDataCache.getProfileDataOrDefault(email).getImage();
    }

    /** Resets ProfileDataCache. Used for flushing cached image when sign-in state changes. */
    private void resetIdentityDiscCache() {
        if (mProfileDataCache != null) {
            mProfileDataCache.removeObserver(this);
            mProfileDataCache = null;
        }
    }

    private void notifyObservers(boolean hint) {
        for (ButtonDataObserver observer : mObservers) {
            observer.buttonDataChanged(hint);
        }
    }

    /** Called after profile image becomes available. Updates the image on toolbar button. */
    @Override
    public void onProfileDataUpdated(String accountEmail) {
        assert mProfileDataCache != null;

        if (accountEmail.equals(CoreAccountInfo.getEmailFrom(getSignedInAccountInfo()))) {
            /*
             * We need to call {@link notifyObservers(false)} before calling
             * {@link notifyObservers(true)}. This is because {@link notifyObservers(true)} has been
             * called in {@link setProfile()}, and without calling {@link notifyObservers(false)},
             * the ObservableSupplierImpl doesn't propagate the call. See https://cubug.com/1137535.
             */
            notifyObservers(false);
            notifyObservers(true);
        }
    }

    /**
     * Implements {@link IdentityManager.Observer}.
     *
     * <p>IdentityDisc should be always shown regardless of whether the user is signed out, signed
     * in or syncing.
     */
    @Override
    public void onPrimaryAccountChanged(PrimaryAccountChangeEvent eventDetails) {
        switch (eventDetails.getEventTypeFor(ConsentLevel.SIGNIN)) {
            case PrimaryAccountChangeEvent.Type.SET:
                notifyObservers(true);
                break;
            case PrimaryAccountChangeEvent.Type.CLEARED:
                resetIdentityDiscCache();
                notifyObservers(false);
                break;
            case PrimaryAccountChangeEvent.Type.NONE:
                break;
        }
    }

    /** Call to tear down dependencies. */
    @Override
    public void destroy() {
        if (mActivityLifecycleDispatcher != null) {
            mActivityLifecycleDispatcher.unregister(this);
            mActivityLifecycleDispatcher = null;
        }

        if (mProfileDataCache != null) {
            mProfileDataCache.removeObserver(this);
            mProfileDataCache = null;
        }

        if (mIdentityManager != null) {
            mIdentityManager.removeObserver(this);
            mIdentityManager = null;
        }

        if (mSyncService != null) {
            mSyncService.removeSyncStateChangedListener(this);
            mSyncService = null;
        }

        if (mNativeIsInitialized) {
            mProfileSupplier.removeObserver(mProfileSupplierObserver);
            mProfile = null;
        }
    }

    /** {@link SyncService.SyncStateChangedListener} implementation. */
    @Override
    public void syncStateChanged() {
        maybeUpdateIdentityErrorAndBadge();
    }

    @VisibleForTesting
    public @SyncError int getIdentityError() {
        return mIdentityError;
    }

    private void maybeUpdateIdentityErrorAndBadge() {
        if (mProfile == null) {
            return;
        }

        @SyncError int error = SyncSettingsUtils.getIdentityError(mProfile);
        if (error == mIdentityError
                || !ChromeFeatureList.isEnabled(ChromeFeatureList.UNO_PHASE_2_FOLLOW_UP)) {
            // Nothing changed.
            return;
        }
        mIdentityError = error;

        CoreAccountInfo coreAccountInfo = getSignedInAccountInfo();
        if (coreAccountInfo != null) {
            ensureProfileDataCache();
            mProfileDataCache.setBadge(
                    coreAccountInfo.getEmail(),
                    mIdentityError == SyncError.NO_ERROR
                            ? null
                            : ProfileDataCache.createToolbarIdentityDiscBadgeConfig(
                                    mContext, R.drawable.ic_error_badge_16dp));
        }
    }

    /**
     * Records IdentityDisc usage with feature engagement tracker. This signal can be used to decide
     * whether to show in-product help. We also record the clicking actions on the profile icon in
     * histograms.
     */
    private void recordIdentityDiscUsed() {
        BrowserUiUtils.recordIdentityDiscClicked(mIsTabNtp);

        assert isProfileInitialized();
        Tracker tracker = TrackerFactory.getTrackerForProfile(mProfileSupplier.get());
        tracker.notifyEvent(EventConstants.IDENTITY_DISC_USED);
        RecordUserAction.record("MobileToolbarIdentityDiscTap");
    }

    /**
     * Returns the account info of mIdentityManager if current profile is regular, and
     * null for off-the-record ones.
     * @return account info for the current profile. Returns null for OTR profile.
     */
    private CoreAccountInfo getSignedInAccountInfo() {
        return mIdentityManager != null
                ? mIdentityManager.getPrimaryAccountInfo(ConsentLevel.SIGNIN)
                : null;
    }

    /**
     * Triggered by mProfileSupplierObserver when profile is changed in mProfileSupplier.
     * mIdentityManager is updated with the profile, as set to null if profile is off-the-record.
     */
    private void setProfile(Profile profile) {
        mProfile = profile;

        if (mSyncService != null) {
            mSyncService.removeSyncStateChangedListener(this);
        }

        if (mIdentityManager != null) {
            mIdentityManager.removeObserver(this);
        }

        if (profile.isOffTheRecord()) {
            mIdentityManager = null;
            mSyncService = null;
        } else {
            mIdentityManager = IdentityServicesProvider.get().getIdentityManager(profile);
            mIdentityManager.addObserver(this);

            mSyncService = SyncServiceFactory.getForProfile(profile);
            if (mSyncService != null) {
                mSyncService.addSyncStateChangedListener(this);
                maybeUpdateIdentityErrorAndBadge();
            }

            notifyObservers(true);
        }
    }

    private String getContentDescription(@Nullable String email) {
        if (email == null) {
            return mContext.getString(R.string.accessibility_toolbar_btn_signed_out_identity_disc);
        }

        DisplayableProfileData profileData = mProfileDataCache.getProfileDataOrDefault(email);
        String userName = profileData.getFullName();
        if (profileData.hasDisplayableEmailAddress()) {
            return mContext.getString(
                    mIdentityError == SyncError.NO_ERROR
                            ? R.string.accessibility_toolbar_btn_identity_disc_with_name_and_email
                            : R.string
                                    .accessibility_toolbar_btn_identity_disc_error_with_name_and_email,
                    userName,
                    email);
        }

        return mContext.getString(
                mIdentityError == SyncError.NO_ERROR
                        ? R.string.accessibility_toolbar_btn_identity_disc_with_name
                        : R.string.accessibility_toolbar_btn_identity_disc_error_with_name,
                userName);
    }

    private boolean isProfileInitialized() {
        return mProfileSupplier != null && mProfileSupplier.hasValue();
    }

    @VisibleForTesting
    void onClick() {
        if (!isProfileInitialized()) {
            return;
        }
        recordIdentityDiscUsed();

        Profile originalProfile = mProfileSupplier.get().getOriginalProfile();
        if (getSignedInAccountInfo() == null
                && UserPrefs.get(originalProfile).getBoolean(Pref.SIGNIN_ALLOWED)) {
            AccountPickerBottomSheetStrings bottomSheetStrings =
                    new AccountPickerBottomSheetStrings.Builder(
                                    R.string.signin_account_picker_bottom_sheet_title)
                            .setSubtitleStringId(
                                    R.string.signin_account_picker_bottom_sheet_benefits_subtitle)
                            .build();
            BottomSheetSigninAndHistorySyncConfig config =
                    new BottomSheetSigninAndHistorySyncConfig.Builder(
                                    bottomSheetStrings,
                                    NoAccountSigninMode.BOTTOM_SHEET,
                                    WithAccountSigninMode.DEFAULT_ACCOUNT_BOTTOM_SHEET,
                                    HistorySyncConfig.OptInMode.OPTIONAL)
                            .build();
            @Nullable
            Intent intent =
                    SigninAndHistorySyncActivityLauncherImpl.get()
                            .createBottomSheetSigninIntentOrShowError(
                                    mContext,
                                    originalProfile,
                                    config,
                                    SigninAccessPoint.NTP_SIGNED_OUT_ICON);
            if (intent != null) {
                mContext.startActivity(intent);
            }
        } else {
            SettingsNavigation settingsNavigation =
                    SettingsNavigationFactory.createSettingsNavigation();
            settingsNavigation.startSettings(mContext, MainSettings.class);
        }
    }

    @VisibleForTesting
    boolean isProfileDataCacheEmpty() {
        return mProfileDataCache == null;
    }
}
