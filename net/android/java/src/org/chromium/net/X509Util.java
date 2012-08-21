// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import android.util.Log;

import java.io.ByteArrayInputStream;
import java.security.KeyStore;
import java.security.KeyStoreException;
import java.security.NoSuchAlgorithmException;
import java.security.cert.CertificateException;
import java.security.cert.CertificateFactory;
import java.security.cert.X509Certificate;

import javax.net.ssl.TrustManager;
import javax.net.ssl.TrustManagerFactory;
import javax.net.ssl.X509TrustManager;

public class X509Util {

    private static final String TAG = X509Util.class.getName();

    private static CertificateFactory sCertificateFactory;

    /**
     * Default sources of authentication trust decisions and certificate object
     * creation.
     */
    private static X509TrustManager sDefaultTrustManager;

    /**
     * Ensures that |sCertificateFactory| and |sDefaultTrustManager| are
     * initialized.
     */
    private static synchronized void ensureInitialized() throws CertificateException,
            KeyStoreException, NoSuchAlgorithmException {
        if (sCertificateFactory == null) {
            sCertificateFactory = CertificateFactory.getInstance("X.509");
        }
        if (sDefaultTrustManager == null) {
            sDefaultTrustManager = X509Util.createDefaultTrustManager();
        }
    }

    /**
     * Creates a TrustManagerFactory and returns the X509TrustManager instance
     * if one can be found.
     *
     * @throws CertificateException,KeyStoreException,NoSuchAlgorithmException
     *             on error initializing the TrustManager.
     */
    private static X509TrustManager createDefaultTrustManager()
            throws KeyStoreException, NoSuchAlgorithmException {
        String algorithm = TrustManagerFactory.getDefaultAlgorithm();
        TrustManagerFactory tmf = TrustManagerFactory.getInstance(algorithm);
        tmf.init((KeyStore) null);

        for (TrustManager tm : tmf.getTrustManagers()) {
            if (tm instanceof X509TrustManager) {
                return (X509TrustManager) tm;
            }
        }
        return null;
    }

    /**
     * Convert a DER encoded certificate to an X509Certificate
     */
    public static X509Certificate createCertificateFromBytes(byte[] derBytes) throws
            CertificateException, KeyStoreException, NoSuchAlgorithmException {
        ensureInitialized();
        return (X509Certificate) sCertificateFactory.generateCertificate(
                new ByteArrayInputStream(derBytes));
    }

    public static boolean verifyServerCertificates(byte[][] certChain, String authType)
            throws CertificateException, KeyStoreException, NoSuchAlgorithmException {
        if (certChain == null || certChain.length == 0 || certChain[0] == null) {
            throw new IllegalArgumentException("Expected non-null and non-empty certificate " +
                    "chain passed as |certChain|. |certChain|=" + certChain);
        }

        ensureInitialized();
        X509Certificate[] serverCertificates = new X509Certificate[certChain.length];
        for (int i = 0; i < certChain.length; ++i) {
            serverCertificates[i] = createCertificateFromBytes(certChain[i]);
        }

        try {
            sDefaultTrustManager.checkServerTrusted(serverCertificates, authType);
            return true;
        } catch (CertificateException e) {
            Log.i(TAG, "failed to validate the certificate chain, error: " +
                    e.getMessage());
        }
        return false;
    }

}
