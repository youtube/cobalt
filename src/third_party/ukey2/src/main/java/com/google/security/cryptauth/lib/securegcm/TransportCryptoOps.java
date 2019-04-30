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
import com.google.security.cryptauth.lib.securegcm.SecureGcmProto.GcmMetadata;
import com.google.security.cryptauth.lib.securemessage.CryptoOps.EncType;
import com.google.security.cryptauth.lib.securemessage.CryptoOps.SigType;
import com.google.security.cryptauth.lib.securemessage.SecureMessageBuilder;
import com.google.security.cryptauth.lib.securemessage.SecureMessageParser;
import com.google.security.cryptauth.lib.securemessage.SecureMessageProto.HeaderAndBody;
import com.google.security.cryptauth.lib.securemessage.SecureMessageProto.SecureMessage;
import java.security.InvalidKeyException;
import java.security.KeyPair;
import java.security.NoSuchAlgorithmException;
import java.security.PrivateKey;
import java.security.PublicKey;
import java.security.SignatureException;
import java.security.interfaces.ECPublicKey;
import java.security.interfaces.RSAPublicKey;
import javax.crypto.SecretKey;

/**
 * Utility class for implementing a secure transport for GCM messages.
 */
public class TransportCryptoOps {
  private TransportCryptoOps() {} // Do not instantiate

  /**
   * A type safe version of the {@link SecureGcmProto} {@code Type} codes.
   */
  public enum PayloadType {
    ENROLLMENT(SecureGcmProto.Type.ENROLLMENT),
    TICKLE(SecureGcmProto.Type.TICKLE),
    TX_REQUEST(SecureGcmProto.Type.TX_REQUEST),
    TX_REPLY(SecureGcmProto.Type.TX_REPLY),
    TX_SYNC_REQUEST(SecureGcmProto.Type.TX_SYNC_REQUEST),
    TX_SYNC_RESPONSE(SecureGcmProto.Type.TX_SYNC_RESPONSE),
    TX_PING(SecureGcmProto.Type.TX_PING),
    DEVICE_INFO_UPDATE(SecureGcmProto.Type.DEVICE_INFO_UPDATE),
    TX_CANCEL_REQUEST(SecureGcmProto.Type.TX_CANCEL_REQUEST),
    LOGIN_NOTIFICATION(SecureGcmProto.Type.LOGIN_NOTIFICATION),
    PROXIMITYAUTH_PAIRING(SecureGcmProto.Type.PROXIMITYAUTH_PAIRING),
    GCMV1_IDENTITY_ASSERTION(SecureGcmProto.Type.GCMV1_IDENTITY_ASSERTION),
    DEVICE_TO_DEVICE_RESPONDER_HELLO_PAYLOAD(
        SecureGcmProto.Type.DEVICE_TO_DEVICE_RESPONDER_HELLO_PAYLOAD),
    DEVICE_TO_DEVICE_MESSAGE(SecureGcmProto.Type.DEVICE_TO_DEVICE_MESSAGE),
    DEVICE_PROXIMITY_CALLBACK(SecureGcmProto.Type.DEVICE_PROXIMITY_CALLBACK),
    UNLOCK_KEY_SIGNED_CHALLENGE(SecureGcmProto.Type.UNLOCK_KEY_SIGNED_CHALLENGE);

    private final SecureGcmProto.Type type;
    PayloadType(SecureGcmProto.Type type) {
      this.type = type;
    }

    public SecureGcmProto.Type getType() {
      return this.type;
    }

    public static PayloadType valueOf(SecureGcmProto.Type type) {
      return PayloadType.valueOf(type.getNumber());
    }

    public static PayloadType valueOf(int type) {
      for (PayloadType payloadType : PayloadType.values()) {
        if (payloadType.getType().getNumber() == type) {
          return payloadType;
        }
      }
      throw new IllegalArgumentException("Unsupported payload type: " + type);
    }
  }

  /**
   * Encapsulates a {@link PayloadType} specifier, and a corresponding raw {@code message} payload.
   */
  public static class Payload {
    private final PayloadType payloadType;
    private final byte[] message;

    public Payload(PayloadType payloadType, byte[] message) {
      if ((payloadType == null) || (message == null)) {
        throw new NullPointerException();
      }
      this.payloadType = payloadType;
      this.message = message;
    }

    public PayloadType getPayloadType() {
      return payloadType;
    }

    public byte[] getMessage() {
      return message;
    }
  }

  /**
   * Used by the the server-side to send a secure {@link Payload} to the client.
   *
   * @param masterKey used to signcrypt the {@link Payload}
   * @param keyHandle the name by which the client refers to the specified {@code masterKey}
   */
  public static byte[] signcryptServerMessage(
      Payload payload, SecretKey masterKey, byte[] keyHandle)
      throws InvalidKeyException, NoSuchAlgorithmException {
    if ((payload == null) || (masterKey == null) || (keyHandle == null)) {
      throw new NullPointerException();
    }
    return new SecureMessageBuilder()
        .setVerificationKeyId(keyHandle)
        .setPublicMetadata(GcmMetadata.newBuilder()
            .setType(payload.getPayloadType().getType())
            .setVersion(SecureGcmConstants.SECURE_GCM_VERSION)
            .build()
            .toByteArray())
        .buildSignCryptedMessage(
            masterKey,
            SigType.HMAC_SHA256,
            masterKey,
            EncType.AES_256_CBC,
            payload.getMessage())
        .toByteArray();
  }

