// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy.settings;

import static org.chromium.components.content_settings.PrefNames.COOKIE_CONTROLS_MODE;

import android.os.Build;
import android.os.Bundle;
import android.text.SpannableString;
import android.text.style.ForegroundColorSpan;
import android.text.style.RelativeSizeSpan;
import android.text.style.SuperscriptSpan;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;

import androidx.preference.Preference;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.enterprise.util.ManagedBrowserUtils;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.incognito.reauth.IncognitoReauthSettingSwitchPreference;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.prefetch.settings.PreloadPagesSettingsFragment;
import org.chromium.chrome.browser.privacy.secure_dns.SecureDnsSettings;
import org.chromium.chrome.browser.privacy_guide.PrivacyGuideInteractions;
import org.chromium.chrome.browser.privacy_sandbox.PrivacySandboxBridge;
import org.chromium.chrome.browser.privacy_sandbox.PrivacySandboxReferrer;
import org.chromium.chrome.browser.privacy_sandbox.PrivacySandboxSettingsBaseFragment;
import org.chromium.chrome.browser.safe_browsing.SafeBrowsingBridge;
import org.chromium.chrome.browser.safe_browsing.metrics.SettingsAccessPoint;
import org.chromium.chrome.browser.safe_browsing.settings.SafeBrowsingSettingsFragment;
import org.chromium.chrome.browser.settings.ChromeBaseSettingsFragment;
import org.chromium.chrome.browser.settings.ChromeManagedPreferenceDelegate;
import org.chromium.chrome.browser.settings.SettingsLauncherImpl;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.sync.settings.GoogleServicesSettings;
import org.chromium.chrome.browser.sync.settings.ManageSyncSettings;
import org.chromium.chrome.browser.usage_stats.UsageStatsConsentDialog;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.browser_ui.settings.SettingsLauncher;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.browser_ui.site_settings.ContentSettingsResources;
import org.chromium.components.browser_ui.site_settings.SingleCategorySettings;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.browser_ui.util.TraceEventVectorDrawableCompat;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.ui.text.NoUnderlineClickableSpan;
import org.chromium.ui.text.SpanApplier;

/**
 * Fragment to keep track of the all the privacy related preferences.
 */
