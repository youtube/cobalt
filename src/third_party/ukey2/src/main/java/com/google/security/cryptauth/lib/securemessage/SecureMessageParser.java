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
import com.google.protobuf.InvalidProtocolBufferException;
import com.google.security.cryptauth.lib.securemessage.CryptoOps.EncType;
import com.google.security.cryptauth.lib.securemessage.CryptoOps.SigType;
import com.google.security.cryptauth.lib.securemessage.SecureMessageProto.Header;
import com.google.security.cryptauth.lib.securemessage.SecureMessageProto.HeaderAndBody;
import com.google.security.cryptauth.lib.securemessage.SecureMessageProto.HeaderAndBodyInternal;
import com.google.security.cryptauth.lib.securemessage.SecureMessageProto.SecureMessage;
import java.security.InvalidAlgorithmParameterException;
import java.security.InvalidKeyException;
import java.security.Key;
import java.security.NoSuchAlgorithmException;
import java.security.SignatureException;
import javax.annotation.Nullable;
import javax.crypto.BadPaddingException;
import javax.crypto.IllegalBlockSizeException;

/**
 * Utility class to parse and verify {@link SecureMessage} protos. Verifies the signature on the
 * message, and decrypts "signcrypted" messages (while simultaneously verifying the signature).
 *
 * @see SecureMessageBuilder
 */
public class SecureMessageParser {

  private SecureMessageParser() {}  // Do not instantiate

  /**
   * Extracts the {@link Header} component from a {@link SecureMessage} but <em>DOES NOT VERIFY</em>
   * the signature when doing so. Callers should not trust the resulting output until after a
   * subsequent {@code parse*()} call has succeeded.
   *
   * <p>The intention is to allow the caller to determine the type of the protocol message and which
   * keys are in use, prior to attempting to verify (and possibly decrypt) the payload body.
   */
  public static Header getUnverifiedHeader(SecureMessage secmsg)
      throws InvalidProtocolBufferException {
    if (!secmsg.hasHeaderAndBody()) {
      throw new InvalidProtocolBufferException("Missing header and body");
    }
    if (!HeaderAndBody.parseFrom(secmsg.getHeaderAndBody()).hasHeader()) {
      throw new InvalidProtocolBufferException("Missing header");
    }
    Header result = HeaderAndBody.parseFrom(secmsg.getHeaderAndBody()).getHeader();
    // Check that at least a signature scheme was set
    if (!result.hasSignatureScheme()) {
      throw new InvalidProtocolBufferException("Missing header field(s)");
    }
    // Check signature scheme is legal
    try {
      SigType.valueOf(result.getSignatureScheme());
    } catch (IllegalArgumentException e) {
      throw new InvalidProtocolBufferException("Corrupt/unsupported SignatureScheme");
    }
    // Check encryption scheme is legal
    if (result.hasEncryptionScheme()) {
      try {
        EncType.valueOf(result.getEncryptionScheme());
      } catch (IllegalArgumentException e) {
        throw new InvalidProtocolBufferException("Corrupt/unsupported EncryptionScheme");
      }
    }
    return result;
  }

  /**
   * Parses a {@link SecureMessage} containing a cleartext payload body, and verifies the signature.
   *
   * @return the parsed {@link HeaderAndBody} pair (which is fully verified)
   * @throws SignatureException if signature verification fails
   * @see SecureMessageBuilder#buildSignedCleartextMessage(Key, SigType, byte[])
   */
  public static HeaderAndBody parseSignedCleartextMessage(
      SecureMessage secmsg, Key verificationKey, SigType sigType)
          throws NoSuchAlgorithmException, InvalidKeyException, SignatureException {
    return parseSignedCleartextMessage(secmsg, verificationKey, sigType, null);
  }

  /**
   * Parses a {@link SecureMessage} containing a cleartext payload body, and verifies the signature.
   *
   * @param associatedData optional associated data bound to the signature (but not in the message)
   * @return the parsed {@link HeaderAndBody} pair (which is fully verified)
   * @throws SignatureException if signature verification fails
   * @see SecureMessageBuilder#buildSignedCleartextMessage(Key, SigType, byte[])
   */
  public static HeaderAndBody parseSignedCleartextMessage(
      SecureMessage secmsg, Key verificationKey, SigType sigType, @Nullable byte[] associatedData)
          throws NoSuchAlgorithmException, InvalidKeyException, SignatureException {
    if ((secmsg == null) || (verificationKey == null) || (sigType == null)) {
      throw new NullPointerException();
    }
    return verifyHeaderAndBody(
        secmsg,
        verificationKey,
        sigType,
        EncType.NONE,
        associatedData,
        false /* suppressAssociatedData is always false for signed cleartext */);
  }

  /**
   * Parses a {@link SecureMessage} containing an encrypted payload body, extracting a decryption of
   * the payload body and verifying the signature.
   *
   * @return the parsed {@link HeaderAndBody} pair (which is fully verified and decrypted)
   * @throws SignatureException if signature verification fails
   * @see SecureMessageBuilder#buildSignCryptedMessage(Key, SigType, Key, EncType, byte[])
   */
  public static HeaderAndBody parseSignCryptedMessage(
      SecureMessage secmsg,
      Key verificationKey,
      SigType sigType,
      Key decryptionKey,
      EncType encType)
    throws InvalidKeyException, NoSuchAlgorithmException, SignatureException {
    return parseSignCryptedMessage(secmsg, verificationKey, sigType, decryptionKey, encType, null);
  }

