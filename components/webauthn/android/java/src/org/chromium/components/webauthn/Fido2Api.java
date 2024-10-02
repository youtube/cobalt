// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webauthn;

import android.app.Activity;
import android.content.Intent;
import android.net.Uri;
import android.os.Parcel;
import android.util.Base64;
import android.util.Pair;

import androidx.annotation.Nullable;

import org.chromium.base.Log;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.blink.mojom.AttestationConveyancePreference;
import org.chromium.blink.mojom.AuthenticatorAttachment;
import org.chromium.blink.mojom.AuthenticatorTransport;
import org.chromium.blink.mojom.CommonCredentialInfo;
import org.chromium.blink.mojom.DevicePublicKeyResponse;
import org.chromium.blink.mojom.GetAssertionAuthenticatorResponse;
import org.chromium.blink.mojom.MakeCredentialAuthenticatorResponse;
import org.chromium.blink.mojom.PrfValues;
import org.chromium.blink.mojom.PublicKeyCredentialCreationOptions;
import org.chromium.blink.mojom.PublicKeyCredentialDescriptor;
import org.chromium.blink.mojom.PublicKeyCredentialParameters;
import org.chromium.blink.mojom.PublicKeyCredentialRequestOptions;
import org.chromium.blink.mojom.PublicKeyCredentialType;
import org.chromium.blink.mojom.ResidentKeyRequirement;
import org.chromium.blink.mojom.UserVerificationRequirement;
import org.chromium.blink.mojom.UvmEntry;
import org.chromium.mojo_base.mojom.TimeDelta;

import java.security.NoSuchAlgorithmException;
import java.util.ArrayList;
import java.util.concurrent.TimeUnit;

/**
 * Fido2Api contains functions for serialising/deserialising the structures that make up Play
 * Services' FIDO2 API.
 * <p>
 * These structures are made with the {@link Parcel} class. Parcel is a linear, binary format that
 * doesn't contain type information. I.e. it knows how to write strings, ints, byte[], etc, but the
 * reading side has to know what order the values come in rather than being able to generically
 * parse like, e.g., JSON. (Parcel can also write Bundles, which are a dict-like object, but that is
 * not used in this API.)
 * <p>
 * Building on top of {@link Parcel} is a format called SafeParcelable. It adds a tag-length-value
 * structure that allows for optional fields and future extensions. Tags and lengths are encoded in
 * one or two ints. The bottom 16 bits of the first int are the tag number. The top 16 bits are
 * either the length, or else the value 0xffff, which indicates that the length is contained in a
 * following int. The single-int format is never encoded by this code so that we never have to
 * shuffle data around in the case that it's longer than 0xfffe bytes.
 * <p>
 * The function {@link writeHeader} writes a tag and placeholder length, returning the offset of the
 * length that can be filled in with a later call to {@link writeLength}. Thus encoding is one-pass:
 * the code doesn't need to calculate lengths before starting to serialise.
 * <p>
 * Objects start with a tag and length where the tag is the special value {@link OBJECT_MAGIC}. Then
 * they contain a series of tag-length-value structures. The tag communicates the type of the value.
 * If the tag is unknown then the length permits it to be skipped over.
 * <p>
 * Since these structures come from Play Services, they should be trustworthy. However, things are a
 * little more dicey than one would hope. {@link Parcel} JNIs to native code and setting a position
 * is accepted without checking if it's in bounds[1]. (There's a check for values > 2^31, to catch
 * negative ints from Java, but that's not in all Android versions.) When reading, there's a bounds
 * check, not until after a potentially overflowing addition[2]. Therefore we are careful when
 * setting the data position and always add lengths parsed from the data using {@link
 * addLengthToParcelPosition}.
 * <p>
 * [1]
 * https://android.googlesource.com/platform/frameworks/native/+/3cf307284a620c67b9eb024439583fc1c42574ee/libs/binder/Parcel.cpp#370
 * [2]
 * https://android.googlesource.com/platform/frameworks/native/+/3cf307284a620c67b9eb024439583fc1c42574ee/libs/binder/Parcel.cpp#1545
 */
@JNINamespace("webauthn")
public final class Fido2Api {
    // Error codes returned by the API.
    public static final int SECURITY_ERR = 18;
    public static final int TIMEOUT_ERR = 23;
    public static final int ENCODING_ERR = 27;
    public static final int NOT_ALLOWED_ERR = 35;
    public static final int DATA_ERR = 30;
    public static final int NOT_SUPPORTED_ERR = 9;
    public static final int CONSTRAINT_ERR = 29;
    public static final int INVALID_STATE_ERR = 11;
    public static final int UNKNOWN_ERR = 28;

    // CREDENTIAL_EXTRA is the Intent key under which the serialised response is stored.
    public static final String CREDENTIAL_EXTRA = "FIDO2_CREDENTIAL_EXTRA";

    private static final double MIN_TIMEOUT_SECONDS = 10;
    private static final double MAX_TIMEOUT_SECONDS = 600;

    private static final String TAG = "Fido2Api";
    private static final int ECDSA_COSE_IDENTIFIER = -7;

    // OBJECT_MAGIC is a magic value used to indicate the start of an object when encoding with
    // Parcel.
    private static final int OBJECT_MAGIC = 20293;

    // VAL_PARCELABLE is the tag value for a Parcelable, as used by `Parcel.writeValue`.
    private static final int VAL_PARCELABLE = 4;

    // parcelUsesLengthPrefixes will be true if `Parcel.writeValue` uses length
    // prefixes. This was added in Android 13 and there's one case where an
    // array is sent directly as a Parcel, rather than as a SafeParcel. We
    // sadly need to care about this because the Parcel class supplied by the
    // system doesn't provide any way of reading arrays that isn't coupled to
    // ClassLoader-based assumptions.
    private static final boolean sParcelUsesLengthPrefixes = doesParcelUseLengthPrefix();

    private static boolean doesParcelUseLengthPrefix() {
        // See comment for `sParcelUsesLengthPrefixes`.
        Parcel parcel = Parcel.obtain();
        parcel.writeValue(new ArrayList());
        final boolean ret = parcel.dataPosition() == 12;
        parcel.recycle();
        return ret;
    }

