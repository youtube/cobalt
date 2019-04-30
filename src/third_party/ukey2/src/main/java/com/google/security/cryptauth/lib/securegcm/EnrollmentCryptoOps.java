/* Copyright 2018 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
package com.google.security.cryptauth.lib.securegcm;

import com.google.protobuf.InvalidProtocolBufferException;
import com.google.security.annotations.SuppressInsecureCipherModeCheckerPendingReview;
import com.google.security.cryptauth.lib.securegcm.SecureGcmProto.GcmDeviceInfo;
import com.google.security.cryptauth.lib.securegcm.SecureGcmProto.GcmMetadata;
import com.google.security.cryptauth.lib.securegcm.TransportCryptoOps.PayloadType;
import com.google.security.cryptauth.lib.securemessage.CryptoOps.EncType;
import com.google.security.cryptauth.lib.securemessage.CryptoOps.SigType;
import com.google.security.cryptauth.lib.securemessage.PublicKeyProtoUtil;
import com.google.security.cryptauth.lib.securemessage.SecureMessageBuilder;
import com.google.security.cryptauth.lib.securemessage.SecureMessageParser;
import com.google.security.cryptauth.lib.securemessage.SecureMessageProto.HeaderAndBody;
import com.google.security.cryptauth.lib.securemessage.SecureMessageProto.SecureMessage;
import java.security.InvalidKeyException;
import java.security.KeyPair;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.security.PrivateKey;
import java.security.PublicKey;
import java.security.SignatureException;
import java.security.spec.InvalidKeySpecException;
import java.util.Arrays;
import javax.crypto.KeyAgreement;
import javax.crypto.SecretKey;

/**
 * Utility class for implementing Secure GCM enrollment flows.
 */
public class EnrollmentCryptoOps {

  private EnrollmentCryptoOps() { }  // Do not instantiate

  /**
   * Type of symmetric key signature to use for the signcrypted "outer layer" message.
   */
  private static final SigType OUTER_SIG_TYPE = SigType.HMAC_SHA256;

  /**
   * Type of symmetric key encryption to use for the signcrypted "outer layer" message.
   */
  private static final EncType OUTER_ENC_TYPE = EncType.AES_256_CBC;

  /**
   * Type of public key signature to use for the (cleartext) "inner layer" message.
   */
  private static final SigType INNER_SIG_TYPE = SigType.ECDSA_P256_SHA256;

  /**
   * Type of public key signature to use for the (cleartext) "inner layer" message on platforms that
   * don't support Elliptic Curve operations (such as old Android versions).
   */
  private static final SigType LEGACY_INNER_SIG_TYPE = SigType.RSA2048_SHA256;

  /**
   * Which {@link KeyAgreement} algorithm to use.
   */
  private static final String KA_ALG = "ECDH";

  /**
   * Which {@link KeyAgreement} algorithm to use on platforms that don't support Elliptic Curve.
   */
  private static final String LEGACY_KA_ALG = "DH";

  /**
   * Used by both the client and server to perform a key exchange.
   *
   * @return a {@link SecretKey} derived from the key exchange
   * @throws InvalidKeyException if either of the input keys is of the wrong type
   */
  @SuppressInsecureCipherModeCheckerPendingReview // b/32143855
  public static SecretKey doKeyAgreement(PrivateKey myKey, PublicKey peerKey)
      throws InvalidKeyException {
    String alg = KA_ALG;
    if (KeyEncoding.isLegacyPrivateKey(myKey)) {
      alg = LEGACY_KA_ALG;
    }
    KeyAgreement agreement;
    try {
      agreement = KeyAgreement.getInstance(alg);
    } catch (NoSuchAlgorithmException e) {
      throw new RuntimeException(e);
    }

    agreement.init(myKey);
    agreement.doPhase(peerKey, true);
    byte[] agreedKey = agreement.generateSecret();

    // Derive a 256-bit AES key by using sha256 on the Diffie-Hellman output
    return KeyEncoding.parseMasterKey(sha256(agreedKey));
  }

  public static KeyPair generateEnrollmentKeyAgreementKeyPair(boolean isLegacy) {
    if (isLegacy) {
      return PublicKeyProtoUtil.generateDh2048KeyPair();
    }
    return PublicKeyProtoUtil.generateEcP256KeyPair();
  }

  /**
   * @return SHA-256 hash of {@code masterKey}
   */
  public static byte[] getMasterKeyHash(SecretKey masterKey) {
    return sha256(masterKey.getEncoded());
  }

