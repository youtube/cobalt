// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import android.util.Log;

import java.io.ByteArrayInputStream;
import java.io.IOException;
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
     * Trust manager backed up by the read-only system certificate store.
     */
    private static X509TrustManager sDefaultTrustManager;

    /**
     * Trust manager backed up by a custom certificate store. We need such manager to plant test
     * root CA to the trust store in testing.
     */
    private static X509TrustManager sTestTrustManager;
    private static KeyStore sTestKeyStore;

    /**
     * Lock object used to synchronize all calls that modify or depend on the trust managers.
     */
    private static final Object sLock = new Object();

    /**
     * Ensures that the trust managers and certificate factory are initialized.
     */
    private static void ensureInitialized() throws CertificateException,
            KeyStoreException, NoSuchAlgorithmException {
        synchronized(sLock) {
            if (sCertificateFactory == null) {
                sCertificateFactory = CertificateFactory.getInstance("X.509");
            }
            if (sDefaultTrustManager == null) {
                sDefaultTrustManager = X509Util.createTrustManager(null);
            }
            if (sTestKeyStore == null) {
                sTestKeyStore = KeyStore.getInstance(KeyStore.getDefaultType());
                try {
                    sTestKeyStore.load(null);
                } catch(IOException e) {}  // No IO operation is attempted.
            }
            if (sTestTrustManager == null) {
                sTestTrustManager = X509Util.createTrustManager(sTestKeyStore);
            }
        }
    }

    /**
     * Creates a X509TrustManager backed up by the given key store. When null is passed as a key
     * store, system default trust store is used.
     * @throws KeyStoreException, NoSuchAlgorithmException on error initializing the TrustManager.
     */
    private static X509TrustManager createTrustManager(KeyStore keyStore) throws KeyStoreException,
            NoSuchAlgorithmException {
        String algorithm = TrustManagerFactory.getDefaultAlgorithm();
        TrustManagerFactory tmf = TrustManagerFactory.getInstance(algorithm);
        tmf.init(keyStore);

        for (TrustManager tm : tmf.getTrustManagers()) {
            if (tm instanceof X509TrustManager) {
                return (X509TrustManager) tm;
            }
        }
        return null;
    }

    /**
     * After each modification of test key store, trust manager has to be generated again.
     */
    private static void reloadTestTrustManager() throws KeyStoreException,
            NoSuchAlgorithmException {
        sTestTrustManager = X509Util.createTrustManager(sTestKeyStore);
    }

    /**
     * Convert a DER encoded certificate to an X509Certificate.
     */
    public static X509Certificate createCertificateFromBytes(byte[] derBytes) throws
            CertificateException, KeyStoreException, NoSuchAlgorithmException {
        ensureInitialized();
        return (X509Certificate) sCertificateFactory.generateCertificate(
                new ByteArrayInputStream(derBytes));
    }

    public static void addTestRootCertificate(byte[] rootCertBytes) throws CertificateException,
            KeyStoreException, NoSuchAlgorithmException {
        ensureInitialized();
        X509Certificate rootCert = createCertificateFromBytes(rootCertBytes);
        synchronized(sLock) {
            sTestKeyStore.setCertificateEntry(
                    "root_cert_" + Integer.toString(sTestKeyStore.size()), rootCert);
            reloadTestTrustManager();
        }
    }

    public static void clearTestRootCertificates() throws NoSuchAlgorithmException,
            CertificateException, KeyStoreException {
        ensureInitialized();
        synchronized(sLock) {
            try {
                sTestKeyStore.load(null);
                reloadTestTrustManager();
            } catch(IOException e) {}  // No IO operation is attempted.
        }
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

        synchronized (sLock) {
            try {
                sDefaultTrustManager.checkServerTrusted(serverCertificates, authType);
                return true;
            } catch (CertificateException eDefaultManager) {
                try {
                    sTestTrustManager.checkServerTrusted(serverCertificates, authType);
                    return true;
                } catch (CertificateException eTestManager) {
                    /*
                     *  Neither of the trust managers confirms the validity of the certificate
                     *  chain, we emit the error message returned by the system trust manager.
                     */
                    Log.i(TAG, "failed to validate the certificate chain, error: " +
                               eDefaultManager.getMessage());
                }
            }
        }
        return false;
    }
}