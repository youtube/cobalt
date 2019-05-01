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
import com.google.security.cryptauth.lib.securegcm.DeviceToDeviceMessagesProto.DeviceToDeviceMessage;
import com.google.security.cryptauth.lib.securegcm.DeviceToDeviceMessagesProto.InitiatorHello;
import com.google.security.cryptauth.lib.securegcm.DeviceToDeviceMessagesProto.ResponderHello;
import com.google.security.cryptauth.lib.securegcm.TransportCryptoOps.Payload;
import com.google.security.cryptauth.lib.securegcm.TransportCryptoOps.PayloadType;
import com.google.security.cryptauth.lib.securemessage.PublicKeyProtoUtil;
import java.security.InvalidKeyException;
import java.security.KeyPair;
import java.security.NoSuchAlgorithmException;
import java.security.PublicKey;
import java.security.SignatureException;
import java.security.spec.InvalidKeySpecException;
import javax.crypto.SecretKey;

/**
 * Implements an unauthenticated EC Diffie Hellman Key Exchange Handshake
 * <p>
 * Initiator sends an InitiatorHello, which is a protobuf that contains a public key. Responder
 * sends a responder hello, which a signed and encrypted message containing a payload, and a public
 * key in the unencrypted header (payload is encrypted with the derived DH key).
 * <p>
 * Example Usage:
 * <pre>
 *    // initiator:
 *    D2DHandshakeContext initiatorHandshakeContext =
 *        D2DDiffieHellmanKeyExchangeHandshake.forInitiator();
 *    byte[] initiatorHello = initiatorHandshakeContext.getNextHandshakeMessage();
 *    // (send initiatorHello to responder)
 *
 *    // responder:
 *    D2DHandshakeContext responderHandshakeContext =
 *        D2DDiffieHellmanKeyExchangeHandshake.forResponder();
 *    responderHandshakeContext.parseHandshakeMessage(initiatorHello);
 *    byte[] responderHelloAndPayload = responderHandshakeContext.getNextHandshakeMessage(
 *        toBytes(RESPONDER_HELLO_MESSAGE));
 *    D2DConnectionContext responderCtx = responderHandshakeContext.toConnectionContext();
 *    // (send responderHelloAndPayload to initiator)
 *
 *    // initiator
 *    byte[] messageFromPayload =
 *        initiatorHandshakeContext.parseHandshakeMessage(responderHelloAndPayload);
 *    if (messageFromPayload.length > 0) {
 *      handle(messageFromPayload);
 *    }
 *
 *    D2DConnectionContext initiatorCtx = initiatorHandshakeContext.toConnectionContext();
 * </pre>
 */
public class D2DDiffieHellmanKeyExchangeHandshake implements D2DHandshakeContext {
  // Data passed to hkdf to create the key used by the initiator to encode messages.
  private static final String INITIATOR_PURPOSE = "initiator";
  // Data passed to hkdf to create the key used by the responder to encode messages.
  private static final String RESPONDER_PURPOSE = "responder";

  private KeyPair ourKeyPair;
  private PublicKey theirPublicKey;
  private SecretKey initiatorEncodeKey;
  private SecretKey responderEncodeKey;
  private State handshakeState;
  private boolean isInitiator;
  private int protocolVersionToUse;

  private enum State {
    // Initiator state
    INITIATOR_START,
    INITIATOR_WAITING_FOR_RESPONDER_HELLO,

    // Responder state
    RESPONDER_START,
    RESPONDER_AFTER_INITIATOR_HELLO,

    // Common completion state
    HANDSHAKE_FINISHED,
    HANDSHAKE_ALREADY_USED
  }

  private D2DDiffieHellmanKeyExchangeHandshake(State state) {
    ourKeyPair = PublicKeyProtoUtil.generateEcP256KeyPair();
    theirPublicKey = null;
    initiatorEncodeKey = null;
    responderEncodeKey = null;
    handshakeState = state;
    isInitiator = state == State.INITIATOR_START;
    protocolVersionToUse = D2DConnectionContextV1.PROTOCOL_VERSION;
  }

  /**
   * Creates a new Diffie Hellman handshake context for the handshake initiator
   */
  public static D2DDiffieHellmanKeyExchangeHandshake forInitiator() {
    return new D2DDiffieHellmanKeyExchangeHandshake(State.INITIATOR_START);
  }

