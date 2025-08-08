// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.privacy_sandbox;

import static org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridge.SITE_WILDCARD;

import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.provider.Browser;
import android.text.Spannable;
import android.text.SpannableStringBuilder;
import android.text.style.ForegroundColorSpan;
import android.view.View;

import androidx.annotation.ColorInt;
import androidx.browser.customtabs.CustomTabsIntent;
import androidx.preference.Preference;
import androidx.preference.Preference.OnPreferenceClickListener;
import androidx.preference.PreferenceFragmentCompat;

import org.chromium.base.IntentUtils;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.browser_ui.settings.CustomDividerFragment;
import org.chromium.components.browser_ui.settings.ExpandablePreferenceGroup;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.browser_ui.settings.TextMessagePreference;
import org.chromium.components.browser_ui.site_settings.AddExceptionPreference;
import org.chromium.components.browser_ui.site_settings.AddExceptionPreference.SiteAddedCallback;
import org.chromium.components.browser_ui.site_settings.SiteSettingsCategory;
import org.chromium.components.browser_ui.site_settings.Website;
import org.chromium.components.browser_ui.site_settings.WebsitePermissionsFetcher;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridge;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.content_settings.ContentSettingValues;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.ui.text.NoUnderlineClickableSpan;
import org.chromium.ui.text.SpanApplier;
import org.chromium.ui.widget.Toast;

import java.util.ArrayList;
import java.util.Collection;
import java.util.List;
import java.util.Locale;

