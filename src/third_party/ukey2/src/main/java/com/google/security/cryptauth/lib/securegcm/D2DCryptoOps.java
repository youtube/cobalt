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

import com.google.common.annotations.VisibleForTesting;
import com.google.protobuf.InvalidProtocolBufferException;
import com.google.security.cryptauth.lib.securegcm.DeviceToDeviceMessagesProto.DeviceToDeviceMessage;
import com.google.security.cryptauth.lib.securegcm.DeviceToDeviceMessagesProto.ResponderHello;
import com.google.security.cryptauth.lib.securegcm.SecureGcmProto.GcmMetadata;
import com.google.security.cryptauth.lib.securegcm.TransportCryptoOps.Payload;
import com.google.security.cryptauth.lib.securegcm.TransportCryptoOps.PayloadType;
import com.google.security.cryptauth.lib.securemessage.CryptoOps;
import com.google.security.cryptauth.lib.securemessage.CryptoOps.EncType;
import com.google.security.cryptauth.lib.securemessage.CryptoOps.SigType;
import com.google.security.cryptauth.lib.securemessage.PublicKeyProtoUtil;
import com.google.security.cryptauth.lib.securemessage.SecureMessageBuilder;
import com.google.security.cryptauth.lib.securemessage.SecureMessageParser;
import com.google.security.cryptauth.lib.securemessage.SecureMessageProto.GenericPublicKey;
import com.google.security.cryptauth.lib.securemessage.SecureMessageProto.Header;
import com.google.security.cryptauth.lib.securemessage.SecureMessageProto.HeaderAndBody;
import com.google.security.cryptauth.lib.securemessage.SecureMessageProto.SecureMessage;
import java.security.InvalidKeyException;
import java.security.NoSuchAlgorithmException;
import java.security.PrivateKey;
import java.security.PublicKey;
import java.security.SignatureException;
import java.security.spec.InvalidKeySpecException;
import javax.annotation.Nullable;
import javax.crypto.SecretKey;

/**
 * A collection of static utility methods used by {@link D2DHandshakeContext} for the Device to
 * Device communication (D2D) library.
 */
class D2DCryptoOps {
  // SHA256 of "D2D"
  // package-private
  static final byte[] SALT = new byte[] {
    (byte) 0x82, (byte) 0xAA, (byte) 0x55, (byte) 0xA0, (byte) 0xD3, (byte) 0x97, (byte) 0xF8,
    (byte) 0x83, (byte) 0x46, (byte) 0xCA, (byte) 0x1C, (byte) 0xEE, (byte) 0x8D, (byte) 0x39,
    (byte) 0x09, (byte) 0xB9, (byte) 0x5F, (byte) 0x13, (byte) 0xFA, (byte) 0x7D, (byte) 0xEB,
    (byte) 0x1D, (byte) 0x4A, (byte) 0xB3, (byte) 0x83, (byte) 0x76, (byte) 0xB8, (byte) 0x25,
    (byte) 0x6D, (byte) 0xA8, (byte) 0x55, (byte) 0x10
  };

  // Don't instantiate
  private D2DCryptoOps() { }

  /**
   * Used by the responder device to create a signcrypted message that contains
   * a payload and a {@link ResponderHello}.
   *
   * @param sharedKey used to signcrypt the {@link Payload}
   * @param publicDhKey the key the recipient will need to derive the shared DH secret.
   *   This key will be added to the {@link ResponderHello} in the header.
   * @param protocolVersion the protocol version to include in the proto
   */
  static byte[] signcryptMessageAndResponderHello(
      Payload payload, SecretKey sharedKey, PublicKey publicDhKey, int protocolVersion)
      throws InvalidKeyException, NoSuchAlgorithmException {
    ResponderHello.Builder responderHello = ResponderHello.newBuilder();
    responderHello.setPublicDhKey(PublicKeyProtoUtil.encodePublicKey(publicDhKey));
    responderHello.setProtocolVersion(protocolVersion);
    return signcryptPayload(payload, sharedKey, responderHello.build().toByteArray());
  }

  /**
   * Used by a device to send a secure {@link Payload} to another device.
   */
  static byte[] signcryptPayload(
      Payload payload, SecretKey masterKey)
      throws InvalidKeyException, NoSuchAlgorithmException {
    return signcryptPayload(payload, masterKey, null);
  }

  /**
   * Used by a device to send a secure {@link Payload} to another device.
   *
   * @param responderHello is an optional public value to attach in the header of
   *     the {@link SecureMessage} (in the DecryptionKeyId).
   */
  @VisibleForTesting
  static byte[] signcryptPayload(
      Payload payload, SecretKey masterKey, @Nullable byte[] responderHello)
      throws InvalidKeyException, NoSuchAlgorithmException {
    if ((payload == null) || (masterKey == null)) {
      throw new NullPointerException();
    }

    SecureMessageBuilder secureMessageBuilder = new SecureMessageBuilder()
        .setPublicMetadata(GcmMetadata.newBuilder()
            .setType(payload.getPayloadType().getType())
            .setVersion(SecureGcmConstants.SECURE_GCM_VERSION)
            .build()
            .toByteArray());

    if (responderHello != null) {
      secureMessageBuilder.setDecryptionKeyId(responderHello);
    }

    return secureMessageBuilder.buildSignCryptedMessage(
            masterKey,
            SigType.HMAC_SHA256,
            masterKey,
            EncType.AES_256_CBC,
            payload.getMessage())
        .toByteArray();
  }

