// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments.browser_binding;

import android.os.Build;
import android.os.Build.VERSION_CODES;
import android.security.keystore.KeyGenParameterSpec;
import android.security.keystore.KeyProperties;
import android.security.keystore.StrongBoxUnavailableException;
import android.util.Base64;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;

import org.chromium.base.Log;
import org.chromium.blink.mojom.PublicKeyCredentialParameters;
import org.chromium.blink.mojom.PublicKeyCredentialType;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.io.IOException;
import java.security.InvalidAlgorithmParameterException;
import java.security.KeyPair;
import java.security.KeyPairGenerator;
import java.security.KeyStore;
import java.security.KeyStoreException;
import java.security.NoSuchAlgorithmException;
import java.security.NoSuchProviderException;
import java.security.UnrecoverableEntryException;
import java.security.cert.CertificateException;
import java.util.ArrayList;
import java.util.List;

/**
 * BrowserBoundKeyStore creates and stores browser bound keys for matching external credentials.
 *
 * <p>On Android, we rely on the underlying AndroidKeyStore to create and store browser bound keys.
 * Note that AndroidKeyStore keys are deleted when the Android App (e.g. Chrome) is uninstalled. The
 * keys will be created in StrongBox on the device secure element.
 */
@JNINamespace("payments")
@NullMarked
public final class BrowserBoundKeyStore {

    /** The logging tag for this class. */
    private static final String TAG = "SpcBbKeyStore";

    public static final String ANDROID_KEY_STORE = "AndroidKeyStore";
    public static final String KEYSTORE_ALIAS_PREFIX = "spcbbk_sha256ecdsa_";

    private BrowserBoundKeyStore() {}

    /** Provides an instance of BrowserBoundKeyStore. */
    @CalledByNative
    public static BrowserBoundKeyStore getInstance() {
        return new BrowserBoundKeyStore();
    }

    /**
     * Creates a list of PublicKeyCredentialParameters from types and algorithms.
     *
     * @param types The types as {@link org.chromium.blink.mojom.PublicKeyCrednetialType} constants.
     * @param algorithms The algorithm as identified by COSE Algorithm identifiers. See <a
     *     href="https://www.iana.org/assignments/cose/cose.xhtml#algorithms">COSE Algorithms</a>.
     *     <p>This function aids in JNI conversions.
     */
    @CalledByNative
    public static List<PublicKeyCredentialParameters> createListOfCredentialParameters(
            @JniType("std::vector<int32_t>") int[] types,
            @JniType("std::vector<int32_t>") int[] algorithms) {
        assert types.length == algorithms.length;
        ArrayList<PublicKeyCredentialParameters> list = new ArrayList<>(types.length);
        for (int i = 0; i < types.length; i++) {
            PublicKeyCredentialParameters params = new PublicKeyCredentialParameters();
            params.type = types[i];
            params.algorithmIdentifier = algorithms[i];
            list.add(params);
        }
        return list;
    }

    /**
     * Get the corresponding browser bound key (or creates it).
     *
     * @param identifier An identifier for the corresponding passkey credential id.
     * @param allowedAlgorithms A list of allowed credential parameters.
     * @return The BrowserBoundKey object or null when the key pair could not be created.
     */
    @CalledByNative
    public @Nullable BrowserBoundKey getOrCreateBrowserBoundKeyForCredentialId(
            @JniType("std::vector<uint8_t>") byte[] identifier,
            List<PublicKeyCredentialParameters> allowedAlgorithms) {
        // TODO(crbug.com/377278827): Generate a random alias and store the association in a table,
        // so that browser bound public keys can be included in clientDataJson on passkey creation
        // time when the identifier is not know.
        if (!containsEs256(allowedAlgorithms)) {
            return null;
        }
        String keyStoreAlias =
                KEYSTORE_ALIAS_PREFIX + Base64.encodeToString(identifier, Base64.URL_SAFE);
        BrowserBoundKey browserBoundKey = getBrowserBoundKey(keyStoreAlias);
        if (browserBoundKey == null) {
            browserBoundKey = createBrowserBoundKey(keyStoreAlias);
        }
        return browserBoundKey;
    }

    private boolean containsEs256(List<PublicKeyCredentialParameters> allowedAlgorithms) {
        for (PublicKeyCredentialParameters params : allowedAlgorithms) {
            if (params.type == PublicKeyCredentialType.PUBLIC_KEY
                    && params.algorithmIdentifier == BrowserBoundKey.COSE_ALGORITHM_ES256) {
                return true;
            }
        }
        return false;
    }

    private @Nullable BrowserBoundKey getBrowserBoundKey(String keyStoreAlias) {
        try {
            KeyStore keyStore = KeyStore.getInstance(ANDROID_KEY_STORE);
            keyStore.load(null);
            if (!keyStore.containsAlias(keyStoreAlias)) {
                return null;
            }
            KeyStore.PrivateKeyEntry privateKeyEntry =
                    (KeyStore.PrivateKeyEntry)
                            keyStore.getEntry(keyStoreAlias, /* protParam= */ null);
            return new BrowserBoundKey(
                    new KeyPair(
                            privateKeyEntry.getCertificate().getPublicKey(),
                            privateKeyEntry.getPrivateKey()));
        } catch (UnrecoverableEntryException e) {
            // Recreate the key pair by returning null here.
            return null;
        } catch (KeyStoreException
                | CertificateException
                | IOException
                | NoSuchAlgorithmException e) {
            // TODO(crbug.com/377278827): Return a reason for the failure instead of recreating the
            // key.
            Log.e(TAG, "Could not load the browser bound key from the key store.");
            return null;
        }
    }

    private @Nullable BrowserBoundKey createBrowserBoundKey(String keyStoreAlias) {
        KeyPairGenerator generator = getAndroidKeyPairGenerator();
        if (generator == null) {
            return null;
        }
        KeyGenParameterSpec.Builder specBuilder;
        if (Build.VERSION.SDK_INT < VERSION_CODES.P) {
            // TODO(crbug.com/377278827): Possibly support keys not in the SecureElement: For
            // example by creating a key in the Trusted Execution Environment, and attesting the
            // key's storage.
            return null;
        }
        try {
            specBuilder =
                    new KeyGenParameterSpec.Builder(keyStoreAlias, KeyProperties.PURPOSE_SIGN)
                            .setDigests(KeyProperties.DIGEST_SHA256)
                            .setIsStrongBoxBacked(
                                    true); // StrongBox requires Android 9 (VERSION_CODES.P).
            try {
                generator.initialize(specBuilder.build());
            } catch (InvalidAlgorithmParameterException e) {
                // TODO(crbug.com/377278827): Return a reason for the failure.
                Log.e(
                        TAG,
                        "Could not initialize key pair generation for browser bound key support.",
                        e);
                return null;
            }
            return new BrowserBoundKey(generator.generateKeyPair());
        } catch (StrongBoxUnavailableException e) {
            Log.e(TAG, "StrongBox is not available while creating a browser bound key.");
            return null;
        }
    }

    private static @Nullable KeyPairGenerator getAndroidKeyPairGenerator() {
        try {
            return KeyPairGenerator.getInstance(KeyProperties.KEY_ALGORITHM_EC, ANDROID_KEY_STORE);
        } catch (NoSuchAlgorithmException | NoSuchProviderException e) {
            // TODO(crbug.com/377278827): Return a reason for the failure.
            Log.e(TAG, "Could not create key pair generation for browser bound key support.", e);
            return null;
        }
    }
}