    /**
     * Serialize a browser's makeCredential request to a {@link Parcel}.
     *
     * @param options the options passed from the renderer.
     * @param origin the origin that the request should act as.
     * @param clientDataHash (optional) override the ClientDataJSON generated by Play Services.
     * @param parcel the {@link Parcel} to append the output to.
     * @throws NoSuchAlgorithmException when options requests an impossible-to-satisfy public-key
     *         algorithm.
     */
    public static void appendBrowserMakeCredentialOptionsToParcel(
            PublicKeyCredentialCreationOptions options, Uri origin, @Nullable byte[] clientDataHash,
            Parcel parcel) throws NoSuchAlgorithmException {
        final int a = writeHeader(OBJECT_MAGIC, parcel);

        // 2: PublicKeyCredentialCreationOptions
        int z = writeHeader(2, parcel);
        appendMakeCredentialOptionsToParcel(options, parcel);
        writeLength(z, parcel);

        // 3: origin
        z = writeHeader(3, parcel);
        origin.writeToParcel(parcel, /* flags= */ 0);
        writeLength(z, parcel);

        // 4: clientDataHash
        if (clientDataHash != null) {
            z = writeHeader(4, parcel);
            parcel.writeByteArray(clientDataHash);
            writeLength(z, parcel);
        }

        writeLength(a, parcel);
    }

    /**
     * Serialize an app's makeCredential request to a {@link Parcel}.
     *
     * @param options the options passed from the renderer.
     * @param parcel the {@link Parcel} to append the output to.
     * @throws NoSuchAlgorithmException when options requests an impossible-to-satisfy public-key
     *         algorithm.
     */
    public static void appendMakeCredentialOptionsToParcel(
            PublicKeyCredentialCreationOptions options, Parcel parcel)
            throws NoSuchAlgorithmException {
        final int a = writeHeader(OBJECT_MAGIC, parcel);

        // 2: PublicKeyCredentialRpEntity

        int b = writeHeader(2, parcel);
        int c = writeHeader(OBJECT_MAGIC, parcel);

        int z = writeHeader(2, parcel);
        parcel.writeString(options.relyingParty.id);
        writeLength(z, parcel);

        z = writeHeader(3, parcel);
        parcel.writeString(options.relyingParty.name);
        writeLength(z, parcel);

        writeLength(c, parcel);
        writeLength(b, parcel);

        // 3: PublicKeyCredentialUserEntity

        b = writeHeader(3, parcel);
        c = writeHeader(OBJECT_MAGIC, parcel);

        z = writeHeader(2, parcel);
        parcel.writeByteArray(options.user.id);
        writeLength(z, parcel);

        z = writeHeader(3, parcel);
        parcel.writeString(options.user.name);
        writeLength(z, parcel);

        z = writeHeader(5, parcel);
        parcel.writeString(options.user.displayName);
        writeLength(z, parcel);

        writeLength(c, parcel);
        writeLength(b, parcel);

        // 4: challenge

        b = writeHeader(4, parcel);
        parcel.writeByteArray(options.challenge);
        writeLength(b, parcel);

        // 5: parameters

        b = writeHeader(5, parcel);

        boolean hasEcdsaP256 = false;
        // TODO(agl): we shouldn't filter here but GMS Core requires that no
        // unknown values be sent, defeating the point.
        for (PublicKeyCredentialParameters param : options.publicKeyParameters) {
            if (param.algorithmIdentifier == ECDSA_COSE_IDENTIFIER
                    && param.type == PublicKeyCredentialType.PUBLIC_KEY) {
                hasEcdsaP256 = true;
                break;
            }
        }

        if (!hasEcdsaP256 && options.publicKeyParameters.length != 0) {
            // The site only accepts algorithms that are not supported.
            throw new NoSuchAlgorithmException();
        }

        if (!hasEcdsaP256) {
            parcel.writeInt(0);
        } else {
            parcel.writeInt(1);
            c = startLength(parcel);
            int d = writeHeader(OBJECT_MAGIC, parcel);

            z = writeHeader(2, parcel);
            parcel.writeString(credentialTypeToString(PublicKeyCredentialType.PUBLIC_KEY));
            writeLength(z, parcel);

            z = writeHeader(3, parcel);
            parcel.writeInt(ECDSA_COSE_IDENTIFIER);
            writeLength(z, parcel);

            writeLength(d, parcel);
            writeLength(c, parcel);
        }

        writeLength(b, parcel);

        // 6: timeout
        if (options.timeout != null) {
            b = writeHeader(6, parcel);
            parcel.writeDouble(adjustTimeout(options.timeout));
            writeLength(b, parcel);
        }

        // 7: exclude list
        if (options.excludeCredentials != null && options.excludeCredentials.length != 0) {
            b = writeHeader(7, parcel);
            appendCredentialListToParcel(options.excludeCredentials, parcel);
            writeLength(b, parcel);
        }

        // 8: authenticator selection
        if (options.authenticatorSelection != null) {
            b = writeHeader(8, parcel);
            c = writeHeader(OBJECT_MAGIC, parcel);

            String attachment =
                    attachmentToString(options.authenticatorSelection.authenticatorAttachment);
            if (attachment != null) {
                z = writeHeader(2, parcel);
                parcel.writeString(attachment);
                writeLength(z, parcel);
            }

            z = writeHeader(3, parcel);
            parcel.writeInt(
                    options.authenticatorSelection.residentKey == ResidentKeyRequirement.REQUIRED
                            ? 1
                            : 0);
            writeLength(z, parcel);

            z = writeHeader(4, parcel);
            parcel.writeString(
                    userVerificationToString(options.authenticatorSelection.userVerification));
            writeLength(z, parcel);

            z = writeHeader(5, parcel);
            parcel.writeString(residentKeyToString(options.authenticatorSelection.residentKey));
            writeLength(z, parcel);

            writeLength(c, parcel);
            writeLength(b, parcel);
        }

        // 11: attestation preference
        b = writeHeader(11, parcel);
        parcel.writeString(attestationPreferenceToString(options.attestation));
        writeLength(b, parcel);

        // 12: extensions
        if (options.devicePublicKey != null || options.isPaymentCredentialCreation
                || options.prfEnable) {
            b = writeHeader(12, parcel);
            appendMakeCredentialExtensionsToParcel(options, parcel);
            writeLength(b, parcel);
        }

        writeLength(a, parcel);
    }

