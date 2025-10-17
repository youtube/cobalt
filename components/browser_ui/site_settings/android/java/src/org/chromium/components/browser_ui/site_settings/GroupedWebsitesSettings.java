// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.site_settings;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.app.Activity;
import android.app.Dialog;
import android.os.Bundle;
import android.view.View;
import android.widget.TextView;

import androidx.annotation.VisibleForTesting;
import androidx.appcompat.app.AlertDialog;
import androidx.preference.Preference;
import androidx.preference.PreferenceCategory;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.settings.ButtonPreference;
import org.chromium.components.browser_ui.settings.CustomDividerFragment;
import org.chromium.components.browser_ui.settings.EmbeddableSettingsPage;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.browser_ui.settings.TextMessagePreference;
import org.chromium.components.browsing_data.DeleteBrowsingDataAction;
import org.chromium.ui.text.ChromeClickableSpan;
import org.chromium.ui.text.SpanApplier;

/** Shows the permissions and other settings for a group of websites. */
@NullMarked
public class GroupedWebsitesSettings extends BaseSiteSettingsFragment
        implements EmbeddableSettingsPage,
                Preference.OnPreferenceClickListener,
                CustomDividerFragment {
    public static final String EXTRA_GROUP = "org.chromium.chrome.preferences.site_group";

    // Preference keys, see grouped_websites_preferences.xml.
    public static final String PREF_SITE_TITLE = "site_title";
    public static final String PREF_CLEAR_DATA = "clear_data";
    public static final String PREF_USAGE = "site_usage";
    public static final String PREF_RELATED_SITES = "related_sites";
    public static final String PREF_RELATED_SITES_CLEAR_DATA = "related_sites_delete_data_button";
    public static final String PREF_SITES_IN_GROUP = "sites_in_group";
    public static final String PREF_RESET_GROUP = "reset_group_button";

    public static final int RWS_ROW_ID = View.generateViewId();

    private static @Nullable GroupedWebsitesSettings sPausedInstance;

    private WebsiteGroup mSiteGroup;

    private @Nullable Dialog mConfirmationDialog;

    private final ObservableSupplierImpl<String> mPageTitle = new ObservableSupplierImpl<>();

    /**
     * Returns a paused instance of GroupedWebsitesSettings, if any.
     *
     * <p>This is used by {@link SingleWebsiteSettings} to go to the 'All Sites' level when clearing
     * data.
     */
    public static @Nullable GroupedWebsitesSettings getPausedInstance() {
        ThreadUtils.assertOnUiThread();
        return sPausedInstance;
    }

    @Override
    @Initializer
    public void onCreatePreferences(@Nullable Bundle savedInstanceState, @Nullable String rootKey) {
        // Remove this Preference if it gets restored without a valid SiteSettingsDelegate. This
        // can happen e.g. when it is included in PageInfo.
        if (!hasSiteSettingsDelegate()) {
            getParentFragmentManager().beginTransaction().remove(this).commit();
            return;
        }

        WebsiteGroup extraGroup = (WebsiteGroup) getArguments().getSerializable(EXTRA_GROUP);
        assert extraGroup != null : "EXTRA_GROUP must be provided.";
        mSiteGroup = extraGroup;
        var domainAndRegistry = extraGroup.getDomainAndRegistry();

        // Set title
        Activity activity = getActivity();
        mPageTitle.set(activity.getString(R.string.domain_settings_title, domainAndRegistry));

        // Preferences screen
        SettingsUtils.addPreferencesFromResource(this, R.xml.grouped_websites_preferences);
        Preference siteTitlePref = findPreference(PREF_SITE_TITLE);
        siteTitlePref.setTitle(domainAndRegistry);
        Preference siteInGroupPref = findPreference(PREF_SITES_IN_GROUP);
        siteInGroupPref.setTitle(
                activity.getString(R.string.domain_settings_sites_in_group, domainAndRegistry));
        setUpClearDataPreference();
        setUpResetGroupPreference();
        setUpRelatedSitesPreferences();
        updateSitesInGroup();
    }

    @Override
    public boolean hasDivider() {
        return false;
    }

    @Override
    public ObservableSupplier<String> getPageTitle() {
        return mPageTitle;
    }

    @Override
    public void onResume() {
        super.onResume();
        sPausedInstance = null;
    }

    @Override
    public void onPause() {
        super.onPause();
        sPausedInstance = this;
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        sPausedInstance = null;
    }

    @Override
    public boolean onPreferenceTreeClick(Preference preference) {
        if (preference instanceof WebsiteRowPreference) {
            ((WebsiteRowPreference) preference)
                    .handleClick(getArguments(), /* fromGrouped= */ true);
        }
        return super.onPreferenceTreeClick(preference);
    }

    @Override
    public boolean onPreferenceClick(Preference preference) {
        // Handle a click on the Clear & Reset button.
        View dialogView =
                getActivity().getLayoutInflater().inflate(R.layout.clear_reset_dialog, null);
        TextView mainMessage = dialogView.findViewById(R.id.main_message);
        mainMessage.setText(
                getString(
                        R.string.website_group_reset_confirmation,
                        mSiteGroup.getDomainAndRegistry()));
        TextView signedOutText = dialogView.findViewById(R.id.signed_out_text);
        signedOutText.setText(R.string.webstorage_clear_data_dialog_sign_out_group_message);
        TextView offlineText = dialogView.findViewById(R.id.offline_text);
        offlineText.setText(R.string.webstorage_delete_data_dialog_offline_message);
        mConfirmationDialog =
                new AlertDialog.Builder(getContext(), R.style.ThemeOverlay_BrowserUI_AlertDialog)
                        .setView(dialogView)
                        .setTitle(R.string.website_reset_confirmation_title)
                        .setPositiveButton(
                                R.string.website_reset,
                                (dialog, which) -> {
                                    resetGroup();
                                })
                        .setNegativeButton(
                                R.string.cancel, (dialog, which) -> mConfirmationDialog = null)
                        .show();
        return true;
    }

    @Override
    public void onDisplayPreferenceDialog(Preference preference) {
        if (preference instanceof ClearWebsiteStorage) {
            // If the activity is getting destroyed or saved, it is not allowed to modify fragments.
            if (assumeNonNull(getFragmentManager()).isStateSaved()) {
                return;
            }
            Callback<Boolean> onDialogClosed =
                    (Boolean confirmed) -> {
                        if (confirmed) {
                            RecordHistogram.recordEnumeratedHistogram(
                                    "Privacy.DeleteBrowsingData.Action",
                                    DeleteBrowsingDataAction.SITES_SETTINGS_PAGE,
                                    DeleteBrowsingDataAction.MAX_VALUE);

                            SiteDataCleaner.clearData(
                                    getSiteSettingsDelegate(), mSiteGroup, mDataClearedCallback);
                        }
                    };
            ClearWebsiteStorageDialog dialogFragment =
                    ClearWebsiteStorageDialog.newInstance(
                            preference, onDialogClosed, /* isGroup= */ true);
            dialogFragment.setTargetFragment(this, 0);
            dialogFragment.show(getFragmentManager(), ClearWebsiteStorageDialog.TAG);
        } else {
            super.onDisplayPreferenceDialog(preference);
        }
    }

    private final Runnable mDataClearedCallback =
            () -> {
                // TODO(crbug.com/40231223): This always navigates the user back to the "All sites"
                // page regardless of whether there are any non-resettable permissions left in the
                // sites within the group. Consider calculating those and refreshing the screen in
                // place for a slightly smoother user experience. However, due to the complexity
                // involved in refreshing the already fetched data and a very marginal benefit, it
                // may not be worth it.
                assumeNonNull(getSettingsNavigation()).finishCurrentSettings(this);
            };

    @VisibleForTesting
    public void resetGroup() {
        if (getActivity() == null) return;
        SiteDataCleaner.resetPermissions(
                getSiteSettingsDelegate().getBrowserContextHandle(), mSiteGroup);

        RecordHistogram.recordEnumeratedHistogram(
                "Privacy.DeleteBrowsingData.Action",
                DeleteBrowsingDataAction.SITES_SETTINGS_PAGE,
                DeleteBrowsingDataAction.MAX_VALUE);

        SiteDataCleaner.clearData(getSiteSettingsDelegate(), mSiteGroup, mDataClearedCallback);
    }

    private void setUpClearDataPreference() {
        ClearWebsiteStorage preference = findPreference(PREF_CLEAR_DATA);
        long storage = mSiteGroup.getTotalUsage();
        int cookies = mSiteGroup.getNumberOfCookies();
        if (storage > 0 || cookies > 0) {
            preference.setTitle(
                    SiteSettingsUtil.generateStorageUsageText(
                            preference.getContext(), storage, cookies));
            preference.setDataForDisplay(
                    mSiteGroup.getDomainAndRegistry(),
                    mSiteGroup.hasInstalledApp(
                            getSiteSettingsDelegate().getOriginsWithInstalledApp()),
                    /* isGroup= */ true);
            if (mSiteGroup.isCookieDeletionDisabled(
                    getSiteSettingsDelegate().getBrowserContextHandle())) {
                preference.setEnabled(false);
            }
        } else {
            getPreferenceScreen().removePreference(preference);
        }
    }

    private void setUpResetGroupPreference() {
        Preference preference = findPreference(PREF_RESET_GROUP);
        if (mSiteGroup.isCookieDeletionDisabled(
                getSiteSettingsDelegate().getBrowserContextHandle())) {
            preference.setEnabled(false);
        }
        preference.setOnPreferenceClickListener(this);
    }

    @VisibleForTesting
    public void resetRwsData() {
        if (getActivity() == null) return;
        RwsCookieInfo rwsInfo = mSiteGroup.getRwsInfo();
        assumeNonNull(rwsInfo);
        WebsiteGroup group = new WebsiteGroup(rwsInfo.getOwner(), rwsInfo.getMembers());
        SiteDataCleaner.clearData(getSiteSettingsDelegate(), group, mDataClearedCallback);
        RecordHistogram.recordEnumeratedHistogram(
                "Privacy.DeleteBrowsingData.Action",
                DeleteBrowsingDataAction.RWS_DELETE_ALL_DATA,
                DeleteBrowsingDataAction.MAX_VALUE);
    }

    public boolean onDeleteRwsDataPreferenceClick(Preference preference) {
        View dialogView =
                getActivity().getLayoutInflater().inflate(R.layout.clear_reset_dialog, null);
        TextView mainMessage = dialogView.findViewById(R.id.main_message);
        RwsCookieInfo rwsInfo = mSiteGroup.getRwsInfo();
        assumeNonNull(rwsInfo);
        mainMessage.setText(
                getString(
                        R.string.site_settings_delete_rws_storage_confirmation_android,
                        rwsInfo.getOwner()));
        TextView signedOutText = dialogView.findViewById(R.id.signed_out_text);
        signedOutText.setText(R.string.site_settings_delete_rws_storage_sign_out);
        TextView offlineText = dialogView.findViewById(R.id.offline_text);
        offlineText.setText(R.string.webstorage_delete_data_dialog_offline_message);
        mConfirmationDialog =
                new AlertDialog.Builder(getContext(), R.style.ThemeOverlay_BrowserUI_AlertDialog)
                        .setView(dialogView)
                        .setTitle(R.string.site_settings_delete_rws_storage_dialog_title)
                        .setPositiveButton(
                                R.string.storage_delete_dialog_clear_storage_option,
                                (dialog, which) -> {
                                    resetRwsData();
                                })
                        .setNegativeButton(
                                R.string.cancel, (dialog, which) -> mConfirmationDialog = null)
                        .show();
        return true;
    }

    private boolean isCurrentSite(WebsiteEntry entry) {
        return mSiteGroup.getDomainAndRegistry().equals(entry.getDomainAndRegistry());
    }

    private void setUpRelatedSitesPreferences() {
        PreferenceCategory relatedSitesSection = findPreference(PREF_RELATED_SITES);
        TextMessagePreference relatedSitesText = new TextMessagePreference(getContext(), null);
        var rwsInfo = mSiteGroup.getRwsInfo();
        boolean shouldRelatedSitesPrefBeVisible =
                getSiteSettingsDelegate().isRelatedWebsiteSetsDataAccessEnabled()
                        && rwsInfo != null;
        relatedSitesText.setVisible(shouldRelatedSitesPrefBeVisible);
        relatedSitesSection.setVisible(shouldRelatedSitesPrefBeVisible);
        ButtonPreference relatedSitesClearDataButton =
                findPreference(PREF_RELATED_SITES_CLEAR_DATA);
        relatedSitesClearDataButton
                .setVisible(
                        shouldRelatedSitesPrefBeVisible
                                && getSiteSettingsDelegate().shouldShowPrivacySandboxRwsUi());

        if (shouldRelatedSitesPrefBeVisible) {
            assumeNonNull(rwsInfo);
            relatedSitesText.setManagedPreferenceDelegate(
                    new ForwardingManagedPreferenceDelegate(
                            getSiteSettingsDelegate().getManagedPreferenceDelegate()) {
                        @Override
                        public boolean isPreferenceControlledByPolicy(Preference preference) {
                            for (var site : mSiteGroup.getWebsites()) {
                                if (getSiteSettingsDelegate()
                                        .isPartOfManagedRelatedWebsiteSet(
                                                site.getAddress().getOrigin())) {
                                    return true;
                                }
                            }
                            return false;
                        }
                    });

            if (getSiteSettingsDelegate().shouldShowPrivacySandboxRwsUi()) {
                relatedSitesText.setSummary(
                        SpanApplier.applySpans(
                                getContext()
                                        .getString(R.string.site_settings_rws_description_android),
                                new SpanApplier.SpanInfo(
                                        "<link>",
                                        "</link>",
                                        new ChromeClickableSpan(
                                                getContext(),
                                                (unused) -> {
                                                    getSiteSettingsDelegate()
                                                            .launchUrlInCustomTab(
                                                                    getActivity(),
                                                                    WebsiteSettingsConstants
                                                                            .RWS_LEARN_MORE_URL);
                                                }))));
                relatedSitesSection.addPreference(relatedSitesText);
                for (WebsiteEntry entry : rwsInfo.getMembersGroupedByDomain()) {
                    WebsiteRowPreference preference =
                            new WebsiteRowPreference(
                                    relatedSitesSection.getContext(),
                                    getSiteSettingsDelegate(),
                                    entry,
                                    getActivity().getLayoutInflater(),
                                    /* showRwsMembershipLabels= */ false,
                                    /* isClickable= */ false);
                    preference.setViewId(RWS_ROW_ID);
                    // If the row is for the current site, deleting the data will bounce back to the
                    // previous page to refresh
                    preference.setOnDeleteCallback(
                            isCurrentSite(entry)
                                    // If deleting data for the current site, pop back to refresh
                                    ? mDataClearedCallback
                                    : () -> {
                                        relatedSitesSection.removePreference(preference);
                                    });
                    relatedSitesSection.addPreference(preference);
                }
                relatedSitesClearDataButton.setOnPreferenceClickListener(
                        new Preference.OnPreferenceClickListener() {
                            @Override
                            public boolean onPreferenceClick(Preference preference) {
                                return onDeleteRwsDataPreferenceClick(preference);
                            }
                        });
            } else {
                relatedSitesText.setTitle(
                        getContext()
                                .getResources()
                                .getQuantityString(
                                        R.plurals.allsites_rws_summary,
                                        rwsInfo.getMembersCount(),
                                        Integer.toString(rwsInfo.getMembersCount()),
                                        rwsInfo.getOwner()));
                relatedSitesSection.addPreference(relatedSitesText);
            }
        }
    }

    private void updateSitesInGroup() {
        PreferenceCategory category = findPreference(PREF_SITES_IN_GROUP);
        category.removeAll();
        for (Website site : mSiteGroup.getWebsites()) {
            WebsiteRowPreference preference =
                    new WebsiteRowPreference(
                            category.getContext(),
                            getSiteSettingsDelegate(),
                            site,
                            getActivity().getLayoutInflater(),
                            /* showRwsMembershipLabels= */ false,
                            /* isClickable= */ true);
            preference.setOnDeleteCallback(
                    () -> {
                        category.removePreference(preference);
                    });
            category.addPreference(preference);
        }
    }

    @Override
    public @AnimationType int getAnimationType() {
        return AnimationType.PROPERTY;
    }
}
