// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import android.content.ActivityNotFoundException;
import android.content.Context;
import android.content.Intent;
import android.util.Log;

import org.chromium.base.CalledByNative;
import org.chromium.base.CalledByNativeUnchecked;

import java.net.NetworkInterface;
import java.net.SocketException;
import java.net.URLConnection;
import java.security.KeyStoreException;
import java.security.NoSuchAlgorithmException;
import java.security.cert.CertificateException;
import java.util.Enumeration;

/**
 * This class implements net utilities required by the net component.
 */
class AndroidNetworkLibrary {

    private static final String TAG = AndroidNetworkLibrary.class.getName();

    /**
     * Stores the key pair into the CertInstaller application.
     */
    @CalledByNative
    static public boolean storeKeyPair(Context context, byte[] public_key, byte[] private_key) {
        // This is based on android.security.Credentials.install()
        // TODO(joth): Use KeyChain API instead of hard-coding constants here:
        // http://crbug.com/124660
        try {
            Intent intent = new Intent("android.credentials.INSTALL");
            intent.setClassName("com.android.certinstaller",
                    "com.android.certinstaller.CertInstallerMain");
            intent.putExtra("KEY", private_key);
            intent.putExtra("PKEY", public_key);
            intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            context.startActivity(intent);
            return true;
        } catch (ActivityNotFoundException e) {
            Log.w(TAG, "could not store certificate: " + e);
        }
        return false;
    }

    /**
     * @return the mime type (if any) that is associated with the file
     *         extension. Returns null if no corresponding mime type exists.
     */
    @CalledByNative
    static public String getMimeTypeFromExtension(String extension) {
        return URLConnection.guessContentTypeFromName("foo." + extension);
    }

    /**
     * @return true if it can determine that only loopback addresses are
     *         configured. i.e. if only 127.0.0.1 and ::1 are routable. Also
     *         returns false if it cannot determine this.
     */
    @CalledByNative
    static public boolean haveOnlyLoopbackAddresses() {
        Enumeration<NetworkInterface> list = null;
        try {
            list = NetworkInterface.getNetworkInterfaces();
            if (list == null) return false;
        } catch (SocketException e) {
            Log.w(TAG, "could not get network interfaces: " + e);
            return false;
        }

        while (list.hasMoreElements()) {
            NetworkInterface netIf = list.nextElement();
            try {
                if (netIf.isUp() && !netIf.isLoopback()) return false;
            } catch (SocketException e) {
                continue;
            }
        }
        return true;
    }

    /**
     * Validate the server's certificate chain is trusted.
     *
     * @param certChain The ASN.1 DER encoded bytes for certificates.
     * @param authType The key exchange algorithm name (e.g. RSA)
     * @return true if the server is trusted
     * @throws CertificateException,KeyStoreException,NoSuchAlgorithmException
     *             on error initializing the TrustManager or reading the
     *             certChain
     */
    @CalledByNativeUnchecked
    public static boolean verifyServerCertificates(byte[][] certChain, String authType)
            throws CertificateException, KeyStoreException, NoSuchAlgorithmException {
        return X509Util.verifyServerCertificates(certChain, authType);
    }

}
