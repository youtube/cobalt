// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.settings;

import static org.chromium.chrome.browser.autofill.AutofillUiUtils.getCardIcon;
import static org.chromium.chrome.browser.autofill.AutofillUiUtils.getSettingsPageIconHeightId;
import static org.chromium.chrome.browser.autofill.AutofillUiUtils.getSettingsPageIconWidthId;

import android.content.Context;
import android.graphics.PorterDuff;
import android.graphics.drawable.Drawable;
import android.hardware.biometrics.BiometricManager;
import android.os.Build;
import android.os.Bundle;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;

import androidx.annotation.Nullable;
import androidx.core.hardware.fingerprint.FingerprintManagerCompat;
import androidx.preference.Preference;
import androidx.preference.PreferenceFragmentCompat;
import androidx.preference.PreferenceScreen;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.AutofillEditorBase;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;
import org.chromium.chrome.browser.device_reauth.DeviceAuthRequester;
import org.chromium.chrome.browser.device_reauth.ReauthenticatorBridge;
import org.chromium.chrome.browser.feedback.FragmentHelpAndFeedbackLauncher;
import org.chromium.chrome.browser.feedback.HelpAndFeedbackLauncher;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.payments.ServiceWorkerPaymentAppBridge;
import org.chromium.chrome.browser.settings.ChromeManagedPreferenceDelegate;
import org.chromium.components.autofill.VirtualCardEnrollmentState;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.payments.AndroidPaymentAppFactory;

/**
 * Autofill credit cards fragment, which allows the user to edit credit cards and control
 * payment apps.
 */
