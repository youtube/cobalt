// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browsing_data;

import android.os.Bundle;

import androidx.preference.Preference;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;

import java.util.Arrays;
import java.util.List;

/**
 * A more advanced version of {@link ClearBrowsingDataFragment} with more dialog options and less
 * explanatory text.
 */
public class ClearBrowsingDataFragmentAdvanced extends ClearBrowsingDataFragment {
    @Override
    public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
        super.onCreatePreferences(savedInstanceState, rootKey);
        // Remove the search history text preferences if they exist, since they should only appear
        // on the basic tab of Clear Browsing Data.
        Preference googleDataTextPref =
                findPreference(ClearBrowsingDataFragment.PREF_GOOGLE_DATA_TEXT);
        if (googleDataTextPref != null) {
            getPreferenceScreen().removePreference(googleDataTextPref);
        }
        Preference nonGoogleSearchHistoryTextPref =
                findPreference(ClearBrowsingDataFragment.PREF_SEARCH_HISTORY_NON_GOOGLE_TEXT);
        if (nonGoogleSearchHistoryTextPref != null) {
            getPreferenceScreen().removePreference(nonGoogleSearchHistoryTextPref);
        }
        // TODO(https://crbug.com/1334920): Change after follow up discussion with privacy team.
        Preference signOutOfChromeTextPref =
                findPreference(ClearBrowsingDataFragment.PREF_SIGN_OUT_OF_CHROME_TEXT);
    }

    @Override
    protected int getClearBrowsingDataTabType() {
        return ClearBrowsingDataTab.ADVANCED;
    }

    @Override
    protected List<Integer> getDialogOptions() {
        return Arrays.asList(DialogOption.CLEAR_HISTORY, DialogOption.CLEAR_COOKIES_AND_SITE_DATA,
                DialogOption.CLEAR_CACHE, DialogOption.CLEAR_PASSWORDS,
                DialogOption.CLEAR_FORM_DATA, DialogOption.CLEAR_SITE_SETTINGS);
    }

    @Override
    protected void onClearBrowsingData() {
        super.onClearBrowsingData();
        RecordHistogram.recordEnumeratedHistogram("History.ClearBrowsingData.UserDeletedFromTab",
                ClearBrowsingDataTab.ADVANCED, ClearBrowsingDataTab.NUM_TYPES);
        RecordUserAction.record("ClearBrowsingData_AdvancedTab");
    }
}