  /**
   * Creates a new Diffie Hellman handshake context for the handshake responder
   */
  public static D2DDiffieHellmanKeyExchangeHandshake forResponder() {
    return new D2DDiffieHellmanKeyExchangeHandshake(State.RESPONDER_START);
  }

  @Override
  public boolean isHandshakeComplete() {
    return handshakeState == State.HANDSHAKE_FINISHED
        || handshakeState == State.HANDSHAKE_ALREADY_USED;
  }

  @Override
  public byte[] getNextHandshakeMessage() throws HandshakeException {
    switch(handshakeState) {
      case INITIATOR_START:
        handshakeState = State.INITIATOR_WAITING_FOR_RESPONDER_HELLO;
        return InitiatorHello.newBuilder()
            .setPublicDhKey(PublicKeyProtoUtil.encodePublicKey(ourKeyPair.getPublic()))
            .setProtocolVersion(protocolVersionToUse)
            .build()
            .toByteArray();

      case RESPONDER_AFTER_INITIATOR_HELLO:
        byte[] responderHello = makeResponderHelloWithPayload(new byte[0]);
        handshakeState = State.HANDSHAKE_FINISHED;
        return responderHello;

      default:
        throw new HandshakeException("Cannot get next message in state: " + handshakeState);
    }
  }

  @Override
  public boolean canSendPayloadInHandshakeMessage() {
    return handshakeState == State.RESPONDER_AFTER_INITIATOR_HELLO;
  }

  @Override
  public byte[] getNextHandshakeMessage(byte[] payload) throws HandshakeException {
    if (handshakeState != State.RESPONDER_AFTER_INITIATOR_HELLO) {
      throw new HandshakeException(
          "Cannot get next message with payload in state: " + handshakeState);
    }

    byte[] responderHello = makeResponderHelloWithPayload(payload);
    handshakeState = State.HANDSHAKE_FINISHED;

    return responderHello;
  }

  private byte[] makeResponderHelloWithPayload(byte[] payload) throws HandshakeException {
    if (payload == null) {
      throw new HandshakeException("Not expecting null payload");
    }

    try {
      SecretKey masterKey =
          EnrollmentCryptoOps.doKeyAgreement(ourKeyPair.getPrivate(), theirPublicKey);

      // V0 uses the same key for encoding and decoding, but V1 uses separate keys.
      switch (protocolVersionToUse) {
        case D2DConnectionContextV0.PROTOCOL_VERSION:
          initiatorEncodeKey = masterKey;
          responderEncodeKey = masterKey;
          break;
        case D2DConnectionContextV1.PROTOCOL_VERSION:
          initiatorEncodeKey = D2DCryptoOps.deriveNewKeyForPurpose(masterKey, INITIATOR_PURPOSE);
          responderEncodeKey = D2DCryptoOps.deriveNewKeyForPurpose(masterKey, RESPONDER_PURPOSE);
          break;
        default:
          throw new IllegalStateException("Unexpected protocol version: " + protocolVersionToUse);
      }

      DeviceToDeviceMessage deviceToDeviceMessage =
          D2DConnectionContext.createDeviceToDeviceMessage(payload, 1 /* sequence number */);

      return D2DCryptoOps.signcryptMessageAndResponderHello(
          new Payload(PayloadType.DEVICE_TO_DEVICE_RESPONDER_HELLO_PAYLOAD,
              deviceToDeviceMessage.toByteArray()),
          responderEncodeKey,
          ourKeyPair.getPublic(),
          protocolVersionToUse);
    } catch (InvalidKeyException|NoSuchAlgorithmException e) {
      throw new HandshakeException(e);
    }
  }

  @Override
  public byte[] parseHandshakeMessage(byte[] handshakeMessage) throws HandshakeException {
    if (handshakeMessage == null || handshakeMessage.length == 0) {
      throw new HandshakeException("Handshake message too short");
    }

    switch(handshakeState) {
      case INITIATOR_WAITING_FOR_RESPONDER_HELLO:
          byte[] payload = parseResponderHello(handshakeMessage);
          handshakeState = State.HANDSHAKE_FINISHED;
          return payload;

      case RESPONDER_START:
          parseInitiatorHello(handshakeMessage);
          handshakeState = State.RESPONDER_AFTER_INITIATOR_HELLO;
          return new byte[0];

      default:
        throw new HandshakeException("Cannot parse message in state: " + handshakeState);
    }
  }