    /**
     * Serialize known extensions for an app's makeCredential request to a {@link Parcel}.
     *
     * @param options the options passed from the renderer.
     * @param parcel the {@link Parcel} to append the output to.
     */
    private static void appendMakeCredentialExtensionsToParcel(
            PublicKeyCredentialCreationOptions options, Parcel parcel) {
        final int a = writeHeader(OBJECT_MAGIC, parcel);

        // 8: devicePubKey
        if (options.devicePublicKey != null) {
            final int b = writeHeader(8, parcel);
            final int c = writeHeader(OBJECT_MAGIC, parcel);
            final int d = writeHeader(1, parcel);
            parcel.writeInt(1);
            writeLength(d, parcel);
            writeLength(c, parcel);
            writeLength(b, parcel);
        }

        // 10: GoogleThirdPartyPayment
        //
        // This only has effect if set to 'true' (i.e., omitting it and setting it to false are
        // equivalent), so we only include the 'true' case.
        if (options.isPaymentCredentialCreation) {
            final int b = writeHeader(10, parcel);
            final int c = writeHeader(OBJECT_MAGIC, parcel);
            final int d = writeHeader(1, parcel);
            parcel.writeInt(1);
            writeLength(d, parcel);
            writeLength(c, parcel);
            writeLength(b, parcel);
        }

        // 11: PRF
        if (options.prfEnable) {
            final int b = writeHeader(11, parcel);
            final int c = writeHeader(OBJECT_MAGIC, parcel);
            final int d = writeHeader(1, parcel);
            // length of PRF inputs. None for makeCredential.
            parcel.writeInt(0);
            writeLength(d, parcel);
            writeLength(c, parcel);
            writeLength(b, parcel);
        }

        writeLength(a, parcel);
    }

    /**
     * Serialize a browser's getAssertion request to a {@link Parcel}.
     *
     * @param options the options passed from the renderer.
     * @param origin the origin that the request should act as.
     * @param clientDataHash (optional) override the ClientDataJSON generated by Play Services.
     * @param parcel the {@link Parcel} to append the output to.
     */
    public static void appendBrowserGetAssertionOptionsToParcel(
            PublicKeyCredentialRequestOptions options, Uri origin, byte[] clientDataHash,
            byte[] tunnelId, Parcel parcel) {
        final int a = writeHeader(OBJECT_MAGIC, parcel);

        // 2: PublicKeyCredentialRequestOptions
        int z = writeHeader(2, parcel);
        appendGetAssertionOptionsToParcel(options, tunnelId, parcel);
        writeLength(z, parcel);

        // 3: origin
        z = writeHeader(3, parcel);
        origin.writeToParcel(parcel, 0);
        writeLength(z, parcel);

        // 4: clientDataHash
        if (clientDataHash != null) {
            z = writeHeader(4, parcel);
            parcel.writeByteArray(clientDataHash);
            writeLength(z, parcel);
        }

        writeLength(a, parcel);
    }

    /**
     * Serialize an app's getAssertion request to a {@link Parcel}.
     *
     * @param options the options passed from the renderer.
     * @param parcel the {@link Parcel} to append the output to.
     */
    public static void appendGetAssertionOptionsToParcel(
            PublicKeyCredentialRequestOptions options, byte[] tunnelId, Parcel parcel) {
        final int a = writeHeader(OBJECT_MAGIC, parcel);

        // 2: challenge
        int z = writeHeader(2, parcel);
        parcel.writeByteArray(options.challenge);
        writeLength(z, parcel);

        // 3: timeout
        if (options.timeout != null) {
            z = writeHeader(3, parcel);
            parcel.writeDouble(adjustTimeout(options.timeout));
            writeLength(z, parcel);
        }

        // 4: RP ID
        z = writeHeader(4, parcel);
        parcel.writeString(options.relyingPartyId);
        writeLength(z, parcel);

        // 5: allow list
        if (options.allowCredentials != null) {
            z = writeHeader(5, parcel);
            appendCredentialListToParcel(options.allowCredentials, parcel);
            writeLength(z, parcel);
        }

        // 8: user verification
        z = writeHeader(8, parcel);
        parcel.writeString(userVerificationToString(options.userVerification));
        writeLength(z, parcel);

        // 9: extensions
        z = writeHeader(9, parcel);
        appendGetAssertionExtensionsToParcel(options, tunnelId, parcel);
        writeLength(z, parcel);

        writeLength(a, parcel);
    }

    private static void appendGetAssertionExtensionsToParcel(
            PublicKeyCredentialRequestOptions options, byte[] tunnelId, Parcel parcel) {
        final int a = writeHeader(OBJECT_MAGIC, parcel);

        // 2: appId
        if (options.appid != null) {
            final int b = writeHeader(2, parcel);
            final int c = writeHeader(OBJECT_MAGIC, parcel);
            final int d = writeHeader(2, parcel);
            parcel.writeString(options.appid);
            writeLength(d, parcel);
            writeLength(c, parcel);
            writeLength(b, parcel);
        }

        // 4: user verification methods
        if (options.userVerificationMethods) {
            final int b = writeHeader(4, parcel);
            final int c = writeHeader(OBJECT_MAGIC, parcel);
            final int d = writeHeader(1, parcel);
            parcel.writeInt(1);
            writeLength(d, parcel);
            writeLength(c, parcel);
            writeLength(b, parcel);
        }

        // 8: device public key
        if (options.devicePublicKey != null) {
            final int b = writeHeader(8, parcel);
            final int c = writeHeader(OBJECT_MAGIC, parcel);
            final int d = writeHeader(1, parcel);
            parcel.writeInt(1);
            writeLength(d, parcel);
            writeLength(c, parcel);
            writeLength(b, parcel);
        }

        // 11: PRF
        if (options.prf) {
            final int b = writeHeader(11, parcel);
            final int c = writeHeader(OBJECT_MAGIC, parcel);
            final int d = writeHeader(1, parcel);
            parcel.writeInt(2 * options.prfInputs.length);
            for (PrfValues input : options.prfInputs) {
                parcel.writeByteArray(input.id);
                if (input.second == null) {
                    parcel.writeByteArray(input.first);
                } else {
                    byte[] values = new byte[input.first.length + input.second.length];
                    System.arraycopy(input.first, 0, values, 0, input.first.length);
                    System.arraycopy(
                            input.second, 0, values, input.first.length, input.second.length);
                    parcel.writeByteArray(values);
                }
            }
            writeLength(d, parcel);
            writeLength(c, parcel);
            writeLength(b, parcel);
        }

        if (tunnelId != null) {
            final int b = writeHeader(9, parcel);
            final int c = writeHeader(OBJECT_MAGIC, parcel);
            final int d = writeHeader(1, parcel);
            parcel.writeString(Base64.encodeToString(tunnelId, Base64.NO_WRAP));
            writeLength(d, parcel);
            writeLength(c, parcel);
            writeLength(b, parcel);
        }

        writeLength(a, parcel);
    }

