// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safe_browsing.settings;

import android.os.Bundle;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;

import androidx.annotation.NonNull;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.safe_browsing.SafeBrowsingBridge;
import org.chromium.chrome.browser.settings.ChromeBaseSettingsFragment;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.browser_ui.util.TraceEventVectorDrawableCompat;

/** The base fragment class for Safe Browsing settings fragments. */
public abstract class SafeBrowsingSettingsFragmentBase extends ChromeBaseSettingsFragment {
    private SafeBrowsingBridge mSafeBrowsingBridge;
    private final ObservableSupplierImpl<String> mPageTitle = new ObservableSupplierImpl<>();

    @Override
    public void onCreatePreferences(Bundle bundle, String s) {
        SettingsUtils.addPreferencesFromResource(this, getPreferenceResource());
        mPageTitle.set(getString(R.string.prefs_section_safe_browsing_title));

        onCreatePreferencesInternal(bundle, s);

        setHasOptionsMenu(true);
    }

    @Override
    public ObservableSupplier<String> getPageTitle() {
        return mPageTitle;
    }

    @Override
    public void setProfile(@NonNull Profile profile) {
        super.setProfile(profile);
        mSafeBrowsingBridge = new SafeBrowsingBridge(profile);
    }

    /**
     * Return the {@link SafeBrowsingBridge} associated with the Profile set in {@link
     * #setProfile(Profile)}.
     */
    protected SafeBrowsingBridge getSafeBrowsingBridge() {
        return mSafeBrowsingBridge;
    }

    @Override
    public void onCreateOptionsMenu(Menu menu, MenuInflater inflater) {
        menu.clear();
        MenuItem help =
                menu.add(Menu.NONE, R.id.menu_id_targeted_help, Menu.NONE, R.string.menu_help);
        help.setIcon(
                TraceEventVectorDrawableCompat.create(
                        getResources(), R.drawable.ic_help_and_feedback, getActivity().getTheme()));
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        if (item.getItemId() != R.id.menu_id_targeted_help) {
            return false;
        }
        getHelpAndFeedbackLauncher()
                .show(getActivity(), getString(R.string.help_context_safe_browsing), null);
        return true;
    }

    /**
     * Called within {@link SafeBrowsingSettingsFragmentBase#onCreatePreferences(Bundle, String)}.
     * If the child class needs to handle specific logic during preference creation, it can override
     * this method.
     */
    protected void onCreatePreferencesInternal(Bundle bundle, String s) {}

    /**
     * @return The resource id of the preference.
     */
    protected abstract int getPreferenceResource();
}
