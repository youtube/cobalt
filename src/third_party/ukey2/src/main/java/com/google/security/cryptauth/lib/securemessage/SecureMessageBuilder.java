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
package com.google.security.cryptauth.lib.securemessage;

import com.google.protobuf.ByteString;
import com.google.security.cryptauth.lib.securemessage.CryptoOps.EncType;
import com.google.security.cryptauth.lib.securemessage.CryptoOps.SigType;
import com.google.security.cryptauth.lib.securemessage.SecureMessageProto.Header;
import com.google.security.cryptauth.lib.securemessage.SecureMessageProto.HeaderAndBodyInternal;
import com.google.security.cryptauth.lib.securemessage.SecureMessageProto.SecureMessage;
import java.security.InvalidKeyException;
import java.security.Key;
import java.security.NoSuchAlgorithmException;
import java.security.SecureRandom;
import java.util.Arrays;
import javax.annotation.Nullable;

/**
 * Builder for {@link SecureMessage} protos. Can be used to create either signed messages,
 * or "signcrypted" (encrypted then signed) messages that include a tight binding between the
 * ciphertext portion and a verification key identity.
 *
 * @see SecureMessageParser
 */
public class SecureMessageBuilder {
  private ByteString publicMetadata;
  private ByteString verificationKeyId;
  private ByteString decryptionKeyId;
  /**
   * This data is never sent inside the protobufs, so the builder just saves it as a byte[].
   */
  private byte[] associatedData;

  private SecureRandom rng;

  public SecureMessageBuilder() {
    reset();
    this.rng = new SecureRandom();
  }

  /**
   * Resets this {@link SecureMessageBuilder} instance to a blank configuration (and returns it).
   */
  public SecureMessageBuilder reset() {
    this.publicMetadata = null;
    this.verificationKeyId = null;
    this.decryptionKeyId = null;
    this.associatedData = null;
    return this;
  }

  /**
   * Optional metadata to be sent along with the header information in this {@link SecureMessage}.
   * <p>
   * Note that this value will be sent <em>UNENCRYPTED</em> in all cases.
   * <p>
   * Can be used with either cleartext or signcrypted messages, but is intended primarily for use
   * with signcrypted messages.
   */
  public SecureMessageBuilder setPublicMetadata(byte[] publicMetadata) {
    this.publicMetadata = ByteString.copyFrom(publicMetadata);
    return this;
  }

  /**
   * The recipient of the {@link SecureMessage} should be able to uniquely determine the correct
   * verification key, given only this value.
   * <p>
   * Can be used with either cleartext or signcrypted messages. Setting this is mandatory for
   * signcrypted messages using a public key {@link SigType}, in order to bind the encrypted
   * body to a specific verification key.
   * <p>
   * Note that this value is sent <em>UNENCRYPTED</em> in all cases.
   */
  public SecureMessageBuilder setVerificationKeyId(byte[] verificationKeyId) {
    this.verificationKeyId = ByteString.copyFrom(verificationKeyId);
    return this;
  }

  /**
   * To be used only with {@link #buildSignCryptedMessage(Key, SigType, Key, EncType, byte[])},
   * this value is sent <em>UNENCRYPTED</em> as part of the header. It should be used by the
   * recipient of the {@link SecureMessage} to identify an appropriate key to use for decrypting
   * the message body.
   */
  public SecureMessageBuilder setDecryptionKeyId(byte[] decryptionKeyId) {
    this.decryptionKeyId = ByteString.copyFrom(decryptionKeyId);
    return this;
  }

  /**
   * Additional data is "associated" with this {@link SecureMessage}, but will not be sent as
   * part of it. The recipient of the {@link SecureMessage} will need to provide the same data in
   * order to verify the message body. Setting this to {@code null} is equivalent to using an
   * empty array (unlike the behavior of {@code VerificationKeyId} and {@code DecryptionKeyId}).
   * <p>
   * Note that the <em>size</em> (length in bytes) of the associated data will be sent in the
   * <em>UNENCRYPTED</em> header information, even if you are using encryption.
   * <p>
   * If you will be using {@link #buildSignedCleartextMessage(Key, SigType, byte[])}, then anyone
   * observing the {@link SecureMessage} may be able to infer this associated data via an
   * "offline dictionary attack". That is, when no encryption is used, you will not be hiding this
   * data simply because it is not being sent over the wire.
   */
  public SecureMessageBuilder setAssociatedData(@Nullable byte[] associatedData) {
   this.associatedData = associatedData;
   return this;
  }

  // @VisibleForTesting
  SecureMessageBuilder setRng(SecureRandom rng) {
    this.rng = rng;
    return this;
  }

  /**
   * Generates a signed {@link SecureMessage} with the payload {@code body} left
   * <em>UNENCRYPTED</em>.
   *
   * <p>Note that if you have used {@link #setAssociatedData(byte[])}, the associated data will
   * be subject to offline dictionary attacks if you use a public key {@link SigType}.
   *
   * <p>Doesn't currently support symmetric keys stored in a TPM (since we access the raw key).
   *
   * @see SecureMessageParser#parseSignedCleartextMessage(SecureMessage, Key, SigType)
   */
  public SecureMessage buildSignedCleartextMessage(Key signingKey, SigType sigType, byte[] body)
          throws NoSuchAlgorithmException, InvalidKeyException {
    if ((signingKey == null) || (sigType == null) || (body == null)) {
      throw new NullPointerException();
    }
    if (decryptionKeyId != null) {
      throw new IllegalStateException("Cannot set decryptionKeyId for a cleartext message");
    }

    byte[] headerAndBody = serializeHeaderAndBody(
        buildHeader(sigType, EncType.NONE, null).toByteArray(), body);
    return createSignedResult(signingKey, sigType, headerAndBody, associatedData);
  }

