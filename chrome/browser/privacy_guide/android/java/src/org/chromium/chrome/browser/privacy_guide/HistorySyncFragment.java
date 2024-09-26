// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_guide;

import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.CompoundButton;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.appcompat.widget.SwitchCompat;
import androidx.fragment.app.Fragment;

import org.chromium.chrome.browser.sync.SyncService;
import org.chromium.components.sync.UserSelectableType;

import java.util.Set;

/**
 * Controls the behaviour of the History Sync privacy guide page.
 */
public class HistorySyncFragment
        extends Fragment implements CompoundButton.OnCheckedChangeListener {
    private SyncService mSyncService;
    private boolean mInitialKeepEverythingSynced;

    @Override
    public View onCreateView(
            LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        return inflater.inflate(R.layout.privacy_guide_history_sync_step, container, false);
    }

    @Override
    public void onViewCreated(@NonNull View view, @Nullable Bundle savedInstanceState) {
        mSyncService = SyncService.get();
        mInitialKeepEverythingSynced = mSyncService.hasKeepEverythingSynced();

        SwitchCompat historySyncSwitch = view.findViewById(R.id.history_sync_switch);
        historySyncSwitch.setChecked(PrivacyGuideUtils.isHistorySyncEnabled());

        historySyncSwitch.setOnCheckedChangeListener(this);
    }

    @Override
    public void onCheckedChanged(CompoundButton buttonView, boolean isChecked) {
        PrivacyGuideMetricsDelegate.recordMetricsOnHistorySyncChange(isChecked);

        boolean keepEverythingSynced = isChecked && mInitialKeepEverythingSynced;

        Set<Integer> syncTypes = mSyncService.getSelectedTypes();
        if (isChecked) {
            syncTypes.add(UserSelectableType.HISTORY);
        } else {
            syncTypes.remove(UserSelectableType.HISTORY);
        }

        mSyncService.setSelectedTypes(keepEverythingSynced, syncTypes);
    }
}
