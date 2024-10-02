// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webauthn;

import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewGroup;

import androidx.fragment.app.Fragment;

import org.chromium.base.annotations.NativeMethods;
import org.chromium.ui.widget.Toast;

/**
 * Shows a fragment for revoking linked caBLEv2 devices.
 *
 * This is reached by selecting Settings -> Privacy and security -> Phone as a
 * Security Key. It shows some explanatory text and has a single button to revoke all
 * linked devices.
 */
public class PrivacySettingsFragment extends Fragment implements OnClickListener {
    @Override
    public View onCreateView(
            LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        getActivity().setTitle(R.string.cablev2_paask_title);

        View v = inflater.inflate(R.layout.cablev2_settings, container, false);
        v.findViewById(R.id.unlink_button).setOnClickListener(this);
        return v;
    }

    @Override
    public void onClick(View v) {
        // The "revoke all" button was tapped.
        PrivacySettingsFragmentJni.get().revokeAllLinkedDevices();
        Toast.makeText(getActivity(),
                     getResources().getString(R.string.cablev2_unlink_confirmation),
                     Toast.LENGTH_SHORT)
                .show();
    }

    @NativeMethods
    interface Natives {
        void revokeAllLinkedDevices();
    }
}