  /**
   * Generates a signed and encrypted {@link SecureMessage}. If the signature type requires a public
   * key, such as with ECDSA_P256_SHA256, then the caller <em>must</em> set a verification id using
   * the {@link #setVerificationKeyId(byte[])} method. The verification key id will be bound to the
   * encrypted {@code body}, preventing attacks that involve stripping the signature and then
   * re-signing the encrypted {@code body} as if it was originally sent by the attacker.
   *
   * <p>
   * It is safe to re-use one {@link javax.crypto.SecretKey} as both {@code signingKey} and
   * {@code encryptionKey}, even if that key is also used for
   * {@link #buildSignedCleartextMessage(Key, SigType, byte[])}. In fact, the resulting output
   * encoding will be more compact when the same symmetric key is used for both.
   *
   * <p>
   * Note that PublicMetadata and other header fields are left <em>UNENCRYPTED</em>.
   *
   * <p>
   * Doesn't currently support symmetric keys stored in a TPM (since we access the raw key).
   *
   * @param encType <em>must not</em> be set to {@link EncType#NONE}
   * @see SecureMessageParser#parseSignCryptedMessage(SecureMessage, Key, SigType, Key, EncType)
   */
  public SecureMessage buildSignCryptedMessage(
      Key signingKey, SigType sigType, Key encryptionKey, EncType encType, byte[] body)
          throws NoSuchAlgorithmException, InvalidKeyException {
    if ((signingKey == null)
        || (sigType == null)
        || (encryptionKey == null)
        || (encType == null)
        || (body == null)) {
      throw new NullPointerException();
    }
    if (encType == EncType.NONE) {
      throw new IllegalArgumentException(encType + " not supported for encrypted messages");
    }
    if (sigType.isPublicKeyScheme() && (verificationKeyId == null)) {
      throw new IllegalStateException(
          "Must set a verificationKeyId when using public key signature with encryption");
    }

    byte[] iv = CryptoOps.generateIv(encType, rng);
    byte[] header = buildHeader(sigType, encType, iv).toByteArray();

    // We may or may not need an extra tag in front of the plaintext body
    byte[] taggedBody;
    // We will only sign the associated data when we don't tag the plaintext body
    byte[] associatedDataToBeSigned;
    if (taggedPlaintextRequired(signingKey, sigType, encryptionKey)) {
      // Place a "tag" in front of the the plaintext message containing a digest of the header
      taggedBody = CryptoOps.concat(
          // Digest the header + any associated data, yielding a tag to be encrypted with the body.
          CryptoOps.digest(CryptoOps.concat(header, associatedData)),
          body);
      associatedDataToBeSigned = null; // We already handled any associatedData via the tag
    } else {
      taggedBody = body;
      associatedDataToBeSigned = associatedData;
    }

    // Compute the encrypted body, which binds the tag to the message inside the ciphertext
    byte[] encryptedBody = CryptoOps.encrypt(encryptionKey, encType, rng, iv, taggedBody);

    byte[] headerAndBody = serializeHeaderAndBody(header, encryptedBody);
    return createSignedResult(signingKey, sigType, headerAndBody, associatedDataToBeSigned);
  }

  /**
   * Indicates whether a "tag" is needed next to the plaintext body inside the ciphertext, to
   * prevent the same ciphertext from being reused with someone else's signature on it.
   */
  static boolean taggedPlaintextRequired(Key signingKey, SigType sigType, Key encryptionKey) {
    // We need a tag if different keys are being used to "sign" vs. encrypt
    return sigType.isPublicKeyScheme()
        || !Arrays.equals(signingKey.getEncoded(), encryptionKey.getEncoded());
  }

  /**
   * @param iv IV or {@code null} if IV to be left unset in the Header
   */
  private Header buildHeader(SigType sigType, EncType encType, byte[] iv) {
    Header.Builder result = Header.newBuilder()
        .setSignatureScheme(sigType.getSigScheme())
        .setEncryptionScheme(encType.getEncScheme());
    if (verificationKeyId != null) {
      result.setVerificationKeyId(verificationKeyId);
    }
    if (decryptionKeyId != null) {
      result.setDecryptionKeyId(decryptionKeyId);
    }
    if (publicMetadata != null) {
      result.setPublicMetadata(publicMetadata);
    }
    if (associatedData != null) {
      result.setAssociatedDataLength(associatedData.length);
    }
    if (iv != null) {
      result.setIv(ByteString.copyFrom(iv));
    }
    return result.build();
  }

  /**
   * @param header a serialized representation of a {@link Header}
   * @param body arbitrary payload data
   * @return a serialized representation of a {@link SecureMessageProto.HeaderAndBody}
   */
  private byte[] serializeHeaderAndBody(byte[] header, byte[] body) {
    return HeaderAndBodyInternal.newBuilder()
        .setHeader(ByteString.copyFrom(header))
        .setBody(ByteString.copyFrom(body))
        .build()
        .toByteArray();
  }

  private SecureMessage createSignedResult(
      Key signingKey, SigType sigType, byte[] headerAndBody, @Nullable byte[] associatedData)
      throws NoSuchAlgorithmException, InvalidKeyException {
    byte[] sig =
        CryptoOps.sign(sigType, signingKey, rng, CryptoOps.concat(headerAndBody, associatedData));
    return SecureMessage.newBuilder()
        .setHeaderAndBody(ByteString.copyFrom(headerAndBody))
        .setSignature(ByteString.copyFrom(sig))
        .build();
  }
}