    private static void appendCredentialListToParcel(
            PublicKeyCredentialDescriptor[] creds, Parcel parcel) {
        parcel.writeInt(creds.length);
        for (PublicKeyCredentialDescriptor cred : creds) {
            int a = startLength(parcel);
            int b = writeHeader(OBJECT_MAGIC, parcel);

            int z = writeHeader(2, parcel);
            parcel.writeString(credentialTypeToString(cred.type));
            writeLength(z, parcel);

            z = writeHeader(3, parcel);
            parcel.writeByteArray(cred.id);
            writeLength(z, parcel);

            int c = writeHeader(4, parcel);
            parcel.writeInt(cred.transports.length);
            for (int transport : cred.transports) {
                z = startLength(parcel);
                parcel.writeString(transportToString(transport));
                writeLength(z, parcel);
            }
            writeLength(c, parcel);

            writeLength(b, parcel);
            writeLength(a, parcel);
        }
    }

    /**
     * Write a SafeParcelable-style header.
     * <p>
     * See the class comment for a description of this function works.
     *
     * @param tag the tag number to encode (<65536).
     * @param parcel the {@link Parcel} to append to.
     * @return the offset of the placeholder length.
     */
    private static int writeHeader(int tag, Parcel parcel) {
        assert tag < 0x10000;
        parcel.writeInt(0xffff0000 | tag);
        return startLength(parcel);
    }

    /**
     * Write a SafeParcelable-style length.
     * <p>
     * See the class comment for a description of this function works.
     *
     * @param parcel the {@link Parcel} to append to.
     * @return the offset of the placeholder length.
     */
    private static int startLength(Parcel parcel) {
        int pos = parcel.dataPosition();
        parcel.writeInt(0xdddddddd);
        return pos;
    }

    /**
     * Fill in a previous placeholder length.
     * <p>
     * See the class comment for a description of this function works.
     *
     * @param pos the return value from {@link writeHeader} or {@link writeLength}.
     * @param parcel the {@link Parcel} to append to.
     */
    private static void writeLength(int pos, Parcel parcel) {
        int totalLength = parcel.dataPosition();
        parcel.setDataPosition(pos);
        parcel.writeInt(totalLength - pos - 4);
        parcel.setDataPosition(totalLength);
    }

    private static String attachmentToString(int attachment) {
        // This is the closest one can get to a static assert that no new enumeration values have
        // been added.
        assert AuthenticatorAttachment.MAX_VALUE == AuthenticatorAttachment.CROSS_PLATFORM;

        switch (attachment) {
            case AuthenticatorAttachment.NO_PREFERENCE:
            default:
                return null;
            case AuthenticatorAttachment.PLATFORM:
                return "platform";
            case AuthenticatorAttachment.CROSS_PLATFORM:
                return "cross-platform";
        }
    }

    private static int stringToAttachment(String v) {
        // This is the closest one can get to a static assert that no new enumeration values have
        // been added.
        assert AuthenticatorAttachment.MAX_VALUE == AuthenticatorAttachment.CROSS_PLATFORM;

        if (v.equals("platform")) {
            return AuthenticatorAttachment.PLATFORM;
        } else if (v.equals("cross-platform")) {
            return AuthenticatorAttachment.CROSS_PLATFORM;
        }
        return AuthenticatorAttachment.MIN_VALUE - 1;
    }

    private static String credentialTypeToString(int credType) {
        // This is the closest one can get to a static assert that no new enumeration values have
        // been added.
        assert PublicKeyCredentialType.MIN_VALUE == PublicKeyCredentialType.MAX_VALUE;

        switch (credType) {
            case PublicKeyCredentialType.PUBLIC_KEY:
            default:
                return "public-key";
        }
    }

    private static String transportToString(int transport) {
        // This is the closest one can get to a static assert that no new enumeration values have
        // been added.
        assert AuthenticatorTransport.MAX_VALUE == AuthenticatorTransport.INTERNAL;

        switch (transport) {
            case AuthenticatorTransport.USB:
            default:
                return "usb";
            case AuthenticatorTransport.NFC:
                return "nfc";
            case AuthenticatorTransport.BLE:
                return "ble";
            case AuthenticatorTransport.INTERNAL:
                return "internal";
            case AuthenticatorTransport.HYBRID:
                return "cable";
        }
    }

    private static String userVerificationToString(int uv) {
        // This is the closest one can get to a static assert that no new enumeration values have
        // been added.
        assert UserVerificationRequirement.MAX_VALUE == UserVerificationRequirement.DISCOURAGED;

        switch (uv) {
            case UserVerificationRequirement.REQUIRED:
                return "required";
            case UserVerificationRequirement.PREFERRED:
            default:
                return "preferred";
            case UserVerificationRequirement.DISCOURAGED:
                return "discouraged";
        }
    }

    private static String residentKeyToString(int rk) {
        // This is the closest one can get to a static assert that no new enumeration values have
        // been added.
        assert ResidentKeyRequirement.MAX_VALUE == ResidentKeyRequirement.REQUIRED;

        switch (rk) {
            case ResidentKeyRequirement.REQUIRED:
                return "required";
            case ResidentKeyRequirement.PREFERRED:
                return "preferred";
            case ResidentKeyRequirement.DISCOURAGED:
            default:
                return "discouraged";
        }
    }

