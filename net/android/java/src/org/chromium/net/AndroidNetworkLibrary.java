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

import java.io.ByteArrayInputStream;
import java.net.NetworkInterface;
import java.net.SocketException;
import java.net.URLConnection;
import java.security.KeyStore;
import java.security.KeyStoreException;
import java.security.NoSuchAlgorithmException;
import java.security.cert.CertificateException;
import java.security.cert.CertificateFactory;
import java.security.cert.X509Certificate;
import java.util.concurrent.atomic.AtomicReference;
import java.util.Enumeration;

import javax.net.ssl.TrustManager;
import javax.net.ssl.TrustManagerFactory;
import javax.net.ssl.X509TrustManager;

// This class implements net utilities required by the net component.
class AndroidNetworkLibrary {
    private static final String TAG = "AndroidNetworkLibrary";

    // Stores the key pair into the CertInstaller application.
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

    // Get the mime type (if any) that is associated with the file extension.
    // Returns null if no corresponding mime type exists.
    @CalledByNative
    static public String getMimeTypeFromExtension(String extension) {
        return URLConnection.guessContentTypeFromName("foo." + extension);
    }

    // Returns true if it can determine that only loopback addresses are configured.
    // i.e. if only 127.0.0.1 and ::1 are routable.
    // Also returns false if it cannot determine this.
    @CalledByNative
    static public boolean haveOnlyLoopbackAddresses() {
        boolean result = true;
        try {
            Enumeration list = NetworkInterface.getNetworkInterfaces();
            if (list == null) return false;

            while (list.hasMoreElements()) {
                NetworkInterface netIf = (NetworkInterface)list.nextElement();
                try {
                    if (!netIf.isUp() || netIf.isLoopback())
                        continue;
                    result = false;
                    break;
                } catch (SocketException e) {
                    continue;
                }
            }
        } catch (SocketException e) {
            Log.w(TAG, "could not get network interfaces: " + e);
            return false;
        }
        return result;
    }

    /**
     * Validate the server's certificate chain is trusted.
     * @param certChain The ASN.1 DER encoded bytes for certificates.
     * @param authType The key exchange algorithm name (e.g. RSA)
     * @return true if the server is trusted
     * @throws CertificateException,KeyStoreException,NoSuchAlgorithmException on error
     * initializing the TrustManager or reading the certChain
     */
    @CalledByNativeUnchecked
    public static boolean verifyServerCertificates(byte[][] certChain, String authType)
            throws CertificateException, KeyStoreException, NoSuchAlgorithmException {
        if (certChain == null || certChain.length == 0 || certChain[0] == null) {
            throw new IllegalArgumentException("Expected non-null and non-empty certificate " +
                                               "chain passed as |certChain|. |certChain|=" +
                                               certChain);
        }

        ensureInitialized();
        X509Certificate[] serverCertificates = new X509Certificate[certChain.length];
        for (int i = 0; i < certChain.length; ++i) {
            serverCertificates[i] =
                (X509Certificate) sCertificateFactory.get().generateCertificate(
                        new ByteArrayInputStream(certChain[i]));
        }

        try {
            sDefaultTrustManager.get().checkServerTrusted(serverCertificates, authType);
            return true;
        } catch (CertificateException e) {
            Log.i(TAG, "failed to validate the certificate chain, error: " +
                    e.getMessage());
        }
        return false;
    }

    // Default sources of authentication trust decisions and certificate object creation.
    private static AtomicReference<X509TrustManager> sDefaultTrustManager =
        new AtomicReference<X509TrustManager>();
    private static AtomicReference<CertificateFactory> sCertificateFactory =
        new AtomicReference<CertificateFactory>();

    /**
     * Ensures that |sDefaultTrustManager| and |sCertificateFactory| are initialized.
     *
     * @throws CertificateException,KeyStoreException,NoSuchAlgorithmException on error initializing
     * the TrustManager.
     */
    private static void ensureInitialized()
            throws CertificateException, KeyStoreException, NoSuchAlgorithmException {
        // There could be a begin race creating two instances of these objects, which
        // is harmless save for a bit of wasted effort.
        if (sDefaultTrustManager.get() == null) {
            sDefaultTrustManager.compareAndSet(null, createDefaultTrustManager());
        }
        if (sCertificateFactory.get() == null) {
            sCertificateFactory.compareAndSet(null, CertificateFactory.getInstance("X.509"));
        }
    }

    /*
     * Creates a TrustManagerFactory and returns the X509TrustManager instance if one can be found.
     *
     * @throws CertificateException,KeyStoreException,NoSuchAlgorithmException on error initializing
     * the TrustManager.
     */
    private static X509TrustManager createDefaultTrustManager()
            throws KeyStoreException, NoSuchAlgorithmException {
        String algorithm = TrustManagerFactory.getDefaultAlgorithm();
        TrustManagerFactory tmf = TrustManagerFactory.getInstance(algorithm);
        tmf.init((KeyStore) null);
        TrustManager[] tms = tmf.getTrustManagers();
        X509TrustManager trustManager = findX509TrustManager(tms);
        return trustManager;
    }

    private static X509TrustManager findX509TrustManager(TrustManager[] tms) {
        for (TrustManager tm : tms) {
            if (tm instanceof X509TrustManager) {
                return (X509TrustManager)tm;
            }
        }
        return null;
    }
}