  private byte[] parseResponderHello(byte[] responderHello) throws HandshakeException {
     try {
        ResponderHello responderHelloProto =
            D2DCryptoOps.parseAndValidateResponderHello(responderHello);

        // Downgrade to protocol version 0 if needed for backwards compatibility.
        int protocolVersion = responderHelloProto.getProtocolVersion();
        if (protocolVersion == D2DConnectionContextV0.PROTOCOL_VERSION) {
          protocolVersionToUse = D2DConnectionContextV0.PROTOCOL_VERSION;
        }

        SecretKey masterKey = D2DCryptoOps.deriveSharedKeyFromGenericPublicKey(
            ourKeyPair.getPrivate(), responderHelloProto.getPublicDhKey());

        // V0 uses the same key for encoding and decoding, but V1 uses separate keys.
        if (protocolVersionToUse == D2DConnectionContextV0.PROTOCOL_VERSION) {
          initiatorEncodeKey = masterKey;
          responderEncodeKey = masterKey;
        } else {
          initiatorEncodeKey = D2DCryptoOps.deriveNewKeyForPurpose(masterKey, INITIATOR_PURPOSE);
          responderEncodeKey = D2DCryptoOps.deriveNewKeyForPurpose(masterKey, RESPONDER_PURPOSE);
        }

        DeviceToDeviceMessage message =
            D2DCryptoOps.decryptResponderHelloMessage(responderEncodeKey, responderHello);

        if (message.getSequenceNumber() != 1) {
          throw new HandshakeException("Incorrect sequence number in responder hello");
        }

        return message.getMessage().toByteArray();
      } catch (SignatureException | InvalidProtocolBufferException
               | NoSuchAlgorithmException | InvalidKeyException e) {
        throw new HandshakeException(e);
      }
  }

  private void parseInitiatorHello(byte[] initiatorHello) throws HandshakeException {
    try {
        InitiatorHello initiatorHelloProto = InitiatorHello.parseFrom(initiatorHello);

        if (!initiatorHelloProto.hasPublicDhKey()) {
          throw new HandshakeException("Missing public key in initiator hello");
        }

        theirPublicKey = PublicKeyProtoUtil.parsePublicKey(initiatorHelloProto.getPublicDhKey());

        // Downgrade to protocol version 0 if needed for backwards compatibility.
        int protocolVersion = initiatorHelloProto.getProtocolVersion();
        if (protocolVersion == D2DConnectionContextV0.PROTOCOL_VERSION) {
          protocolVersionToUse = D2DConnectionContextV0.PROTOCOL_VERSION;
        }
      } catch (InvalidKeySpecException | InvalidProtocolBufferException e) {
        throw new HandshakeException(e);
      }
  }

  @Override
  public D2DConnectionContext toConnectionContext() throws HandshakeException {
    if (handshakeState == State.HANDSHAKE_ALREADY_USED) {
      throw new HandshakeException("Cannot reuse handshake context; is has already been used");
    }

    if (!isHandshakeComplete()) {
      throw new HandshakeException("Handshake is not complete; cannot create connection context");
    }

    handshakeState = State.HANDSHAKE_ALREADY_USED;

    if (protocolVersionToUse == D2DConnectionContextV0.PROTOCOL_VERSION) {
      // Both sides start with an initial sequence number of 1 because the last message of the
      // handshake had an optional payload with sequence number 1.  D2DConnectionContext remembers
      // the last sequence number used by each side.
      // Note: initiatorEncodeKey == responderEncodeKey
      return new D2DConnectionContextV0(initiatorEncodeKey, 1 /** initialSequenceNumber */);
    } else {
      SecretKey encodeKey = isInitiator ? initiatorEncodeKey : responderEncodeKey;
      SecretKey decodeKey = isInitiator ? responderEncodeKey : initiatorEncodeKey;
      // Only the responder sends a DeviceToDeviceMessage during the handshake, so it has an initial
      // sequence number of 1.  The initiator will therefore have an initial sequence number of 0.
      int initialEncodeSequenceNumber = isInitiator ? 0 : 1;
      int initialDecodeSequenceNumber = isInitiator ? 1 : 0;
      return new D2DConnectionContextV1(
          encodeKey, decodeKey, initialEncodeSequenceNumber, initialDecodeSequenceNumber);
    }
  }
}
