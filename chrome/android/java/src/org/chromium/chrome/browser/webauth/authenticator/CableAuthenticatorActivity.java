// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webauth.authenticator;

import android.content.Intent;
import android.hardware.usb.UsbAccessory;
import android.hardware.usb.UsbManager;
import android.os.Bundle;
import android.util.Base64;

import androidx.fragment.app.Fragment;

import org.chromium.base.Log;
import org.chromium.chrome.browser.ChromeBaseAppCompatActivity;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.webauthn.CableAuthenticatorModuleProvider;

/**
 * Phone as a Security Key activity.
 *
 * This activity lives in the main APK and is the target for:
 *   1. Notifications triggered by cloud messages telling us that an authentication
 *      is pending.
 *   2. Intents from Play Services when a FIDO QR code has been scanned.
 *   3. Intents from Play Services when accounts.google.com is doing a security key operation.
 *
 * It hosts the {@link Fragment} that drives the security key process, which
 * pulls in the dynamic feature module containing the needed code.

 * Note: it does *not* handle USB intents when a computer is connected via USB
 * cable. See {@link CableAuthenticatorUSBActivity}.
 */
public class CableAuthenticatorActivity extends ChromeBaseAppCompatActivity {
    private static final String TAG = "CableAuthenticatorActivity";
    static final String EXTRA_SHOW_FRAGMENT_ARGUMENTS = "show_fragment_args";
    // See https://developer.android.com/guide/topics/connectivity/usb/accessory#java
    static final String USB_ACCESSORY_ATTACHED =
            "android.hardware.usb.action.USB_ACCESSORY_ATTACHED";
    static final String SERVER_LINK_EXTRA =
            "org.chromium.chrome.browser.webauth.authenticator.ServerLink";
    static final String QR_EXTRA = "org.chromium.chrome.browser.webauth.authenticator.QR";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        setTitle(getResources().getString(org.chromium.chrome.R.string.cablev2_paask_title));

        // Ensure that the full browser is running since this activity may be
        // triggered by a USB message.
        ChromeBrowserInitializer.getInstance().handleSynchronousStartup();

        super.onCreate(savedInstanceState);

        onNewIntent(getIntent());
    }

    @Override
    protected void onNewIntent(Intent intent) {
        super.onNewIntent(intent);

        Bundle arguments;
        if (intent.getAction() != null && intent.getAction().equals(USB_ACCESSORY_ATTACHED)) {
            // This can be triggered by an implicit intent if a desktop
            // Chrome is connected via USB. This is used to expose the
            // phone's security key to the desktop.
            UsbAccessory accessory =
                    (UsbAccessory) intent.getParcelableExtra(UsbManager.EXTRA_ACCESSORY);

            // The specific extra of interest is filtered out rather than
            // passing intent's whole Bundle because the external Intent
            // is untrusted.
            arguments = new Bundle();
            arguments.putParcelable(UsbManager.EXTRA_ACCESSORY, accessory);
        } else if (intent.getAction() != null && intent.getAction().equals(Intent.ACTION_VIEW)
                && intent.getData() != null) {
            // This is from Play Services and contains a FIDO URL scanned from a
            // QR code.
            arguments = new Bundle();
            arguments.putParcelable(QR_EXTRA, intent.getData());
        } else if (intent.hasExtra(SERVER_LINK_EXTRA)) {
            // This Intent comes from GMSCore when it's triggering a server-linked connection.
            final String serverLinkBase64 = intent.getStringExtra(SERVER_LINK_EXTRA);
            arguments = new Bundle();
            try {
                final byte[] serverLink = Base64.decode(serverLinkBase64, Base64.DEFAULT);
                arguments.putByteArray(SERVER_LINK_EXTRA, serverLink);
            } catch (IllegalArgumentException e) {
                Log.i(TAG, "Invalid base64 in ServerLink argument");
                return;
            }
        } else {
            // Since this Activity is not otherwise exported, this only happens when a notification
            // is tapped and |EXTRA_SHOW_FRAGMENT_ARGUMENTS| thus comes from our own PendingIntent.
            arguments = intent.getBundleExtra(EXTRA_SHOW_FRAGMENT_ARGUMENTS);
        }

        Fragment fragment = new CableAuthenticatorModuleProvider();
        fragment.setArguments(arguments);

        getSupportFragmentManager()
                .beginTransaction()
                .replace(android.R.id.content, fragment)
                .commit();
    }
}