  /**
   * Used by the client to signcrypt an enrollment request before sending it to the server.
   *
   *  <p>Note: You <em>MUST</em> correctly set the value of the {@code device_master_key_hash} on
   *  {@code enrollmentInfo} from {@link #getMasterKeyHash(SecretKey)} before calling this method.
   *
   * @param enrollmentInfo the enrollment request to send to the server. You must correctly set
   *   the {@code device_master_key_hash} field.
   * @param masterKey the shared key derived from the key agreement
   * @param signingKey the signing key corresponding to the user's {@link PublicKey} being enrolled
   * @return the encrypted enrollment message
   * @throws IllegalArgumentException if {@code enrollmentInfo} doesn't have a valid
   *   {@code device_master_key_hash}
   * @throws InvalidKeyException if {@code masterKey} or {@code signingKey} is the wrong type
   */
  public static byte[] encryptEnrollmentMessage(
      GcmDeviceInfo enrollmentInfo, SecretKey masterKey, PrivateKey signingKey)
          throws InvalidKeyException, NoSuchAlgorithmException {
    if ((enrollmentInfo == null) || (masterKey == null) || (signingKey == null)) {
      throw new NullPointerException();
    }

    if (!Arrays.equals(enrollmentInfo.getDeviceMasterKeyHash().toByteArray(),
        getMasterKeyHash(masterKey))) {
      throw new IllegalArgumentException("DeviceMasterKeyHash not set correctly");
    }

    // First create the inner message, which is basically a self-signed certificate
    SigType sigType =
        KeyEncoding.isLegacyPrivateKey(signingKey) ? LEGACY_INNER_SIG_TYPE : INNER_SIG_TYPE;
    SecureMessage innerMsg = new SecureMessageBuilder()
        .setVerificationKeyId(enrollmentInfo.getUserPublicKey().toByteArray())
        .buildSignedCleartextMessage(signingKey, sigType, enrollmentInfo.toByteArray());

    // Next create the outer message, which uses the newly exchanged master key to signcrypt
    SecureMessage outerMsg = new SecureMessageBuilder()
        .setVerificationKeyId(new byte[] {})  // Empty
        .setPublicMetadata(GcmMetadata.newBuilder()
            .setType(PayloadType.ENROLLMENT.getType())
            .setVersion(SecureGcmConstants.SECURE_GCM_VERSION)
            .build()
            .toByteArray())
        .buildSignCryptedMessage(
            masterKey, OUTER_SIG_TYPE, masterKey, OUTER_ENC_TYPE, innerMsg.toByteArray());
    return outerMsg.toByteArray();
  }

  /**
   * Used by the server to decrypt the client's enrollment request.
   * @param enrollmentMessage generated by the client's call to
   *        {@link #encryptEnrollmentMessage(GcmDeviceInfo, SecretKey, PrivateKey)}
   * @param masterKey the shared key derived from the key agreement
   * @return the client's enrollment request data
   * @throws SignatureException if {@code enrollmentMessage} is malformed or has been tampered with
   * @throws InvalidKeyException if {@code masterKey} is the wrong type
   */
  public static GcmDeviceInfo decryptEnrollmentMessage(
      byte[] enrollmentMessage, SecretKey masterKey, boolean isLegacy)
      throws SignatureException, InvalidKeyException, NoSuchAlgorithmException {
    if ((enrollmentMessage == null) || (masterKey == null)) {
      throw new NullPointerException();
    }

    HeaderAndBody outerHeaderAndBody;
    GcmMetadata outerMetadata;
    HeaderAndBody innerHeaderAndBody;
    byte[] encodedUserPublicKey;
    GcmDeviceInfo enrollmentInfo;
    try {
      SecureMessage outerMsg = SecureMessage.parseFrom(enrollmentMessage);
      outerHeaderAndBody = SecureMessageParser.parseSignCryptedMessage(
          outerMsg, masterKey, OUTER_SIG_TYPE, masterKey, OUTER_ENC_TYPE);
      outerMetadata = GcmMetadata.parseFrom(outerHeaderAndBody.getHeader().getPublicMetadata());

      SecureMessage innerMsg = SecureMessage.parseFrom(outerHeaderAndBody.getBody());
      encodedUserPublicKey = SecureMessageParser.getUnverifiedHeader(innerMsg)
          .getVerificationKeyId().toByteArray();
      PublicKey userPublicKey = KeyEncoding.parseUserPublicKey(encodedUserPublicKey);
      SigType sigType = isLegacy ? LEGACY_INNER_SIG_TYPE : INNER_SIG_TYPE;
      innerHeaderAndBody = SecureMessageParser.parseSignedCleartextMessage(
          innerMsg, userPublicKey, sigType);
      enrollmentInfo = GcmDeviceInfo.parseFrom(innerHeaderAndBody.getBody());
    } catch (InvalidProtocolBufferException e) {
      throw new SignatureException(e);
    } catch (InvalidKeySpecException e) {
      throw new SignatureException(e);
    }

    boolean verified =
           (outerMetadata.getType() == PayloadType.ENROLLMENT.getType())
        && (outerMetadata.getVersion() <= SecureGcmConstants.SECURE_GCM_VERSION)
        && outerHeaderAndBody.getHeader().getVerificationKeyId().isEmpty()
        && innerHeaderAndBody.getHeader().getPublicMetadata().isEmpty()
        // Verify the encoded public key we used matches the encoded public key key being enrolled
        && Arrays.equals(encodedUserPublicKey, enrollmentInfo.getUserPublicKey().toByteArray())
        && Arrays.equals(getMasterKeyHash(masterKey),
            enrollmentInfo.getDeviceMasterKeyHash().toByteArray());

    if (verified) {
      return enrollmentInfo;
    }
    throw new SignatureException();
  }

  static byte[] sha256(byte[] input) {
    try {
      MessageDigest sha256 = MessageDigest.getInstance("SHA-256");
      return sha256.digest(input);
    } catch (NoSuchAlgorithmException e) {
      throw new RuntimeException(e);  // Shouldn't happen
    }
  }
}
