// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.privacy_sandbox;

import static org.chromium.components.browser_ui.settings.SearchUtils.handleSearchNavigation;
import static org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridge.SITE_WILDCARD;

import android.os.Bundle;
import android.text.Spannable;
import android.text.SpannableStringBuilder;
import android.text.style.ClickableSpan;
import android.text.style.ForegroundColorSpan;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;

import androidx.annotation.ColorInt;
import androidx.annotation.VisibleForTesting;
import androidx.preference.Preference;
import androidx.preference.Preference.OnPreferenceClickListener;
import androidx.preference.PreferenceFragmentCompat;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.browser_ui.settings.CustomDividerFragment;
import org.chromium.components.browser_ui.settings.EmbeddableSettingsPage;
import org.chromium.components.browser_ui.settings.ExpandablePreferenceGroup;
import org.chromium.components.browser_ui.settings.SearchUtils;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.browser_ui.settings.TextMessagePreference;
import org.chromium.components.browser_ui.site_settings.AddExceptionPreference;
import org.chromium.components.browser_ui.site_settings.AddExceptionPreference.SiteAddedCallback;
import org.chromium.components.browser_ui.site_settings.SiteSettingsCategory;
import org.chromium.components.browser_ui.site_settings.Website;
import org.chromium.components.browser_ui.site_settings.WebsitePermissionsFetcher;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridge;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.browser_ui.util.TraceEventVectorDrawableCompat;
import org.chromium.components.content_settings.ContentSettingValues;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.privacy_sandbox.CustomTabs.CustomTabIntentHelper;
import org.chromium.ui.text.SpanApplier;
import org.chromium.ui.widget.Toast;

import java.util.ArrayList;
import java.util.Collection;
import java.util.List;
import java.util.Locale;