  /**
   * Parses a {@link SecureMessage} containing an encrypted payload body, extracting a decryption of
   * the payload body and verifying the signature.
   *
   * @param associatedData optional associated data bound to the signature (but not in the message)
   * @return the parsed {@link HeaderAndBody} pair (which is fully verified and decrypted)
   * @throws SignatureException if signature verification fails
   * @see SecureMessageBuilder#buildSignCryptedMessage(Key, SigType, Key, EncType, byte[])
   */
  public static HeaderAndBody parseSignCryptedMessage(
      SecureMessage secmsg,
      Key verificationKey,
      SigType sigType,
      Key decryptionKey,
      EncType encType,
      @Nullable byte[] associatedData)
          throws InvalidKeyException, NoSuchAlgorithmException, SignatureException {
    if ((secmsg == null)
        || (verificationKey == null)
        || (sigType == null)
        || (decryptionKey == null)
        || (encType == null)) {
      throw new NullPointerException();
    }
    if (encType == EncType.NONE) {
      throw new SignatureException("Not a signcrypted message");
    }

    boolean tagRequired =
        SecureMessageBuilder.taggedPlaintextRequired(verificationKey, sigType, decryptionKey);
    HeaderAndBody headerAndEncryptedBody;
    headerAndEncryptedBody = verifyHeaderAndBody(
        secmsg,
        verificationKey,
        sigType,
        encType,
        associatedData,
        tagRequired /* suppressAssociatedData if it is handled by the tag instead */);

    byte[] rawDecryptedBody;
    Header header = headerAndEncryptedBody.getHeader();
    if (!header.hasIv()) {
      throw new SignatureException();
    }
    try {
      rawDecryptedBody = CryptoOps.decrypt(
          decryptionKey, encType, header.getIv().toByteArray(),
          headerAndEncryptedBody.getBody().toByteArray());
    } catch (InvalidAlgorithmParameterException e) {
      throw new SignatureException();
    } catch (IllegalBlockSizeException e) {
      throw new SignatureException();
    } catch (BadPaddingException e) {
      throw new SignatureException();
    }

    if (!tagRequired) {
      // No tag expected, so we're all done
      return HeaderAndBody.newBuilder(headerAndEncryptedBody)
          .setBody(ByteString.copyFrom(rawDecryptedBody))
          .build();
    }

    // Verify the tag that binds the ciphertext to the header, and remove it
    byte[] headerBytes;
    try {
      headerBytes =
          HeaderAndBodyInternal.parseFrom(secmsg.getHeaderAndBody()).getHeader().toByteArray();
    } catch (InvalidProtocolBufferException e) {
      // This shouldn't happen, but throw it up just in case
      throw new SignatureException(e);
    }
    boolean verifiedBinding = false;
    byte[] expectedTag = CryptoOps.digest(CryptoOps.concat(headerBytes, associatedData));
    if (rawDecryptedBody.length >= CryptoOps.DIGEST_LENGTH) {
      byte[] actualTag = CryptoOps.subarray(rawDecryptedBody, 0, CryptoOps.DIGEST_LENGTH);
      if (CryptoOps.constantTimeArrayEquals(actualTag, expectedTag)) {
        verifiedBinding = true;
      }
    }
    if (!verifiedBinding) {
      throw new SignatureException();
    }

    int bodyLen = rawDecryptedBody.length - CryptoOps.DIGEST_LENGTH;
    return HeaderAndBody.newBuilder(headerAndEncryptedBody)
        // Remove the tag and set the plaintext body
        .setBody(ByteString.copyFrom(rawDecryptedBody, CryptoOps.DIGEST_LENGTH, bodyLen))
        .build();
  }

  private static HeaderAndBody verifyHeaderAndBody(
      SecureMessage secmsg,
      Key verificationKey,
      SigType sigType,
      EncType encType,
      @Nullable byte[] associatedData,
      boolean suppressAssociatedData /* in case it is in the tag instead */)
      throws NoSuchAlgorithmException, InvalidKeyException, SignatureException {
    if (!secmsg.hasHeaderAndBody() || !secmsg.hasSignature()) {
      throw new SignatureException("Signature failed verification");
    }
    byte[] signature = secmsg.getSignature().toByteArray();
    byte[] data = secmsg.getHeaderAndBody().toByteArray();
    byte[] signedData = suppressAssociatedData ? data : CryptoOps.concat(data, associatedData);

    // Try not to leak the specific reason for verification failures, due to security concerns.
    boolean verified = CryptoOps.verify(verificationKey, sigType, signature, signedData);
    HeaderAndBody result = null;
    try {
      result = HeaderAndBody.parseFrom(secmsg.getHeaderAndBody());
      // Even if declared required, micro proto doesn't throw an exception if fields are not present
      if (!result.hasHeader() || !result.hasBody()) {
        throw new SignatureException("Signature failed verification");
      }
      verified &= (result.getHeader().getSignatureScheme() == sigType.getSigScheme());
      verified &= (result.getHeader().getEncryptionScheme() == encType.getEncScheme());
      // Check that either a decryption operation is expected, or no DecryptionKeyId is set.
      verified &= (encType != EncType.NONE) || !result.getHeader().hasDecryptionKeyId();
      // If encryption was used, check that either we are not using a public key signature or a
      // VerificationKeyId was set (as is required for public key based signature + encryption).
      verified &= (encType == EncType.NONE) || !sigType.isPublicKeyScheme() ||
          result.getHeader().hasVerificationKeyId();
      int associatedDataLength = associatedData == null ? 0 : associatedData.length;
      verified &= (result.getHeader().getAssociatedDataLength() == associatedDataLength);
    } catch (InvalidProtocolBufferException e) {
      verified = false;
    }

    if (verified) {
      return result;
    }
    throw new SignatureException("Signature failed verification");
  }
}