public class PrivacySettings
        extends ChromeBaseSettingsFragment implements Preference.OnPreferenceChangeListener {
    private static final String PREF_CAN_MAKE_PAYMENT = "can_make_payment";
    private static final String PREF_PRELOAD_PAGES = "preload_pages";
    private static final String PREF_HTTPS_FIRST_MODE = "https_first_mode";
    private static final String PREF_SECURE_DNS = "secure_dns";
    private static final String PREF_USAGE_STATS = "usage_stats_reporting";
    private static final String PREF_DO_NOT_TRACK = "do_not_track";
    private static final String PREF_SAFE_BROWSING = "safe_browsing";
    private static final String PREF_SYNC_AND_SERVICES_LINK = "sync_and_services_link";
    private static final String PREF_PRIVACY_SANDBOX = "privacy_sandbox";
    private static final String PREF_PRIVACY_GUIDE = "privacy_guide";
    private static final String PREF_INCOGNITO_LOCK = "incognito_lock";
    private static final String PREF_THIRD_PARTY_COOKIES = "third_party_cookies";
    private static final String PREF_TRACKING_PROTECTION = "tracking_protection";

    private IncognitoLockSettings mIncognitoLockSettings;

    @Override
    public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
        getActivity().setTitle(R.string.prefs_privacy_security);

        SettingsUtils.addPreferencesFromResource(this, R.xml.privacy_preferences);

        Preference sandboxPreference = findPreference(PREF_PRIVACY_SANDBOX);
        // Overwrite the click listener to pass a correct referrer to the fragment.
        sandboxPreference.setOnPreferenceClickListener(preference -> {
            PrivacySandboxSettingsBaseFragment.launchPrivacySandboxSettings(getContext(),
                    new SettingsLauncherImpl(), PrivacySandboxReferrer.PRIVACY_SETTINGS);
            return true;
        });

        if (PrivacySandboxBridge.isPrivacySandboxRestricted()) {
            if (PrivacySandboxBridge.isRestrictedNoticeEnabled()) {
                // Update the summary to one that describes only ad measurement if ad-measurement
                // is available to restricted users.
                sandboxPreference.setSummary(getContext().getString(
                        R.string.settings_ad_privacy_restricted_link_row_sub_label));
            } else {
                // Hide the Privacy Sandbox if it is restricted and ad-measurement is not
                // available to restricted users.
                getPreferenceScreen().removePreference(sandboxPreference);
            }
        }

        Preference privacyGuidePreference = findPreference(PREF_PRIVACY_GUIDE);
        // Record the launch of PG from the S&P link-row entry point
        privacyGuidePreference.setOnPreferenceClickListener(preference -> {
            RecordUserAction.record("Settings.PrivacyGuide.StartPrivacySettings");
            RecordHistogram.recordEnumeratedHistogram("Settings.PrivacyGuide.EntryExit",
                    PrivacyGuideInteractions.SETTINGS_LINK_ROW_ENTRY,
                    PrivacyGuideInteractions.MAX_VALUE);
            UserPrefs.get(getProfile()).setBoolean(Pref.PRIVACY_GUIDE_VIEWED, true);
            return false;
        });
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.PRIVACY_GUIDE) || getProfile().isChild()
                || ManagedBrowserUtils.isBrowserManaged(getProfile())) {
            getPreferenceScreen().removePreference(privacyGuidePreference);
        }

        IncognitoReauthSettingSwitchPreference incognitoReauthPreference =
                (IncognitoReauthSettingSwitchPreference) findPreference(PREF_INCOGNITO_LOCK);
        mIncognitoLockSettings = new IncognitoLockSettings(incognitoReauthPreference, getProfile());
        mIncognitoLockSettings.setUpIncognitoReauthPreference(getActivity());

        Preference safeBrowsingPreference = findPreference(PREF_SAFE_BROWSING);
        safeBrowsingPreference.setSummary(
                SafeBrowsingSettingsFragment.getSafeBrowsingSummaryString(getContext()));
        safeBrowsingPreference.setOnPreferenceClickListener((preference) -> {
            preference.getExtras().putInt(
                    SafeBrowsingSettingsFragment.ACCESS_POINT, SettingsAccessPoint.PARENT_SETTINGS);
            return false;
        });

        setHasOptionsMenu(true);

        ChromeSwitchPreference canMakePaymentPref =
                (ChromeSwitchPreference) findPreference(PREF_CAN_MAKE_PAYMENT);
        canMakePaymentPref.setOnPreferenceChangeListener(this);

        ChromeSwitchPreference httpsFirstModePref =
                (ChromeSwitchPreference) findPreference(PREF_HTTPS_FIRST_MODE);
        httpsFirstModePref.setOnPreferenceChangeListener(this);
        httpsFirstModePref.setManagedPreferenceDelegate(new ChromeManagedPreferenceDelegate(
                getProfile()) {
            @Override
            public boolean isPreferenceControlledByPolicy(Preference preference) {
                String key = preference.getKey();
                assert PREF_HTTPS_FIRST_MODE.equals(key) : "Unexpected preference key: " + key;
                return UserPrefs.get(getProfile())
                        .isManagedPreference(Pref.HTTPS_ONLY_MODE_ENABLED);
            }

            @Override
            public boolean isPreferenceClickDisabled(Preference preference) {
                // Advanced Protection automatically enables HTTPS-Only Mode so
                // lock the setting.
                return isPreferenceControlledByPolicy(preference)
                        || SafeBrowsingBridge.isUnderAdvancedProtection();
            }
        });
        httpsFirstModePref.setChecked(
                UserPrefs.get(getProfile()).getBoolean(Pref.HTTPS_ONLY_MODE_ENABLED));
        if (SafeBrowsingBridge.isUnderAdvancedProtection()) {
            httpsFirstModePref.setSummary(getContext().getResources().getString(
                    R.string.settings_https_first_mode_with_advanced_protection_summary));
        }

        Preference secureDnsPref = findPreference(PREF_SECURE_DNS);
        secureDnsPref.setVisible(SecureDnsSettings.isUiEnabled());

        Preference syncAndServicesLink = findPreference(PREF_SYNC_AND_SERVICES_LINK);
        syncAndServicesLink.setSummary(buildSyncAndServicesLink());

        Preference thirdPartyCookies = findPreference(PREF_THIRD_PARTY_COOKIES);
        Preference doNotTrackPref = findPreference(PREF_DO_NOT_TRACK);

        if (showTrackingProtectionUI()) {
            if (thirdPartyCookies != null) thirdPartyCookies.setVisible(false);
            if (doNotTrackPref != null) doNotTrackPref.setVisible(false);
            Preference trackingProtection = findPreference(PREF_TRACKING_PROTECTION);
            trackingProtection.setVisible(true);
        } else if (thirdPartyCookies != null) {
            thirdPartyCookies.getExtras().putString(
                    SingleCategorySettings.EXTRA_CATEGORY, thirdPartyCookies.getKey());
            thirdPartyCookies.getExtras().putString(
                    SingleCategorySettings.EXTRA_TITLE, thirdPartyCookies.getTitle().toString());
        }

        updatePreferences();
    }

    private SpannableString buildSyncAndServicesLink() {
        SettingsLauncher settingsLauncher = new SettingsLauncherImpl();
        NoUnderlineClickableSpan servicesLink = new NoUnderlineClickableSpan(getContext(), v -> {
            settingsLauncher.launchSettingsActivity(getActivity(), GoogleServicesSettings.class);
        });
        if (IdentityServicesProvider.get()
                        .getIdentityManager(getProfile())
                        .getPrimaryAccountInfo(ConsentLevel.SYNC)
                == null) {
            // Sync is off, show the string with one link to "Google Services".
            return SpanApplier.applySpans(
                    getString(R.string.privacy_sync_and_services_link_sync_off),
                    new SpanApplier.SpanInfo("<link>", "</link>", servicesLink));
        }
        // Otherwise, show the string with both links to "Sync" and "Google Services".
        NoUnderlineClickableSpan syncLink = new NoUnderlineClickableSpan(getContext(), v -> {
            settingsLauncher.launchSettingsActivity(getActivity(), ManageSyncSettings.class,
                    ManageSyncSettings.createArguments(false));
        });
        return SpanApplier.applySpans(getString(R.string.privacy_sync_and_services_link_sync_on),
                new SpanApplier.SpanInfo("<link1>", "</link1>", syncLink),
                new SpanApplier.SpanInfo("<link2>", "</link2>", servicesLink));
    }

    @Override
    public boolean onPreferenceChange(Preference preference, Object newValue) {
        String key = preference.getKey();
        if (PREF_CAN_MAKE_PAYMENT.equals(key)) {
            UserPrefs.get(getProfile())
                    .setBoolean(Pref.CAN_MAKE_PAYMENT_ENABLED, (boolean) newValue);
        } else if (PREF_HTTPS_FIRST_MODE.equals(key)) {
            UserPrefs.get(getProfile())
                    .setBoolean(Pref.HTTPS_ONLY_MODE_ENABLED, (boolean) newValue);
        }
        return true;
    }

    @Override
    public void onResume() {
        super.onResume();
        updatePreferences();
    }

    /**
     * Updates the preferences.
     */
    public void updatePreferences() {
        ChromeSwitchPreference canMakePaymentPref =
                (ChromeSwitchPreference) findPreference(PREF_CAN_MAKE_PAYMENT);
        if (canMakePaymentPref != null) {
            canMakePaymentPref.setChecked(
                    UserPrefs.get(getProfile()).getBoolean(Pref.CAN_MAKE_PAYMENT_ENABLED));
        }

        Preference doNotTrackPref = findPreference(PREF_DO_NOT_TRACK);
        if (doNotTrackPref != null) {
            doNotTrackPref.setSummary(
                    UserPrefs.get(getProfile()).getBoolean(Pref.ENABLE_DO_NOT_TRACK)
                            ? R.string.text_on
                            : R.string.text_off);
        }

        Preference preloadPagesPreference = findPreference(PREF_PRELOAD_PAGES);
        if (preloadPagesPreference != null) {
            preloadPagesPreference.setSummary(
                    PreloadPagesSettingsFragment.getPreloadPagesSummaryString(getContext()));
        }

        Preference secureDnsPref = findPreference(PREF_SECURE_DNS);
        if (secureDnsPref != null && secureDnsPref.isVisible()) {
            secureDnsPref.setSummary(SecureDnsSettings.getSummary(getContext()));
        }

        Preference safeBrowsingPreference = findPreference(PREF_SAFE_BROWSING);
        if (safeBrowsingPreference != null && safeBrowsingPreference.isVisible()) {
            safeBrowsingPreference.setSummary(
                    SafeBrowsingSettingsFragment.getSafeBrowsingSummaryString(getContext()));
        }

        Preference usageStatsPref = findPreference(PREF_USAGE_STATS);
        if (usageStatsPref != null) {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q
                    && UserPrefs.get(getProfile()).getBoolean(Pref.USAGE_STATS_ENABLED)) {
                usageStatsPref.setOnPreferenceClickListener(preference -> {
                    UsageStatsConsentDialog
                            .create(getActivity(), true,
                                    (didConfirm) -> {
                                        if (didConfirm) {
                                            updatePreferences();
                                        }
                                    })
                            .show();
                    return true;
                });
            } else {
                getPreferenceScreen().removePreference(usageStatsPref);
            }
        }

        mIncognitoLockSettings.updateIncognitoReauthPreferenceIfNeeded(getActivity());

        Preference thirdPartyCookies = findPreference(PREF_THIRD_PARTY_COOKIES);
        if (thirdPartyCookies != null) {
            thirdPartyCookies.setSummary(ContentSettingsResources.getThirdPartyCookieListSummary(
                    UserPrefs.get(getProfile()).getInteger(COOKIE_CONTROLS_MODE)));
        }

        updatePrivacyGuidePreferenceTitle();
    }

    // TODO(crbug.com/1431101): This will be removed when the Privacy Guide is rolled out and no
    //  longer a new feature.
    private void updatePrivacyGuidePreferenceTitle() {
        Preference privacyGuide = findPreference(PREF_PRIVACY_GUIDE);
        if (privacyGuide == null) {
            return;
        }

        final CharSequence privacyGuidePrefTitle;
        if (!UserPrefs.get(getProfile()).getBoolean(Pref.PRIVACY_GUIDE_VIEWED)) {
            privacyGuidePrefTitle = SpanApplier.applySpans(
                    getString(R.string.privacy_guide_pref_title),
                    new SpanApplier.SpanInfo("<new>", "</new>", new SuperscriptSpan(),
                            new RelativeSizeSpan(0.75f),
                            new ForegroundColorSpan(
                                    SemanticColorUtils.getDefaultTextColorAccent1(getContext()))));
        } else {
            privacyGuidePrefTitle =
                    (CharSequence) (SpanApplier
                                            .removeSpanText(
                                                    getString(R.string.privacy_guide_pref_title),
                                                    new SpanApplier.SpanInfo("<new>", "</new>"))
                                            .trim());
        }
        privacyGuide.setTitle(privacyGuidePrefTitle);
    }

    private boolean showTrackingProtectionUI() {
        return UserPrefs.get(getProfile()).getBoolean(Pref.TRACKING_PROTECTION3PCD_ENABLED)
                || ChromeFeatureList.isEnabled(ChromeFeatureList.TRACKING_PROTECTION_3PCD);
    }

    @Override
    public void onCreateOptionsMenu(Menu menu, MenuInflater inflater) {
        menu.clear();
        MenuItem help =
                menu.add(Menu.NONE, R.id.menu_id_targeted_help, Menu.NONE, R.string.menu_help);
        help.setIcon(TraceEventVectorDrawableCompat.create(
                getResources(), R.drawable.ic_help_and_feedback, getActivity().getTheme()));
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        if (item.getItemId() == R.id.menu_id_targeted_help) {
            getHelpAndFeedbackLauncher().show(
                    getActivity(), getString(R.string.help_context_privacy), null);
            return true;
        }
        return false;
    }
}