/** Fragment to manage settings for tracking protection. */
public class TrackingProtectionSettings extends PreferenceFragmentCompat
        implements CustomDividerFragment, OnPreferenceClickListener, SiteAddedCallback {

    /**
     * Functional interface to start a Chrome Custom Tab for the given intent, e.g. by using {@link
     * org.chromium.chrome.browser.LaunchIntentDispatcher#createCustomTabActivityIntent}.
     * TODO(crbug.com/1181700): Update when LaunchIntentDispatcher is (partially-)modularized.
     */
    public interface CustomTabIntentHelper {
        /**
         * @see org.chromium.chrome.browser.LaunchIntentDispatcher#createCustomTabActivityIntent
         */
        Intent createCustomTabActivityIntent(Context context, Intent intent);
    }

    // Must match keys in tracking_protection_preferences.xml.
    private static final String OFFBOARDING_NOTICE = "offboarding_notice";
    private static final String PREF_BLOCK_ALL_TOGGLE = "block_all_3pcd_toggle";
    private static final String PREF_DNT_TOGGLE = "dnt_toggle";
    private static final String PREF_BULLET_TWO = "bullet_point_two";
    private static final String ALLOWED_GROUP = "allowed_group";
    public static final String ADD_EXCEPTION_KEY = "add_exception";

    public static final String LEARN_MORE_URL =
            "https://support.google.com/chrome/?p=tracking_protection";

    // The number of sites that are on the Allowed list.
    private int mAllowedSiteCount;

    // Whether the Allowed list should be shown expanded.
    private boolean mAllowListExpanded = true;

    private TrackingProtectionDelegate mDelegate;

    private CustomTabIntentHelper mCustomTabIntentHelper;

    @Override
    public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
        SettingsUtils.addPreferencesFromResource(this, R.xml.tracking_protection_preferences);
        getActivity().setTitle(R.string.privacy_sandbox_tracking_protection_title);

        // Format the Learn More link in the second bullet point.
        TextMessagePreference bulletTwo = (TextMessagePreference) findPreference(PREF_BULLET_TWO);
        bulletTwo.setSummary(SpanApplier.applySpans(
                getResources().getString(
                        R.string.privacy_sandbox_tracking_protection_bullet_two_description),
                new SpanApplier.SpanInfo("<link>", "</link>",
                        new NoUnderlineClickableSpan(getContext(), this::onLearnMoreClicked))));

        ChromeSwitchPreference blockAll3PCookiesSwitch =
                (ChromeSwitchPreference) findPreference(PREF_BLOCK_ALL_TOGGLE);
        ChromeSwitchPreference doNotTrackSwitch =
                (ChromeSwitchPreference) findPreference(PREF_DNT_TOGGLE);

        // Block all 3PCD switch.
        blockAll3PCookiesSwitch.setChecked(mDelegate.isBlockAll3PCDEnabled());
        blockAll3PCookiesSwitch.setOnPreferenceChangeListener((preference, newValue) -> {
            mDelegate.setBlockAll3PCD((boolean) newValue);
            return true;
        });

        // Do not track switch.
        doNotTrackSwitch.setChecked(mDelegate.isDoNotTrackEnabled());
        doNotTrackSwitch.setOnPreferenceChangeListener((preference, newValue) -> {
            mDelegate.setDoNotTrack((boolean) newValue);
            return true;
        });

        mAllowListExpanded = true;
        mAllowedSiteCount = 0;
        ExpandablePreferenceGroup allowedGroup =
                getPreferenceScreen().findPreference(ALLOWED_GROUP);
        allowedGroup.setOnPreferenceClickListener(this);
        refreshBlockingExceptions();

        // Add the exceptions button.
        SiteSettingsCategory cookiesCategory = SiteSettingsCategory.createFromType(
                mDelegate.getBrowserContext(), SiteSettingsCategory.Type.THIRD_PARTY_COOKIES);
        getPreferenceScreen().addPreference(new AddExceptionPreference(getContext(),
                ADD_EXCEPTION_KEY,
                getString(
                        R.string.website_settings_third_party_cookies_page_add_allow_exception_description),
                cookiesCategory, this));
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
        WebsitePreferenceBridge.setContentSettingCustomScope(mDelegate.getBrowserContext(),
                ContentSettingsType.COOKIES, primaryPattern, secondaryPattern,
                ContentSettingValues.ALLOW);

        String hostname = primaryPattern.equals(SITE_WILDCARD) ? secondaryPattern : primaryPattern;
        Toast.makeText(getContext(),
                     String.format(getContext().getString(R.string.website_settings_add_site_toast),
                             hostname),
                     Toast.LENGTH_SHORT)
                .show();

        refreshBlockingExceptions();
    }

    public void setTrackingProtectionDelegate(TrackingProtectionDelegate delegate) {
        mDelegate = delegate;
    }

    private void refreshBlockingExceptions() {
        SiteSettingsCategory cookiesCategory = SiteSettingsCategory.createFromType(
                mDelegate.getBrowserContext(), SiteSettingsCategory.Type.THIRD_PARTY_COOKIES);
        new WebsitePermissionsFetcher(mDelegate.getBrowserContext())
                .fetchPreferencesForCategory(cookiesCategory, this::onExceptionsFetched);
    }

    private void onExceptionsFetched(Collection<Website> sites) {
        List<WebsiteExceptionRowPreference> websites = new ArrayList<>();
        for (Website site : sites) {
            WebsiteExceptionRowPreference preference = new WebsiteExceptionRowPreference(
                    getContext(), site, mDelegate, this::refreshBlockingExceptions);
            websites.add(preference);
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
        SpannableStringBuilder spannable = new SpannableStringBuilder(
                getString(R.string.tracking_protection_allowed_group_title));
        String prefCount = String.format(Locale.getDefault(), " - %d", mAllowedSiteCount);
        spannable.append(prefCount);

        // Color the first part of the title blue.
        ForegroundColorSpan blueSpan = new ForegroundColorSpan(
                SemanticColorUtils.getDefaultTextColorAccent1(getContext()));
        spannable.setSpan(blueSpan, 0, spannable.length() - prefCount.length(),
                Spannable.SPAN_EXCLUSIVE_EXCLUSIVE);

        // Gray out the total count of items.
        final @ColorInt int gray = SemanticColorUtils.getDefaultTextColorSecondary(getContext());
        spannable.setSpan(new ForegroundColorSpan(gray), spannable.length() - prefCount.length(),
                spannable.length(), Spannable.SPAN_EXCLUSIVE_EXCLUSIVE);

        // Configure the preference group.
        allowedGroup.setTitle(spannable);
        allowedGroup.setExpanded(mAllowListExpanded);
    }

    private void onLearnMoreClicked(View view) {
        openUrlInCct(LEARN_MORE_URL);
    }

    public void setCustomTabIntentHelper(CustomTabIntentHelper helper) {
        mCustomTabIntentHelper = helper;
    }

    private void openUrlInCct(String url) {
        assert (mCustomTabIntentHelper != null)
                : "CCT helpers must be set on TrackingProtectionSettings before opening a link";
        CustomTabsIntent customTabIntent =
                new CustomTabsIntent.Builder().setShowTitle(true).build();
        customTabIntent.intent.setData(Uri.parse(url));
        Intent intent =
                mCustomTabIntentHelper.createCustomTabActivityIntent(
                        getContext(), customTabIntent.intent);
        intent.setPackage(getContext().getPackageName());
        intent.putExtra(Browser.EXTRA_APPLICATION_ID, getContext().getPackageName());
        IntentUtils.addTrustedIntentExtras(intent);
        IntentUtils.safeStartActivity(getContext(), intent);
    }
}