    private static String attestationPreferenceToString(int attestationPref) {
        // This is the closest one can get to a static assert that no new enumeration values have
        // been added.
        assert AttestationConveyancePreference.MAX_VALUE
                == AttestationConveyancePreference.ENTERPRISE;

        switch (attestationPref) {
            case AttestationConveyancePreference.NONE:
            default:
                return "none";
            case AttestationConveyancePreference.INDIRECT:
                return "indirect";
            case AttestationConveyancePreference.DIRECT:
                return "direct";
            case AttestationConveyancePreference.ENTERPRISE:
                return "direct"; // cannot be represented in the GMS Core API.
        }
    }

    /**
     * Read a SafeParcelable-style tag and length.
     *
     * @param parcel the {@link Parcel} to read from.
     * @return a (tag, length) pair.
     */
    private static Pair<Integer, Integer> readHeader(Parcel parcel) {
        int a = parcel.readInt();
        int tag = a & 0xffff;
        int length = (a >> 16) & 0xffff;
        if (length == 0xffff) {
            length = parcel.readInt();
        }

        return new Pair(tag, length);
    }

    /**
     * Read a FIDO API response from a {@link PendingIntent} result.
     *
     * @param data the Intent, as passed to {@link Activity.onActivityResult}.
     * @param attestationAcceptable if expecting a makeCredential response, this controls whether
     *         attestation of the primary credential will be included.
     * @return see {@link parseResponse}.
     * @throws IllegalArgumentException if there was a parse error.
     */
    public static @Nullable Object parseIntentResponse(Intent data, boolean attestationAcceptable)
            throws IllegalArgumentException {
        byte[] responseBytes = data.getByteArrayExtra(CREDENTIAL_EXTRA);
        if (responseBytes == null) {
            Log.e(TAG, "FIDO2 PendingIntent missing response");
            throw new IllegalArgumentException();
        }

        final Object response = parseResponse(responseBytes, attestationAcceptable);
        if (response == null) {
            Log.e(TAG, "Failed to parse FIDO2 API response");
            throw new IllegalArgumentException();
        }

        return response;
    }

    /**
     * Read a FIDO API response from a bytestring.
     *
     * @param responseBytes an encoded PublicKeyCredential object.
     * @param attestationAcceptable if expecting a makeCredential response, this controls whether
     *         attestation of the primary credential will be included.
     * @return One of the following: 1) a Pair&lt;Integer, String&gt;, if the response is an error.
     *         (The first value is the error code, the second is an optional error message.) 2) a
     *         MakeCredentialAuthenticatorResponse. 3) a GetAssertionAuthenticatorResponse.
     * @throws IllegalArgumentException if there was a parse error.
     */
    public static Object parseResponse(byte[] responseBytes, boolean attestationAcceptable)
            throws IllegalArgumentException {
        Parcel parcel = Parcel.obtain();
        parcel.unmarshall(responseBytes, 0, responseBytes.length);
        parcel.setDataPosition(0);

        Pair<Integer, Integer> header = readHeader(parcel);
        if (header.first != OBJECT_MAGIC) {
            throw new IllegalArgumentException();
        }
        final int endPosition = addLengthToParcelPosition(header.second, parcel);

        MakeCredentialAuthenticatorResponse creationResponse = null;
        GetAssertionAuthenticatorResponse assertionResponse = null;
        Extensions extensions = null;
        int attachment = AuthenticatorAttachment.MIN_VALUE - 1;

        while (parcel.dataPosition() < endPosition) {
            header = readHeader(parcel);
            switch (header.first) {
                case 4:
                    // Attestation response
                    creationResponse = parseAttestationResponse(parcel, attestationAcceptable);
                    if (creationResponse == null) {
                        throw new IllegalArgumentException();
                    }
                    // The response may need to have attachment information included, which is in
                    // another field.
                    break;

                case 5:
                    // Sign response
                    assertionResponse = parseAssertionResponse(parcel);
                    if (assertionResponse == null) {
                        throw new IllegalArgumentException();
                    }
                    // An assertion response may need to be merged with
                    // extension information, which is in another field.
                    break;

                case 6:
                    // Error
                    return parseErrorResponse(parcel);

                case 7:
                    // Extension outputs
                    extensions = parseExtensionResponse(parcel);
                    if (extensions == null) {
                        throw new IllegalArgumentException();
                    }
                    break;

                case 8:
                    // authenticatorAttachment
                    attachment = stringToAttachment(parcel.readString());
                    break;

                default:
                    // unknown tag. Skip over it.
                    parcel.setDataPosition(addLengthToParcelPosition(header.second, parcel));
            }
        }

        if (creationResponse != null) {
            if (attachment >= AuthenticatorAttachment.MIN_VALUE) {
                creationResponse.authenticatorAttachment = attachment;
            }
            if (extensions != null) {
                if (extensions.devicePublicKey != null) {
                    creationResponse.devicePublicKey = extensions.devicePublicKey;
                    creationResponse.devicePublicKey.authenticatorOutput =
                            Fido2ApiJni.get().getDevicePublicKeyFromAuthenticatorData(
                                    creationResponse.info.authenticatorData);
                }
                if (extensions.hasCredProps) {
                    creationResponse.hasCredPropsRk = true;
                    creationResponse.credPropsRk = extensions.didCreateDiscoverableCredential;
                }
                if (extensions.prf != null) {
                    creationResponse.echoPrf = true;
                    creationResponse.prf = extensions.prf.first;
                }
            }
            return creationResponse;
        }

        if (assertionResponse != null) {
            if (extensions != null && extensions.userVerificationMethods != null) {
                assertionResponse.echoUserVerificationMethods = true;
                assertionResponse.userVerificationMethods = new UvmEntry[0];
                assertionResponse.userVerificationMethods =
                        extensions.userVerificationMethods.toArray(
                                assertionResponse.userVerificationMethods);
            }
            if (extensions != null && extensions.devicePublicKey != null) {
                assertionResponse.devicePublicKey = extensions.devicePublicKey;
                assertionResponse.devicePublicKey.authenticatorOutput =
                        Fido2ApiJni.get().getDevicePublicKeyFromAuthenticatorData(
                                assertionResponse.info.authenticatorData);
            }
            if (extensions != null && extensions.prf != null) {
                assertionResponse.echoPrf = true;
                assertionResponse.prfResults = new PrfValues();
                if (extensions.prf.second.length == 32) {
                    assertionResponse.prfResults.first = extensions.prf.second;
                } else {
                    assertionResponse.prfResults.first = new byte[32];
                    assertionResponse.prfResults.second = new byte[32];
                    System.arraycopy(
                            extensions.prf.second, 0, assertionResponse.prfResults.first, 0, 32);
                    System.arraycopy(
                            extensions.prf.second, 32, assertionResponse.prfResults.second, 0, 32);
                }
            }
            if (attachment >= AuthenticatorAttachment.MIN_VALUE) {
                assertionResponse.authenticatorAttachment = attachment;
            }
            return assertionResponse;
        }

        throw new IllegalArgumentException();
    }