public class AutofillPaymentMethodsFragment
        extends PreferenceFragmentCompat implements PersonalDataManager.PersonalDataManagerObserver,
                                                    FragmentHelpAndFeedbackLauncher {
    static final String PREF_MANDATORY_REAUTH = "mandatory_reauth";
    private static final String PREF_PAYMENT_APPS = "payment_apps";

    private HelpAndFeedbackLauncher mHelpAndFeedbackLauncher;

    @Nullable
    private ReauthenticatorBridge mReauthenticatorBridge;

    @Override
    public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
        getActivity().setTitle(R.string.autofill_payment_methods);
        setHasOptionsMenu(true);
        PreferenceScreen screen = getPreferenceManager().createPreferenceScreen(getStyledContext());
        // Suppresses unwanted animations while Preferences are removed from and re-added to the
        // screen.
        screen.setShouldUseGeneratedIds(false);

        setPreferenceScreen(screen);
    }

    @Override
    public void onCreateOptionsMenu(Menu menu, MenuInflater inflater) {
        menu.clear();
        MenuItem help =
                menu.add(Menu.NONE, R.id.menu_id_targeted_help, Menu.NONE, R.string.menu_help);
        help.setIcon(R.drawable.ic_help_and_feedback);
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        if (item.getItemId() == R.id.menu_id_targeted_help) {
            mHelpAndFeedbackLauncher.show(
                    getActivity(), getActivity().getString(R.string.help_context_autofill), null);
            return true;
        }
        return super.onOptionsItemSelected(item);
    }

    @Override
    public void onResume() {
        super.onResume();
        // Always rebuild our list of credit cards.  Although we could detect if credit cards are
        // added or deleted, the credit card summary (number) might be different.  To be safe, we
        // update all.
        rebuildPage();
    }

    private void rebuildPage() {
        getPreferenceScreen().removeAll();
        getPreferenceScreen().setOrderingAsAdded(true);

        ChromeSwitchPreference autofillSwitch =
                new ChromeSwitchPreference(getStyledContext(), null);
        autofillSwitch.setTitle(R.string.autofill_enable_credit_cards_toggle_label);
        autofillSwitch.setSummary(R.string.autofill_enable_credit_cards_toggle_sublabel);
        autofillSwitch.setChecked(PersonalDataManager.isAutofillCreditCardEnabled());
        autofillSwitch.setOnPreferenceChangeListener((preference, newValue) -> {
            PersonalDataManager.setAutofillCreditCardEnabled((boolean) newValue);
            return true;
        });
        autofillSwitch.setManagedPreferenceDelegate(new ChromeManagedPreferenceDelegate() {
            @Override
            public boolean isPreferenceControlledByPolicy(Preference preference) {
                return PersonalDataManager.isAutofillCreditCardManaged();
            }

            @Override
            public boolean isPreferenceClickDisabled(Preference preference) {
                return PersonalDataManager.isAutofillCreditCardManaged()
                        && !PersonalDataManager.isAutofillCreditCardEnabled();
            }
        });
        getPreferenceScreen().addPreference(autofillSwitch);

        if (isBiometricAvailable()
                && PersonalDataManager.getInstance().isFidoAuthenticationAvailable()) {
            ChromeSwitchPreference fidoAuthSwitch =
                    new ChromeSwitchPreference(getStyledContext(), null);
            fidoAuthSwitch.setTitle(R.string.enable_credit_card_fido_auth_label);
            fidoAuthSwitch.setSummary(R.string.enable_credit_card_fido_auth_sublabel);
            fidoAuthSwitch.setChecked(PersonalDataManager.isAutofillCreditCardFidoAuthEnabled());
            fidoAuthSwitch.setOnPreferenceChangeListener((preference, newValue) -> {
                PersonalDataManager.setAutofillCreditCardFidoAuthEnabled((boolean) newValue);
                return true;
            });
            getPreferenceScreen().addPreference(fidoAuthSwitch);
        }

        // TODO(crbug.com/1427216): Confirm with Product on the order of the toggles.
        if (ChromeFeatureList.isEnabled(
                    ChromeFeatureList.AUTOFILL_ENABLE_PAYMENTS_MANDATORY_REAUTH)) {
            if (mReauthenticatorBridge == null) {
                // The DeviceAuthRequester value also determines canUseAuthentication() underlying
                // logic. Here we set a value to ensure it checks biometric only (exclude screen
                // lock).
                // TODO(crbug.com/1434875): Update when we split canUseAuthentication() function.
                mReauthenticatorBridge = ReauthenticatorBridge.create(
                        DeviceAuthRequester.PAYMENT_METHODS_REAUTH_IN_SETTINGS);
            }
            // We don't show the Reauth toggle when Autofill credit card is disabled or the device
            // doesn't have biometric auth.
            if (PersonalDataManager.isAutofillCreditCardEnabled()
                    && mReauthenticatorBridge.canUseAuthentication()) {
                ChromeSwitchPreference mandatoryReauthSwitch =
                        new ChromeSwitchPreference(getStyledContext(), null);
                mandatoryReauthSwitch.setTitle(
                        R.string.autofill_settings_page_enable_payment_method_mandatory_reauth_label);
                mandatoryReauthSwitch.setSummary(
                        R.string.autofill_settings_page_enable_payment_method_mandatory_reauth_sublabel);
                mandatoryReauthSwitch.setChecked(
                        PersonalDataManager.isAutofillPaymentMethodsMandatoryReauthEnabled());
                mandatoryReauthSwitch.setKey(PREF_MANDATORY_REAUTH);
                mandatoryReauthSwitch.setOnPreferenceChangeListener(
                        this::onMandatoryReauthSwitchToggled);
                getPreferenceScreen().addPreference(mandatoryReauthSwitch);
            }
        }

        for (CreditCard card : PersonalDataManager.getInstance().getCreditCardsForSettings()) {
            // Add a preference for the credit card.
            Preference card_pref = new Preference(getStyledContext());
            // Make the card_pref multi-line, since cards with long nicknames won't fit on a single
            // line.
            card_pref.setSingleLineTitle(false);
            card_pref.setTitle(card.getCardLabel());

            // Show virtual card enabled status for enrolled cards, expiration date otherwise.
            if (card.getVirtualCardEnrollmentState() == VirtualCardEnrollmentState.ENROLLED
                    && ChromeFeatureList.isEnabled(
                            ChromeFeatureList.AUTOFILL_ENABLE_VIRTUAL_CARD_METADATA)) {
                card_pref.setSummary(R.string.autofill_virtual_card_enrolled_text);
            } else {
                card_pref.setSummary(
                        card.getFormattedExpirationDateWithTwoDigitYear(getActivity()));
            }

            // Set card icon. It can be either a custom card art or a network icon.
            card_pref.setIcon(getCardIcon(getStyledContext(), card.getCardArtUrl(),
                    card.getIssuerIconDrawableId(), getSettingsPageIconWidthId(),
                    getSettingsPageIconHeightId(), R.dimen.card_art_corner_radius,
                    ChromeFeatureList.isEnabled(ChromeFeatureList.AUTOFILL_ENABLE_CARD_ART_IMAGE)));

            if (card.getIsLocal()) {
                card_pref.setFragment(AutofillLocalCardEditor.class.getName());
            } else {
                card_pref.setFragment(AutofillServerCardEditor.class.getName());
                if (ChromeFeatureList.isEnabled(
                            ChromeFeatureList.AUTOFILL_ENABLE_VIRTUAL_CARD_METADATA)) {
                    card_pref.setWidgetLayoutResource(R.layout.autofill_server_data_label);
                } else {
                    card_pref.setWidgetLayoutResource(R.layout.autofill_server_data_text_label);
                }
            }

            Bundle args = card_pref.getExtras();
            args.putString(AutofillEditorBase.AUTOFILL_GUID, card.getGUID());
            getPreferenceScreen().addPreference(card_pref);
        }

        // Add 'Add credit card' button. Tap of it brings up card editor which allows users type in
        // new credit cards.
        if (PersonalDataManager.isAutofillCreditCardEnabled()) {
            Preference add_card_pref = new Preference(getStyledContext());
            Drawable plusIcon = ApiCompatibilityUtils.getDrawable(getResources(), R.drawable.plus);
            plusIcon.mutate();
            plusIcon.setColorFilter(SemanticColorUtils.getDefaultControlColorActive(getContext()),
                    PorterDuff.Mode.SRC_IN);
            add_card_pref.setIcon(plusIcon);
            add_card_pref.setTitle(R.string.autofill_create_credit_card);
            add_card_pref.setFragment(AutofillLocalCardEditor.class.getName());
            getPreferenceScreen().addPreference(add_card_pref);
        }

        // Add the link to payment apps only after the credit card list is rebuilt.
        Preference payment_apps_pref = new Preference(getStyledContext());
        payment_apps_pref.setTitle(R.string.payment_apps_title);
        payment_apps_pref.setFragment(AndroidPaymentAppsFragment.class.getCanonicalName());
        payment_apps_pref.setShouldDisableView(true);
        payment_apps_pref.setKey(PREF_PAYMENT_APPS);
        getPreferenceScreen().addPreference(payment_apps_pref);
        refreshPaymentAppsPrefForAndroidPaymentApps(payment_apps_pref);
    }

    private Context getStyledContext() {
        return getPreferenceManager().getContext();
    }

    private void refreshPaymentAppsPrefForAndroidPaymentApps(Preference pref) {
        if (AndroidPaymentAppFactory.hasAndroidPaymentApps()) {
            setPaymentAppsPrefStatus(pref, true);
        } else {
            refreshPaymentAppsPrefForServiceWorkerPaymentApps(pref);
        }
    }

    private void refreshPaymentAppsPrefForServiceWorkerPaymentApps(Preference pref) {
        ServiceWorkerPaymentAppBridge.hasServiceWorkerPaymentApps(
                new ServiceWorkerPaymentAppBridge.HasServiceWorkerPaymentAppsCallback() {
                    @Override
                    public void onHasServiceWorkerPaymentAppsResponse(boolean hasPaymentApps) {
                        setPaymentAppsPrefStatus(pref, hasPaymentApps);
                    }
                });
    }

    private void setPaymentAppsPrefStatus(Preference pref, boolean enabled) {
        if (enabled) {
            pref.setSummary(null);
            pref.setEnabled(true);
        } else {
            pref.setSummary(R.string.payment_no_apps_summary);
            pref.setEnabled(false);
        }
    }

    private boolean isBiometricAvailable() {
        // Only Android versions 9 and above are supported.
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.P) {
            return false;
        }

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            BiometricManager biometricManager =
                    getStyledContext().getSystemService(BiometricManager.class);
            return biometricManager != null
                    && biometricManager.canAuthenticate() == BiometricManager.BIOMETRIC_SUCCESS;
        } else {
            // For API level < Q, we will use FingerprintManagerCompat to check enrolled
            // fingerprints. Note that for API level lower than 23, FingerprintManagerCompat behaves
            // like no fingerprint hardware and no enrolled fingerprints.
            FingerprintManagerCompat fingerprintManager =
                    FingerprintManagerCompat.from(getStyledContext());
            return fingerprintManager != null && fingerprintManager.isHardwareDetected()
                    && fingerprintManager.hasEnrolledFingerprints();
        }
    }

    /** Handle preference changes from mandatory reauth toggle */
    private boolean onMandatoryReauthSwitchToggled(Preference preference, Object newValue) {
        assert preference.getKey().equals(PREF_MANDATORY_REAUTH);
        // We require user authentication every time user trys to change this
        // preference. Set useLastValidAuth=false to skip the grace period.
        mReauthenticatorBridge.reauthenticate(success -> {
            if (success) {
                // Only set the preference to new value when user passes the
                // authentication.
                PersonalDataManager.setAutofillPaymentMethodsMandatoryReauth((boolean) newValue);
            }
        }, /*useLastValidAuth=*/false);
        // Returning false here holds the toggle to still display the old value while
        // waiting for biometric auth. Once biometric is completed (either succeed or
        // fail), OnResume will reload the page with the pref value, which will switch
        // to the new value if biometric auth succeeded.
        return false;
    }

    @Override
    public void onPersonalDataChanged() {
        rebuildPage();
    }

    @Override
    public void onActivityCreated(Bundle savedInstanceState) {
        super.onActivityCreated(savedInstanceState);
        PersonalDataManager.getInstance().registerDataObserver(this);
    }

    @Override
    public void onDestroyView() {
        PersonalDataManager.getInstance().unregisterDataObserver(this);
        super.onDestroyView();
    }

    @Override
    public void setHelpAndFeedbackLauncher(HelpAndFeedbackLauncher helpAndFeedbackLauncher) {
        mHelpAndFeedbackLauncher = helpAndFeedbackLauncher;
    }
}