  /**
   * Extracts a ResponderHello proto from the header of a signcrypted message so that we
   * can derive the shared secret that was used to sign/encrypt the message.
   *
   * @return the {@link ResponderHello} embedded in the signcrypted message.
   */
  static ResponderHello parseAndValidateResponderHello(
      byte[] signcryptedMessageFromResponder) throws InvalidProtocolBufferException {
    if (signcryptedMessageFromResponder == null) {
      throw new NullPointerException();
    }
    SecureMessage secmsg = SecureMessage.parseFrom(signcryptedMessageFromResponder);
    Header messageHeader = SecureMessageParser.getUnverifiedHeader(secmsg);
    if (!messageHeader.hasDecryptionKeyId()) {
      // Maybe this should be a different exception type, because in general, it's legal for the
      // SecureMessage proto to not have the decryption key id, but it's illegal in this protocol.
      throw new InvalidProtocolBufferException("Missing decryption key id");
    }
    byte[] encodedResponderHello = messageHeader.getDecryptionKeyId().toByteArray();
    ResponderHello responderHello = ResponderHello.parseFrom(encodedResponderHello);
    if (!responderHello.hasPublicDhKey()) {
      throw new InvalidProtocolBufferException("Missing public key in responder hello");
    }
    return responderHello;
  }

  /**
   * Used by a device to recover a secure {@link Payload} sent by another device.
   */
  static Payload verifydecryptPayload(
      byte[] signcryptedMessage, SecretKey masterKey)
      throws SignatureException, InvalidKeyException, NoSuchAlgorithmException {
    if ((signcryptedMessage == null) || (masterKey == null)) {
      throw new NullPointerException();
    }
    try {
      SecureMessage secmsg = SecureMessage.parseFrom(signcryptedMessage);
      HeaderAndBody parsed = SecureMessageParser.parseSignCryptedMessage(
          secmsg,
          masterKey,
          SigType.HMAC_SHA256,
          masterKey,
          EncType.AES_256_CBC);
      if (!parsed.getHeader().hasPublicMetadata()) {
        throw new SignatureException("missing metadata");
      }
      GcmMetadata metadata = GcmMetadata.parseFrom(parsed.getHeader().getPublicMetadata());
      if (metadata.getVersion() > SecureGcmConstants.SECURE_GCM_VERSION) {
        throw new SignatureException("Unsupported protocol version");
      }
      return new Payload(PayloadType.valueOf(metadata.getType()), parsed.getBody().toByteArray());
    } catch (InvalidProtocolBufferException e) {
      throw new SignatureException(e);
    } catch (IllegalArgumentException e) {
      throw new SignatureException(e);
    }
  }

  /**
   * Used by the initiator device to derive the shared key from the {@link PrivateKey} in the
   * {@link D2DHandshakeContext} and the responder's {@link GenericPublicKey} (contained in the
   * {@link ResponderHello} proto).
   */
  static SecretKey deriveSharedKeyFromGenericPublicKey(
      PrivateKey ourPrivateKey, GenericPublicKey theirGenericPublicKey) throws SignatureException {
    try {
      PublicKey theirPublicKey = PublicKeyProtoUtil.parsePublicKey(theirGenericPublicKey);
      return EnrollmentCryptoOps.doKeyAgreement(ourPrivateKey, theirPublicKey);
    } catch (InvalidKeySpecException e) {
      throw new SignatureException(e);
    } catch (InvalidKeyException e) {
      throw new SignatureException(e);
    }
  }

  /**
   * Used to derive a distinct key for each initiator and responder.
   * 
   * @param masterKey the source key used to derive the new key.
   * @param purpose a string to make the new key different for each purpose.
   * @return the derived {@link SecretKey}.
   */
  static SecretKey deriveNewKeyForPurpose(SecretKey masterKey, String purpose)
      throws NoSuchAlgorithmException, InvalidKeyException {
    byte[] info = purpose.getBytes();
    return KeyEncoding.parseMasterKey(CryptoOps.hkdf(masterKey, SALT, info));
  }

  /**
   * Used by the initiator device to decrypt the first payload portion that was sent in the
   * {@code responderHelloAndPayload}, and extract the {@link DeviceToDeviceMessage} contained
   * within it. In order to decrypt, the {@code sharedKey} must first be derived.
   *
   * @see #deriveSharedKeyFromGenericPublicKey(PrivateKey, GenericPublicKey)
   */
  static DeviceToDeviceMessage decryptResponderHelloMessage(
      SecretKey sharedKey, byte[] responderHelloAndPayload) throws SignatureException {
    try {
      Payload payload = verifydecryptPayload(responderHelloAndPayload, sharedKey);
      if (!PayloadType.DEVICE_TO_DEVICE_RESPONDER_HELLO_PAYLOAD.equals(
          payload.getPayloadType())) {
        throw new SignatureException("wrong message type in responder hello");
      }
      return DeviceToDeviceMessage.parseFrom(payload.getMessage());
    } catch (InvalidProtocolBufferException e) {
      throw new SignatureException(e);
    } catch (InvalidKeyException e) {
      throw new SignatureException(e);
    } catch (NoSuchAlgorithmException e) {
      throw new SignatureException(e);
    }
  }
}