    private static MakeCredentialAuthenticatorResponse parseAttestationResponse(
            Parcel parcel, boolean attestationAcceptable) throws IllegalArgumentException {
        Pair<Integer, Integer> header = readHeader(parcel);
        if (header.first != OBJECT_MAGIC) {
            throw new IllegalArgumentException();
        }
        final int endPosition = addLengthToParcelPosition(header.second, parcel);

        byte[] keyHandle = null;
        byte[] clientDataJson = null;
        byte[] attestationObject = null;
        int[] transports = new int[] {};

        while (parcel.dataPosition() < endPosition) {
            header = readHeader(parcel);
            switch (header.first) {
                case 2:
                    keyHandle = parcel.createByteArray();
                    break;

                case 3:
                    clientDataJson = parcel.createByteArray();
                    break;

                case 4:
                    attestationObject = parcel.createByteArray();
                    break;

                case 5:
                    transports = parseTransports(parcel);
                    break;

                default:
                    // unknown tag. Skip over it.
                    parcel.setDataPosition(addLengthToParcelPosition(header.second, parcel));
            }
        }

        if (keyHandle == null || clientDataJson == null || attestationObject == null) {
            throw new IllegalArgumentException();
        }

        MakeCredentialAuthenticatorResponse ret = new MakeCredentialAuthenticatorResponse();
        CommonCredentialInfo info = new CommonCredentialInfo();

        AttestationObjectParts parts = new AttestationObjectParts();
        if (!Fido2ApiJni.get().parseAttestationObject(
                    attestationObject, attestationAcceptable, parts)) {
            // A failure to parse the attestation object is fatal to the request
            // on desktop and so the same behavior is used here.
            throw new IllegalArgumentException();
        }
        ret.publicKeyAlgo = parts.coseAlgorithm;
        info.authenticatorData = parts.authenticatorData;
        ret.publicKeyDer = parts.spki;
        ret.attestationObject = parts.attestationObject;
        ret.transports = transports;

        info.id = encodeId(keyHandle);
        info.rawId = keyHandle;
        info.clientDataJson = clientDataJson;
        ret.info = info;
        return ret;
    }

    private static int[] parseTransports(Parcel parcel) throws IllegalArgumentException {
        int numValues = parcel.readInt();
        int[] pending = new int[numValues];
        int j = 0;

        for (int i = 0; i < numValues; i++) {
            String transport = parcel.readString();

            if (transport.equals("usb")) {
                pending[j++] = AuthenticatorTransport.USB;
            } else if (transport.equals("nfc")) {
                pending[j++] = AuthenticatorTransport.NFC;
            } else if (transport.equals("ble")) {
                pending[j++] = AuthenticatorTransport.BLE;
            } else if (transport.equals("cable") || transport.equals("hybrid")) {
                pending[j++] = AuthenticatorTransport.HYBRID;
            } else if (transport.equals("internal")) {
                pending[j++] = AuthenticatorTransport.INTERNAL;
            }
        }

        if (j == numValues) {
            return pending;
        }

        int[] ret = new int[j];
        System.arraycopy(pending, 0, ret, 0, j);
        return ret;
    }

    private static GetAssertionAuthenticatorResponse parseAssertionResponse(Parcel parcel)
            throws IllegalArgumentException {
        Pair<Integer, Integer> header = readHeader(parcel);
        if (header.first != OBJECT_MAGIC) {
            throw new IllegalArgumentException();
        }
        final int endPosition = addLengthToParcelPosition(header.second, parcel);

        byte[] keyHandle = null;
        byte[] clientDataJson = null;
        byte[] authenticatorData = null;
        byte[] signature = null;
        byte[] userHandle = null;

        while (parcel.dataPosition() < endPosition) {
            header = readHeader(parcel);
            switch (header.first) {
                case 2:
                    keyHandle = parcel.createByteArray();
                    break;

                case 3:
                    clientDataJson = parcel.createByteArray();
                    break;

                case 4:
                    authenticatorData = parcel.createByteArray();
                    break;

                case 5:
                    signature = parcel.createByteArray();
                    break;

                case 6:
                    userHandle = parcel.createByteArray();
                    break;

                default:
                    // unknown tag. Skip over it.
                    parcel.setDataPosition(addLengthToParcelPosition(header.second, parcel));
            }
        }

        if (keyHandle == null || clientDataJson == null || authenticatorData == null
                || signature == null) {
            throw new IllegalArgumentException();
        }

        CommonCredentialInfo info = new CommonCredentialInfo();
        info.authenticatorData = authenticatorData;
        info.id = encodeId(keyHandle);
        info.rawId = keyHandle;
        info.clientDataJson = clientDataJson;

        GetAssertionAuthenticatorResponse response = new GetAssertionAuthenticatorResponse();
        response.info = info;
        response.signature = signature;
        response.userHandle = userHandle;

        return response;
    }

