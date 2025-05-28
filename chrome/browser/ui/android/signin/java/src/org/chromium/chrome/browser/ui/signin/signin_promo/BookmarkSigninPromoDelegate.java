// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.signin_promo;

import android.content.Context;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.DisplayableProfileData;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.signin.services.SigninPreferencesManager;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.browser.ui.signin.R;
import org.chromium.chrome.browser.ui.signin.SigninAndHistorySyncActivityLauncher;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.sync.SyncService;
import org.chromium.components.sync.UserSelectableType;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Set;

/** {@link SigninPromoDelegate} for bookmark signin promo. */
public class BookmarkSigninPromoDelegate extends SigninPromoDelegate {

    /** Indicates the type of content the should be shown in the visible promo. */
    @IntDef({PromoState.NONE, PromoState.SIGNIN, PromoState.ACCOUNT_SETTINGS})
    @Retention(RetentionPolicy.SOURCE)
    private @interface PromoState {
        /** No promo should be shown. */
        int NONE = 0;

        /** The promo content should promote sign-in. Shown to signed-out user. */
        int SIGNIN = 1;

        /**
         * The promo content should promote enabling bookmark sync in the settings. Shown to
         * signed-in user with bookmarks or reading list sync disabled in settings.
         */
        int ACCOUNT_SETTINGS = 2;
    }

    @VisibleForTesting static final int MAX_IMPRESSIONS_BOOKMARKS = 20;

    private final String mPromoShowCountPreferenceName;
    private final Runnable mOnOpenSettingsClicked;
    private @PromoState int mPromoState = PromoState.NONE;

    public BookmarkSigninPromoDelegate(
            Context context,
            Profile profile,
            SigninAndHistorySyncActivityLauncher launcher,
            Runnable onPromoStateChange,
            Runnable onOpenSettingsClicked) {
        super(context, profile, launcher, onPromoStateChange);

        mPromoShowCountPreferenceName =
                ChromePreferenceKeys.SYNC_PROMO_SHOW_COUNT.createKey(
                        SigninPreferencesManager.SigninPromoAccessPointId.BOOKMARKS);
        mOnOpenSettingsClicked = onOpenSettingsClicked;
    }

    @Override
    String getTitle() {
        switch (mPromoState) {
            case PromoState.SIGNIN:
                return mContext.getString(R.string.signin_promo_title_bookmarks);
            case PromoState.ACCOUNT_SETTINGS:
                return mContext.getString(R.string.sync_promo_title_bookmarks);
            case PromoState.NONE:
            default:
                throw new IllegalStateException("Forbidden promo type: " + mPromoState);
        }
    }

    @Override
    String getDescription() {
        switch (mPromoState) {
            case PromoState.SIGNIN:
                return mContext.getString(R.string.signin_promo_description_bookmarks);
            case PromoState.ACCOUNT_SETTINGS:
                return mContext.getString(R.string.account_settings_promo_description_bookmarks);
            case PromoState.NONE:
            default:
                throw new IllegalStateException("Forbidden promo type: " + mPromoState);
        }
    }

    @Override
    @SigninPreferencesManager.SigninPromoAccessPointId
    String getAccessPointName() {
        return SigninPreferencesManager.SigninPromoAccessPointId.BOOKMARKS;
    }

    @Override
    @SigninAccessPoint
    int getAccessPoint() {
        return SigninAccessPoint.BOOKMARK_MANAGER;
    }

    @Override
    void onDismissButtonClicked() {
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.SIGNIN_PROMO_BOOKMARKS_DECLINED, true);
    }

    @Override
    boolean canShowPromo() {
        return mPromoState != PromoState.NONE;
    }

    @Override
    boolean refreshPromoState(@Nullable CoreAccountInfo visibleAccount) {
        @PromoState int newState = computePromoState();
        boolean wasStateChanged = mPromoState != newState;
        mPromoState = newState;
        return wasStateChanged;
    }

    @Override
    boolean shouldHideSecondaryButton() {
        switch (mPromoState) {
            case PromoState.SIGNIN:
                return false;
            case PromoState.ACCOUNT_SETTINGS:
                return true;
            case PromoState.NONE:
            default:
                throw new IllegalStateException("Forbidden promo type: " + mPromoState);
        }
    }

    @Override
    String getTextForPrimaryButton(@Nullable DisplayableProfileData profileData) {
        switch (mPromoState) {
            case PromoState.SIGNIN:
                return super.getTextForPrimaryButton(profileData);
            case PromoState.ACCOUNT_SETTINGS:
                return mContext.getString(R.string.open_settings_button);
            case PromoState.NONE:
            default:
                throw new IllegalStateException("Forbidden promo type: " + mPromoState);
        }
    }

    @Override
    void recordImpression() {
        ChromeSharedPreferences.getInstance().incrementInt(mPromoShowCountPreferenceName);
    }

    @Override
    boolean isMaxImpressionsReached() {
        return ChromeSharedPreferences.getInstance().readInt(mPromoShowCountPreferenceName)
                >= MAX_IMPRESSIONS_BOOKMARKS;
    }

    @Override
    void onPrimaryButtonClicked() {
        switch (mPromoState) {
            case PromoState.SIGNIN:
                super.onPrimaryButtonClicked();
                break;
            case PromoState.ACCOUNT_SETTINGS:
                mOnOpenSettingsClicked.run();
                break;
            case PromoState.NONE:
            default:
                throw new IllegalStateException("Forbidden promo type: " + mPromoState);
        }
    }

    private @PromoState int computePromoState() {
        if (wasPromoDeclined() || !canManuallyEnableSyncTypes()) {
            return PromoState.NONE;
        }

        IdentityManager identityManager =
                IdentityServicesProvider.get().getIdentityManager(mProfile);
        if (identityManager.hasPrimaryAccount(ConsentLevel.SIGNIN)) {
            return PromoState.ACCOUNT_SETTINGS;
        }

        SigninManager signinManager = IdentityServicesProvider.get().getSigninManager(mProfile);
        return signinManager.isSigninAllowed() ? PromoState.SIGNIN : PromoState.NONE;
    }

    private boolean wasPromoDeclined() {
        return ChromeSharedPreferences.getInstance()
                .readBoolean(ChromePreferenceKeys.SIGNIN_PROMO_BOOKMARKS_DECLINED, false);
    }

    private boolean canManuallyEnableSyncTypes() {
        SyncService syncService = SyncServiceFactory.getForProfile(mProfile);
        boolean areTypesAlreadyEnabled =
                syncService
                        .getSelectedTypes()
                        .containsAll(
                                Set.of(
                                        UserSelectableType.BOOKMARKS,
                                        UserSelectableType.READING_LIST));
        boolean areBookmarksManaged =
                syncService.isTypeManagedByPolicy(UserSelectableType.BOOKMARKS);

        return !areTypesAlreadyEnabled && !areBookmarksManaged;
    }
}