  /**
   * Extracts the {@code keyHandle} from a {@code signcryptedMessage}.
   *
   * @see #signcryptServerMessage(Payload, SecretKey, byte[])
   */
  public static byte[] getKeyHandleFor(byte[] signcryptedServerMessage)
      throws InvalidProtocolBufferException  {
    if (signcryptedServerMessage == null) {
      throw new NullPointerException();
    }
    SecureMessage secmsg = SecureMessage.parseFrom(signcryptedServerMessage);
    return SecureMessageParser.getUnverifiedHeader(secmsg).getVerificationKeyId().toByteArray();
  }

  /**
   * Used by a client to recover a secure {@link Payload} sent by the server-side.
   *
   * @see #getKeyHandleFor(byte[])
   * @see #signcryptServerMessage(Payload, SecretKey, byte[])
   */
  public static Payload verifydecryptServerMessage(
      byte[] signcryptedServerMessage, SecretKey masterKey)
      throws SignatureException, InvalidKeyException, NoSuchAlgorithmException {
    if ((signcryptedServerMessage == null) || (masterKey == null)) {
      throw new NullPointerException();
    }
    try {
      SecureMessage secmsg = SecureMessage.parseFrom(signcryptedServerMessage);
      HeaderAndBody parsed = SecureMessageParser.parseSignCryptedMessage(
          secmsg,
          masterKey,
          SigType.HMAC_SHA256,
          masterKey,
          EncType.AES_256_CBC);
      GcmMetadata metadata = GcmMetadata.parseFrom(parsed.getHeader().getPublicMetadata());
      if (metadata.getVersion() > SecureGcmConstants.SECURE_GCM_VERSION) {
        throw new SignatureException("Unsupported protocol version");
      }
      return new Payload(PayloadType.valueOf(metadata.getType()), parsed.getBody().toByteArray());
    } catch (InvalidProtocolBufferException | IllegalArgumentException e) {
      throw new SignatureException(e);
    }
  }

  /**
   * Used by the the client-side to send a secure {@link Payload} to the client.
   *
   * @param userKeyPair used to sign the {@link Payload}. In particular, the {@link PrivateKey}
   *   portion is used for signing, and (the {@link PublicKey} portion is sent to the server.
   * @param masterKey used to encrypt the {@link Payload}
   */
  public static byte[] signcryptClientMessage(
      Payload payload, KeyPair userKeyPair, SecretKey masterKey)
      throws InvalidKeyException, NoSuchAlgorithmException {
    if ((payload == null) || (masterKey == null)) {
      throw new NullPointerException();
    }

    PublicKey userPublicKey = userKeyPair.getPublic();
    PrivateKey userPrivateKey = userKeyPair.getPrivate();

    return new SecureMessageBuilder()
        .setVerificationKeyId(KeyEncoding.encodeUserPublicKey(userPublicKey))
        .setPublicMetadata(GcmMetadata.newBuilder()
            .setType(payload.getPayloadType().getType())
            .setVersion(SecureGcmConstants.SECURE_GCM_VERSION)
            .build()
            .toByteArray())
        .buildSignCryptedMessage(
            userPrivateKey,
            getSigTypeFor(userPublicKey),
            masterKey,
            EncType.AES_256_CBC,
            payload.getMessage())
        .toByteArray();
  }

  /**
   * Used by the server-side to recover a secure {@link Payload} sent by a client.
   *
   * @see #getEncodedUserPublicKeyFor(byte[])
   * @see #signcryptClientMessage(Payload, KeyPair, SecretKey)
   */
  public static Payload verifydecryptClientMessage(
      byte[] signcryptedClientMessage, PublicKey userPublicKey, SecretKey masterKey)
      throws SignatureException, InvalidKeyException, NoSuchAlgorithmException {
    if ((signcryptedClientMessage == null) || (masterKey == null)) {
      throw new NullPointerException();
    }
    try {
      SecureMessage secmsg = SecureMessage.parseFrom(signcryptedClientMessage);
      HeaderAndBody parsed = SecureMessageParser.parseSignCryptedMessage(
          secmsg,
          userPublicKey,
          getSigTypeFor(userPublicKey),
          masterKey,
          EncType.AES_256_CBC);
      GcmMetadata metadata = GcmMetadata.parseFrom(parsed.getHeader().getPublicMetadata());
      if (metadata.getVersion() > SecureGcmConstants.SECURE_GCM_VERSION) {
        throw new SignatureException("Unsupported protocol version");
      }
      return new Payload(PayloadType.valueOf(metadata.getType()), parsed.getBody().toByteArray());
    } catch (InvalidProtocolBufferException | IllegalArgumentException e) {
      throw new SignatureException(e);
    }
  }

  /**
   * Extracts an encoded {@code userPublicKey} from a {@code signcryptedClientMessage}.
   *
   * @see #signcryptClientMessage(Payload, KeyPair, SecretKey)
   */
  public static byte[] getEncodedUserPublicKeyFor(byte[] signcryptedClientMessage)
      throws InvalidProtocolBufferException  {
    if (signcryptedClientMessage == null) {
      throw new NullPointerException();
    }
    SecureMessage secmsg = SecureMessage.parseFrom(signcryptedClientMessage);
    return SecureMessageParser.getUnverifiedHeader(secmsg).getVerificationKeyId().toByteArray();
  }

  private static SigType getSigTypeFor(PublicKey userPublicKey) throws InvalidKeyException {
    if (userPublicKey instanceof ECPublicKey) {
      return SigType.ECDSA_P256_SHA256;
    } else if (userPublicKey instanceof RSAPublicKey) {
      return SigType.RSA2048_SHA256;
    } else {
      throw new InvalidKeyException("Unsupported key type");
    }
  }
}