    private static Pair<Integer, String> parseErrorResponse(Parcel parcel)
            throws IllegalArgumentException {
        Pair<Integer, Integer> header = readHeader(parcel);
        if (header.first != OBJECT_MAGIC) {
            throw new IllegalArgumentException();
        }
        final int endPosition = addLengthToParcelPosition(header.second, parcel);

        Integer code = null;
        String message = null;

        while (parcel.dataPosition() < endPosition) {
            header = readHeader(parcel);
            switch (header.first) {
                case 2:
                    code = parcel.readInt();
                    break;

                case 3:
                    message = parcel.readString();
                    break;

                default:
                    // unknown tag. Skip over it.
                    parcel.setDataPosition(addLengthToParcelPosition(header.second, parcel));
            }
        }

        if (code == null /* `message` is optional */) {
            throw new IllegalArgumentException();
        }

        return new Pair<>(code, message);
    }

    private static class Extensions {
        public ArrayList<UvmEntry> userVerificationMethods;
        public DevicePublicKeyResponse devicePublicKey;
        public boolean hasCredProps;
        public boolean didCreateDiscoverableCredential;
        // prf contains an "enabled" flag and a bytestring that contains either
        // one or two 32-byte strings.
        public Pair<Boolean, byte[]> prf;
    }

    private static Extensions parseExtensionResponse(Parcel parcel)
            throws IllegalArgumentException {
        Pair<Integer, Integer> header = readHeader(parcel);
        if (header.first != OBJECT_MAGIC) {
            throw new IllegalArgumentException();
        }
        final int endPosition = addLengthToParcelPosition(header.second, parcel);

        Extensions ret = new Extensions();

        while (parcel.dataPosition() < endPosition) {
            header = readHeader(parcel);
            switch (header.first) {
                case 1:
                    ret.userVerificationMethods = parseUvmEntries(parcel);
                    if (ret.userVerificationMethods == null) {
                        throw new IllegalArgumentException();
                    }
                    break;

                case 2:
                    ret.devicePublicKey = parseDevicePublicKeyResponse(parcel);
                    break;

                case 3:
                    ret.hasCredProps = true;
                    ret.didCreateDiscoverableCredential = parseCredPropsResponse(parcel);
                    break;

                case 4:
                    ret.prf = parsePrfResponse(parcel);
                    break;

                default:
                    // unknown tag. Skip over it.
                    parcel.setDataPosition(addLengthToParcelPosition(header.second, parcel));
            }
        }

        return ret;
    }

    private static ArrayList<UvmEntry> parseUvmEntries(Parcel parcel)
            throws IllegalArgumentException {
        Pair<Integer, Integer> header = readHeader(parcel);
        if (header.first != OBJECT_MAGIC) {
            throw new IllegalArgumentException();
        }
        final int endPosition = addLengthToParcelPosition(header.second, parcel);

        ArrayList<UvmEntry> ret = new ArrayList<>();

        while (parcel.dataPosition() < endPosition) {
            header = readHeader(parcel);
            switch (header.first) {
                case 1:
                    int num = parcel.readInt();
                    for (int i = 0; i < num; i++) {
                        int unusedLength = parcel.readInt();
                        UvmEntry entry = parseUvmEntry(parcel);
                        if (entry == null) {
                            throw new IllegalArgumentException();
                        }
                        ret.add(entry);
                    }
                    break;

                default:
                    // unknown tag. Skip over it.
                    parcel.setDataPosition(addLengthToParcelPosition(header.second, parcel));
            }
        }

        return ret;
    }

    private static UvmEntry parseUvmEntry(Parcel parcel) throws IllegalArgumentException {
        Pair<Integer, Integer> header = readHeader(parcel);
        if (header.first != OBJECT_MAGIC) {
            throw new IllegalArgumentException();
        }
        final int endPosition = addLengthToParcelPosition(header.second, parcel);

        UvmEntry ret = new UvmEntry();

        while (parcel.dataPosition() < endPosition) {
            header = readHeader(parcel);
            switch (header.first) {
                case 1:
                    ret.userVerificationMethod = parcel.readInt();
                    break;

                case 2:
                    ret.keyProtectionType = (short) parcel.readInt();
                    break;

                case 3:
                    ret.matcherProtectionType = (short) parcel.readInt();
                    break;

                default:
                    // unknown tag. Skip over it.
                    parcel.setDataPosition(addLengthToParcelPosition(header.second, parcel));
            }
        }

        return ret;
    }

    private static DevicePublicKeyResponse parseDevicePublicKeyResponse(Parcel parcel)
            throws IllegalArgumentException {
        Pair<Integer, Integer> header = readHeader(parcel);
        if (header.first != OBJECT_MAGIC) {
            throw new IllegalArgumentException();
        }
        final int endPosition = addLengthToParcelPosition(header.second, parcel);

        DevicePublicKeyResponse ret = new DevicePublicKeyResponse();

        while (parcel.dataPosition() < endPosition) {
            header = readHeader(parcel);
            switch (header.first) {
                case 1:
                    ret.signature = parcel.createByteArray();
                    break;

                default:
                    // unknown tag. Skip over it.
                    parcel.setDataPosition(addLengthToParcelPosition(header.second, parcel));
            }
        }

        if (ret.signature == null) {
            throw new IllegalArgumentException();
        }

        return ret;
    }

    private static boolean parseCredPropsResponse(Parcel parcel) throws IllegalArgumentException {
        Pair<Integer, Integer> header = readHeader(parcel);
        if (header.first != OBJECT_MAGIC) {
            throw new IllegalArgumentException();
        }
        final int endPosition = addLengthToParcelPosition(header.second, parcel);

        boolean ret = false;

        while (parcel.dataPosition() < endPosition) {
            header = readHeader(parcel);
            switch (header.first) {
                case 1:
                    ret = parcel.readInt() != 0;
                    break;

                default:
                    // unknown tag. Skip over it.
                    parcel.setDataPosition(addLengthToParcelPosition(header.second, parcel));
            }
        }

        return ret;
    }