/** Fragment to manage settings for tracking protection. */
public class TrackingProtectionSettings extends PreferenceFragmentCompat
        implements CustomDividerFragment,
                OnPreferenceClickListener,
                SiteAddedCallback,
                EmbeddableSettingsPage {
    private static final String PREF_BLOCK_ALL_TOGGLE = "block_all_3pcd_toggle";
    private static final String PREF_IP_PROTECTION_TOGGLE = "ip_protection_toggle";
    private static final String PREF_IP_PROTECTION_LEARN_MORE = "ip_protection_learn_more";
    private static final String PREF_FINGERPRINTING_PROTECTION_TOGGLE =
            "fingerprinting_protection_toggle";
    private static final String PREF_FINGERPRINTING_PROTECTION_LEARN_MORE =
            "fingerprinting_protection_learn_more";
    private static final String PREF_BULLET_TWO = "bullet_point_two";
    private static final String ALLOWED_GROUP = "allowed_group";
    public static final String ADD_EXCEPTION_KEY = "add_exception";
    @VisibleForTesting static final String PREF_DNT_TOGGLE = "dnt_toggle";

    public static final String LEARN_MORE_URL =
            "https://support.google.com/chrome/?p=pause_protections";

    // The number of sites that are on the Allowed list.
    private int mAllowedSiteCount;

    // Whether the Allowed list should be shown expanded.
    private boolean mAllowListExpanded = true;

    // The item for searching the list of items.
    private MenuItem mSearchItem;

    // If not blank, represents a substring to use to search for site names.
    private String mSearch;

    private TrackingProtectionDelegate mDelegate;

    private CustomTabIntentHelper mCustomTabIntentHelper;

    private final ObservableSupplierImpl<String> mPageTitle = new ObservableSupplierImpl<>();

    @Override
    public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
        SettingsUtils.addPreferencesFromResource(this, R.xml.tracking_protection_preferences);
        if (mDelegate.shouldShowTrackingProtectionBrandedUi()) {
            mPageTitle.set(getString(R.string.privacy_sandbox_tracking_protection_title));
        } else {
            mPageTitle.set(getString(R.string.third_party_cookies_page_title));
        }

        setHasOptionsMenu(true);

        // Format the Learn More link in the second bullet point.
        TextMessagePreference bulletTwo = (TextMessagePreference) findPreference(PREF_BULLET_TWO);
        bulletTwo.setSummary(
                SpanApplier.applySpans(
                        getResources()
                                .getString(
                                        R.string
                                                .privacy_sandbox_tracking_protection_bullet_two_description),
                        new SpanApplier.SpanInfo(
                                "<link>",
                                "</link>",
                                new ClickableSpan() {
                                    @Override
                                    public void onClick(View view) {
                                        onLearnMoreClicked();
                                    }
                                })));

        ChromeSwitchPreference blockAll3pCookiesSwitch =
                (ChromeSwitchPreference) findPreference(PREF_BLOCK_ALL_TOGGLE);
        ChromeSwitchPreference ipProtectionSwitch =
                (ChromeSwitchPreference) findPreference(PREF_IP_PROTECTION_TOGGLE);
        TextMessagePreference ipProtectionLearnMore =
                (TextMessagePreference) findPreference(PREF_IP_PROTECTION_LEARN_MORE);
        ChromeSwitchPreference fingerprintingProtectionSwitch =
                (ChromeSwitchPreference) findPreference(PREF_FINGERPRINTING_PROTECTION_TOGGLE);
        TextMessagePreference fingerprintingProtectionLearnMore =
                (TextMessagePreference) findPreference(PREF_FINGERPRINTING_PROTECTION_LEARN_MORE);
        ChromeSwitchPreference doNotTrackSwitch =
                (ChromeSwitchPreference) findPreference(PREF_DNT_TOGGLE);

        // Block all 3rd party cookies switch.
        blockAll3pCookiesSwitch.setChecked(mDelegate.isBlockAll3pcEnabled());
        blockAll3pCookiesSwitch.setOnPreferenceChangeListener(
                (preference, newValue) -> {
                    mDelegate.setBlockAll3pc((boolean) newValue);
                    return true;
                });

        // IP protection switch.
        if (mDelegate.shouldDisplayIpProtection()) {
            ipProtectionSwitch.setVisible(true);
            ipProtectionSwitch.setChecked(mDelegate.isIpProtectionEnabled());
            ipProtectionSwitch.setOnPreferenceChangeListener(
                    (preference, newValue) -> {
                        mDelegate.setIpProtection((boolean) newValue);
                        return true;
                    });
            ipProtectionLearnMore.setVisible(true);
            // TODO(b/330745124): Update the learn more action.
            ipProtectionLearnMore.setSummary(
                    SpanApplier.applySpans(
                            getResources()
                                    .getString(
                                            R.string.tracking_protection_ip_protection_learn_more),
                            new SpanApplier.SpanInfo(
                                    "<link>",
                                    "</link>",
                                    new ClickableSpan() {
                                        @Override
                                        public void onClick(View view) {
                                            onLearnMoreClicked();
                                        }
                                    })));
        }

        // Fingerprinting protection switch.
        if (mDelegate.shouldDisplayFingerprintingProtection()) {
            fingerprintingProtectionSwitch.setVisible(true);
            fingerprintingProtectionSwitch.setChecked(
                    mDelegate.isFingerprintingProtectionEnabled());
            fingerprintingProtectionSwitch.setOnPreferenceChangeListener(
                    (preference, newValue) -> {
                        mDelegate.setFingerprintingProtection((boolean) newValue);
                        return true;
                    });
            fingerprintingProtectionLearnMore.setVisible(true);
            // TODO(b/330745124): Update the learn more action.
            fingerprintingProtectionLearnMore.setSummary(
                    SpanApplier.applySpans(
                            getResources()
                                    .getString(
                                            R.string
                                                    .tracking_protection_fingerprinting_protection_learn_more),
                            new SpanApplier.SpanInfo(
                                    "<link>",
                                    "</link>",
                                    new ClickableSpan() {
                                        @Override
                                        public void onClick(View view) {
                                            onLearnMoreClicked();
                                        }
                                    })));
        }

        // Do not track switch.
        if (mDelegate.shouldShowTrackingProtectionBrandedUi()) {
            doNotTrackSwitch.setVisible(true);
            doNotTrackSwitch.setChecked(mDelegate.isDoNotTrackEnabled());
            doNotTrackSwitch.setOnPreferenceChangeListener(
                    (preference, newValue) -> {
                        mDelegate.setDoNotTrack((boolean) newValue);
                        return true;
                    });
        }

        mAllowListExpanded = true;
        mAllowedSiteCount = 0;
        ExpandablePreferenceGroup allowedGroup =
                getPreferenceScreen().findPreference(ALLOWED_GROUP);
        allowedGroup.setOnPreferenceClickListener(this);
        refreshBlockingExceptions();

        // Add the exceptions button.
        SiteSettingsCategory cookiesCategory =
                SiteSettingsCategory.createFromType(
                        mDelegate.getBrowserContext(),
                        SiteSettingsCategory.Type.THIRD_PARTY_COOKIES);
        getPreferenceScreen()
                .addPreference(
                        new AddExceptionPreference(
                                getContext(),
                                ADD_EXCEPTION_KEY,
                                getString(
                                        R.string
                                                .website_settings_third_party_cookies_page_add_allow_exception_description),
                                cookiesCategory,
                                this));
    }

    @Override
    public ObservableSupplier<String> getPageTitle() {
        return mPageTitle;
    }

    @Override
    public void onCreateOptionsMenu(Menu menu, MenuInflater inflater) {
        menu.clear();
        inflater.inflate(R.menu.tracking_protection_menu, menu);

        mSearchItem = menu.findItem(R.id.search);
        SearchUtils.initializeSearchView(
                mSearchItem,
                mSearch,
                getActivity(),
                (query) -> {
                    boolean queryHasChanged =
                            mSearch == null
                                    ? query != null && !query.isEmpty()
                                    : !mSearch.equals(query);
                    mSearch = query;
                    if (queryHasChanged) refreshBlockingExceptions();
                });

        MenuItem help =
                menu.add(Menu.NONE, R.id.menu_id_site_settings_help, Menu.NONE, R.string.menu_help);
        help.setIcon(
                TraceEventVectorDrawableCompat.create(
                        getResources(), R.drawable.ic_help_and_feedback, getContext().getTheme()));
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        if (item.getItemId() == R.id.menu_id_site_settings_help) {
            mDelegate
                    .getSiteSettingsDelegate(getContext())
                    .launchSettingsHelpAndFeedbackActivity(getActivity());
            return true;
        }
        if (handleSearchNavigation(item, mSearchItem, mSearch, getActivity())) {
            boolean queryHasChanged = mSearch != null && !mSearch.isEmpty();
            mSearch = null;
            if (queryHasChanged) refreshBlockingExceptions();
            return true;
        }
        return false;
    }

    @Override
    public void onResume() {
        super.onResume();
        if (mSearch == null && mSearchItem != null) {
            SearchUtils.clearSearch(mSearchItem, getActivity());
            mSearch = null;
        }
        refreshBlockingExceptions();
    }

    @Override
    public boolean hasDivider() {
        // Remove dividers between preferences.
        return false;
    }

    // OnPreferenceClickListener:
    @Override
    public boolean onPreferenceClick(Preference preference) {
        if (ALLOWED_GROUP.equals(preference.getKey())) {
            mAllowListExpanded = !mAllowListExpanded;
        }
        refreshBlockingExceptions();
        return true;
    }

    // AddExceptionPreference.SiteAddedCallback:
    @Override
    public void onAddSite(String primaryPattern, String secondaryPattern) {
        WebsitePreferenceBridge.setContentSettingCustomScope(
                mDelegate.getBrowserContext(),
                ContentSettingsType.COOKIES,
                primaryPattern,
                secondaryPattern,
                ContentSettingValues.ALLOW);

        String hostname = primaryPattern.equals(SITE_WILDCARD) ? secondaryPattern : primaryPattern;
        Toast.makeText(
                        getContext(),
                        getContext().getString(R.string.website_settings_add_site_toast, hostname),
                        Toast.LENGTH_SHORT)
                .show();

        refreshBlockingExceptions();
    }

    public void setTrackingProtectionDelegate(TrackingProtectionDelegate delegate) {
        mDelegate = delegate;
    }

    private void refreshBlockingExceptions() {
        SiteSettingsCategory cookiesCategory =
                SiteSettingsCategory.createFromType(
                        mDelegate.getBrowserContext(),
                        SiteSettingsCategory.Type.THIRD_PARTY_COOKIES);
        new WebsitePermissionsFetcher(mDelegate.getSiteSettingsDelegate(getContext()))
                .fetchPreferencesForCategory(cookiesCategory, this::onExceptionsFetched);
    }

    private void onExceptionsFetched(Collection<Website> sites) {
        List<WebsiteExceptionRowPreference> websites = new ArrayList<>();
        for (Website site : sites) {
            if (mSearch == null || mSearch.isEmpty() || site.getTitle().contains(mSearch)) {
                WebsiteExceptionRowPreference preference =
                        new WebsiteExceptionRowPreference(
                                getContext(), site, mDelegate, this::refreshBlockingExceptions);
                websites.add(preference);
            }
        }

        ExpandablePreferenceGroup allowedGroup =
                getPreferenceScreen().findPreference(ALLOWED_GROUP);
        allowedGroup.removeAll();
        // Add the description preference.
        var description = new TextMessagePreference(getContext(), null);
        description.setSummary(getString(R.string.tracking_protection_allowed_group_description));
        allowedGroup.addPreference(description);
        mAllowedSiteCount = 0;
        for (WebsiteExceptionRowPreference website : websites) {
            allowedGroup.addPreference(website);
            mAllowedSiteCount++;
        }
        if (!mAllowListExpanded) allowedGroup.removeAll();
        updateExceptionsHeader();
    }

    private void updateExceptionsHeader() {
        ExpandablePreferenceGroup allowedGroup =
                getPreferenceScreen().findPreference(ALLOWED_GROUP);
        SpannableStringBuilder spannable =
                new SpannableStringBuilder(
                        getString(R.string.tracking_protection_allowed_group_title));
        String prefCount = String.format(Locale.getDefault(), " - %d", mAllowedSiteCount);
        spannable.append(prefCount);

        // Color the first part of the title blue.
        ForegroundColorSpan blueSpan =
                new ForegroundColorSpan(
                        SemanticColorUtils.getDefaultTextColorAccent1(getContext()));
        spannable.setSpan(
                blueSpan,
                0,
                spannable.length() - prefCount.length(),
                Spannable.SPAN_EXCLUSIVE_EXCLUSIVE);

        // Gray out the total count of items.
        final @ColorInt int gray = SemanticColorUtils.getDefaultTextColorSecondary(getContext());
        spannable.setSpan(
                new ForegroundColorSpan(gray),
                spannable.length() - prefCount.length(),
                spannable.length(),
                Spannable.SPAN_EXCLUSIVE_EXCLUSIVE);

        // Configure the preference group.
        allowedGroup.setTitle(spannable);
        allowedGroup.setExpanded(mAllowListExpanded);
    }

    private void onLearnMoreClicked() {
        CustomTabs.openUrlInCct(mCustomTabIntentHelper, getContext(), LEARN_MORE_URL);
    }

    public void setCustomTabIntentHelper(CustomTabIntentHelper helper) {
        mCustomTabIntentHelper = helper;
    }
}