    private static Pair<Boolean, byte[]> parsePrfResponse(Parcel parcel)
            throws IllegalArgumentException {
        Pair<Integer, Integer> header = readHeader(parcel);
        if (header.first != OBJECT_MAGIC) {
            throw new IllegalArgumentException();
        }
        final int endPosition = addLengthToParcelPosition(header.second, parcel);

        boolean enabled = false;
        byte[] outputs = null;

        while (parcel.dataPosition() < endPosition) {
            header = readHeader(parcel);
            switch (header.first) {
                case 1:
                    enabled = parcel.readInt() != 0;
                    break;

                case 2:
                    outputs = parcel.createByteArray();
                    if (outputs.length != 32 && outputs.length != 64) {
                        throw new IllegalArgumentException("bad PRF output length");
                    }
                    break;

                default:
                    // unknown tag. Skip over it.
                    parcel.setDataPosition(addLengthToParcelPosition(header.second, parcel));
            }
        }

        return Pair.create(enabled, outputs);
    }

    /**
     * Return a position that is `length` bytes after the current {@link Parcel} position.
     * <p>
     * This function safely adds an untrusted length to a {@link Parcel} position, watching
     * for overflow and for exceeding the bounds of the data.
     *
     * @throws IllegalArgumentException when running off the end of the data.
     */
    private static int addLengthToParcelPosition(int length, Parcel parcel)
            throws IllegalArgumentException {
        final int ret = length + parcel.dataPosition();
        if (length < 0 || ret < length || ret > parcel.dataSize()) {
            throw new IllegalArgumentException();
        }
        return ret;
    }

    /**
     * Base64 encodes the raw id.
     * @param keyHandle the raw id (key handle of the credential).
     * @return Base64-encoded id.
     */
    private static String encodeId(byte[] keyHandle) {
        return Base64.encodeToString(
                keyHandle, Base64.URL_SAFE | Base64.NO_PADDING | Base64.NO_WRAP);
    }

    /**
     * Adjusts a timeout between a reasonable minimum and maximum.
     *
     * @param timeout The unadjusted timeout as specified by the website. May be null.
     * @return The adjusted timeout in seconds.
     */
    private static double adjustTimeout(TimeDelta timeout) {
        if (timeout == null) return MAX_TIMEOUT_SECONDS;

        return Math.max(MIN_TIMEOUT_SECONDS,
                Math.min(MAX_TIMEOUT_SECONDS,
                        TimeUnit.MICROSECONDS.toSeconds(timeout.microseconds)));
    }

    /** AttestationObjectParts groups together the return values of |parseAttestationObject|. */
    public static final class AttestationObjectParts {
        @CalledByNative("AttestationObjectParts")
        void setAll(byte[] authenticatorData, byte[] spki, int coseAlgorithm,
                byte[] attestationObject) {
            this.authenticatorData = authenticatorData;
            this.spki = spki;
            this.coseAlgorithm = coseAlgorithm;
            this.attestationObject = attestationObject;
        }

        public byte[] authenticatorData;
        public byte[] spki;
        public int coseAlgorithm;
        public byte[] attestationObject;
    }

    /**
     * Parse a {@link WebAuthnCredentialDetails} list from a parcel.
     *
     * @param parcel the {@link parcel} with current position set to the beginning of the list.
     * @return The list of {@link WebAuthnCredentialDetails} if successfully parsed.
     * @throws IllegalArgumentException if a parsing error is encountered.
     */
    public static ArrayList<WebAuthnCredentialDetails> parseCredentialList(Parcel parcel)
            throws IllegalArgumentException {
        int numCredentials = parcel.readInt();
        ArrayList<WebAuthnCredentialDetails> credentials = new ArrayList<>();
        for (int i = 0; i < numCredentials; i++) {
            WebAuthnCredentialDetails details = new WebAuthnCredentialDetails();

            // The array is as written by `Parcel.writeArray`. Each element of the array is prefixed
            // by the class name of that element. The class names will be
            // "com.google.android.gms.fido.fido2.api.common.DiscoverableCredentialInfo" but that
            // isn't checked here to avoid depending on the name of the class.
            if (parcel.readInt() != VAL_PARCELABLE) {
                throw new IllegalArgumentException();
            }
            if (sParcelUsesLengthPrefixes) {
                parcel.readInt(); // discard length prefix.
            }
            parcel.readString(); // ignore class name
            Pair<Integer, Integer> header = readHeader(parcel);
            if (header.first != OBJECT_MAGIC) {
                throw new IllegalArgumentException();
            }
            final int endPosition = addLengthToParcelPosition(header.second, parcel);

            // The original version of this API returned only discoverable credentials, not usable
            // for Secure Payment Confirmation. If the tags are missing, this is the default.
            details.mIsDiscoverable = true;
            details.mIsPayment = false;

            while (parcel.dataPosition() < endPosition) {
                header = readHeader(parcel);
                switch (header.first) {
                    case 1:
                        details.mUserName = parcel.readString();
                        break;
                    case 2:
                        details.mUserDisplayName = parcel.readString();
                        break;
                    case 3:
                        details.mUserId = parcel.createByteArray();
                        break;
                    case 4:
                        details.mCredentialId = parcel.createByteArray();
                        break;
                    case 5:
                        details.mIsDiscoverable = parcel.readInt() != 0;
                        break;
                    case 6:
                        details.mIsPayment = parcel.readInt() != 0;
                        break;
                    default:
                        // unknown tag. Skip over it.
                        parcel.setDataPosition(addLengthToParcelPosition(header.second, parcel));
                }
            }
            if (details.mCredentialId == null) {
                throw new IllegalArgumentException();
            }
            if (details.mIsDiscoverable
                    && (details.mUserName == null || details.mUserDisplayName == null
                            || details.mUserId == null)) {
                throw new IllegalArgumentException();
            }
            credentials.add(details);
        }
        return credentials;
    }

    @NativeMethods
    interface Natives {
        // parseAttestationObject parses a CTAP2 attestation[1] and extracts the
        // parts that the browser provides via Javascript API [2]. If
        // `attestationAcceptable` is false then the returned attestation
        // object will have attestation stripped.
        //
        // [1] https://www.w3.org/TR/webauthn/#attestation-object
        // [2] https://w3c.github.io/webauthn/#sctn-public-key-easy
        boolean parseAttestationObject(byte[] attestationObject, boolean attestationAcceptable,
                AttestationObjectParts result);

        // getDevicePublicKeyFromAuthenticatorData extracts the DPK
        // authenticator output because this is returned in the client's DPK
        // extension output.
        byte[] getDevicePublicKeyFromAuthenticatorData(byte[] authenticatorData);
    }
}
