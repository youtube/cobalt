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
import com.google.protobuf.ByteString;
import com.google.protobuf.InvalidProtocolBufferException;
import com.google.security.cryptauth.lib.securegcm.DeviceToDeviceMessagesProto.DeviceToDeviceMessage;
import com.google.security.cryptauth.lib.securegcm.TransportCryptoOps.Payload;
import com.google.security.cryptauth.lib.securegcm.TransportCryptoOps.PayloadType;
import java.io.UnsupportedEncodingException;
import java.security.InvalidKeyException;
import java.security.NoSuchAlgorithmException;
import java.security.SignatureException;
import java.util.Arrays;
import javax.crypto.SecretKey;
import javax.crypto.spec.SecretKeySpec;

/**
 * The full context of a secure connection. This object has methods to encode and decode messages
 * that are to be sent to another device.
 *
 * Subclasses keep track of the keys shared with the other device, and of the sequence in which the
 * messages are expected.
 */
public abstract class D2DConnectionContext {
  private static final String UTF8 = "UTF-8";
  private final int protocolVersion;

  protected D2DConnectionContext(int protocolVersion) {
    this.protocolVersion = protocolVersion;
  }

  /**
   * @return the version of the D2D protocol.
   */
  public int getProtocolVersion() {
    return protocolVersion;
  }

  /**
   * Once initiator and responder have exchanged public keys, use this method to encrypt and
   * sign a payload. Both initiator and responder devices can use this message.
   *
   * @param payload the payload that should be encrypted.
   */
  public byte[] encodeMessageToPeer(byte[] payload) {
    incrementSequenceNumberForEncoding();
    DeviceToDeviceMessage message = createDeviceToDeviceMessage(
        payload, getSequenceNumberForEncoding());
    try {
      return D2DCryptoOps.signcryptPayload(
          new Payload(PayloadType.DEVICE_TO_DEVICE_MESSAGE,
              message.toByteArray()),
          getEncodeKey());
    } catch (InvalidKeyException e) {
      // should never happen, since we agreed on the key earlier
      throw new RuntimeException(e);
    } catch (NoSuchAlgorithmException e) {
      // should never happen
      throw new RuntimeException(e);
    }
  }

  /**
   * Encrypting/signing a string for transmission to another device.
   *
   * @see #encodeMessageToPeer(byte[])
   *
   * @param payload the payload that should be encrypted.
   */
  public byte[] encodeMessageToPeer(String payload) {
    try {
      return encodeMessageToPeer(payload.getBytes(UTF8));
    } catch (UnsupportedEncodingException e) {
      // Should never happen - we should always be able to UTF-8-encode a string
      throw new RuntimeException(e);
    }
  }

  /**
   * Once InitiatorHello and ResponderHello(AndPayload) are exchanged, use this method
   * to decrypt and verify a message received from the other device. Both initiator and
   * responder device can use this message.
   *
   * @param message the message that should be encrypted.
   * @throws SignatureException if the message from the remote peer did not pass verification
   */
  public byte[] decodeMessageFromPeer(byte[] message) throws SignatureException {
    try {
      Payload payload = D2DCryptoOps.verifydecryptPayload(message, getDecodeKey());
      if (!PayloadType.DEVICE_TO_DEVICE_MESSAGE.equals(payload.getPayloadType())) {
        throw new SignatureException("wrong message type in device-to-device message");
      }

      DeviceToDeviceMessage messageProto = DeviceToDeviceMessage.parseFrom(payload.getMessage());
      incrementSequenceNumberForDecoding();
      if (messageProto.getSequenceNumber() != getSequenceNumberForDecoding()) {
        throw new SignatureException("Incorrect sequence number");
      }

      return messageProto.getMessage().toByteArray();
    } catch (InvalidKeyException e) {
      throw new SignatureException(e);
    } catch (NoSuchAlgorithmException e) {
      // this shouldn't happen - the algorithms are hard-coded.
      throw new RuntimeException(e);
    } catch (InvalidProtocolBufferException e) {
      throw new SignatureException(e);
    }
  }

  /**
   * Once InitiatorHello and ResponderHello(AndPayload) are exchanged, use this method
   * to decrypt and verify a message received from the other device. Both initiator and
   * responder device can use this message.
   *
   * @param message the message that should be encrypted.
   */
  public String decodeMessageFromPeerAsString(byte[] message) throws SignatureException {
    try {
      return new String(decodeMessageFromPeer(message), UTF8);
    } catch (UnsupportedEncodingException e) {
      // Should never happen - we should always be able to UTF-8-encode a string
      throw new RuntimeException(e);
    }
  }

  // package-private
  static DeviceToDeviceMessage createDeviceToDeviceMessage(byte[] message, int sequenceNumber) {
    DeviceToDeviceMessage.Builder deviceToDeviceMessage = DeviceToDeviceMessage.newBuilder();
    deviceToDeviceMessage.setSequenceNumber(sequenceNumber);
    deviceToDeviceMessage.setMessage(ByteString.copyFrom(message));
    return deviceToDeviceMessage.build();
  }

  /**
   * Returns a cryptographic digest (SHA256) of the session keys prepended by the SHA256 hash
   * of the ASCII string "D2D"
   * @throws NoSuchAlgorithmException if SHA 256 doesn't exist on this platform
   */
  public abstract byte[] getSessionUnique() throws NoSuchAlgorithmException;

  /**
   * Increments the sequence number used for encoding messages.
   */
  protected abstract void incrementSequenceNumberForEncoding();

  /**
   * Increments the sequence number used for decoding messages.
   */
  protected abstract void incrementSequenceNumberForDecoding();

  /**
   * @return the last sequence number used to encode a message.
   */
  @VisibleForTesting
  abstract int getSequenceNumberForEncoding();

  /**
   * @return the last sequence number used to decode a message.
   */
  @VisibleForTesting
  abstract int getSequenceNumberForDecoding();

  /**
   * @return the {@link SecretKey} used for encoding messages.
   */
  @VisibleForTesting
  abstract SecretKey getEncodeKey();

  /**
   * @return the {@link SecretKey} used for decoding messages.
   */
  @VisibleForTesting
  abstract SecretKey getDecodeKey();

  /**
   * Creates a saved session that can later be used for resumption.  Note, this must be stored in a
   * secure location.
   *
   * @return the saved session, suitable for resumption.
   */
  public abstract byte[] saveSession();

  /**
   * Parse a saved session info and attempt to construct a resumed context.
   * The first byte in a saved session info must always be the protocol version.
   * Note that an {@link IllegalArgumentException} will be thrown if the savedSessionInfo is not
   * properly formatted.
   *
   * @return a resumed context from a saved session.
   */
  public static D2DConnectionContext fromSavedSession(byte[] savedSessionInfo) {
    if (savedSessionInfo == null || savedSessionInfo.length == 0) {
      throw new IllegalArgumentException("savedSessionInfo null or too short");
    }

    int protocolVersion = savedSessionInfo[0] & 0xff;

    switch (protocolVersion) {
      case 0:
        // Version 0 has a 1 byte protocol version, a 4 byte sequence number,
        // and 32 bytes of AES key (1 + 4 + 32 = 37)
        if (savedSessionInfo.length != 37) {
          throw new IllegalArgumentException("Incorrect data length (" + savedSessionInfo.length
              + ") for v0 protocol");
        }
        int sequenceNumber = bytesToSignedInt(Arrays.copyOfRange(savedSessionInfo, 1, 5));
        SecretKey sharedKey = new SecretKeySpec(Arrays.copyOfRange(savedSessionInfo, 5, 37), "AES");
        return new D2DConnectionContextV0(sharedKey, sequenceNumber);

      case 1:
        // Version 1 has a 1 byte protocol version, two 4 byte sequence numbers,
        // and two 32 byte AES keys (1 + 4 + 4 + 32 + 32 = 73)
        if (savedSessionInfo.length != 73) {
          throw new IllegalArgumentException("Incorrect data length for v1 protocol");
        }
        int encodeSequenceNumber = bytesToSignedInt(Arrays.copyOfRange(savedSessionInfo, 1, 5));
        int decodeSequenceNumber = bytesToSignedInt(Arrays.copyOfRange(savedSessionInfo, 5, 9));
        SecretKey encodeKey =
            new SecretKeySpec(Arrays.copyOfRange(savedSessionInfo, 9, 41), "AES");
        SecretKey decodeKey =
            new SecretKeySpec(Arrays.copyOfRange(savedSessionInfo, 41, 73), "AES");
        return new D2DConnectionContextV1(encodeKey, decodeKey, encodeSequenceNumber,
            decodeSequenceNumber);

      default:
        throw new IllegalArgumentException("Cannot rebuild context, unkown protocol version: "
            + protocolVersion);
    }
  }

  /**
   * Convert 4 bytes in big-endian representation into a signed int.
   */
  static int bytesToSignedInt(byte[] bytes) {
    if (bytes.length != 4) {
      throw new IllegalArgumentException("Expected 4 bytes to encode int, but got: "
          + bytes.length + " bytes");
    }

    return ((bytes[0] << 24) & 0xff000000)
        |  ((bytes[1] << 16) & 0x00ff0000)
        |  ((bytes[2] << 8)  & 0x0000ff00)
        |   (bytes[3]        & 0x000000ff);
  }

  /**
   * Convert a signed int into a 4 byte big-endian representation
   */
  static byte[] signedIntToBytes(int val) {
    byte[] bytes = new byte[4];

    bytes[0] = (byte) ((val >> 24) & 0xff);
    bytes[1] = (byte) ((val >> 16) & 0xff);
    bytes[2] = (byte) ((val >> 8)  & 0xff);
    bytes[3] = (byte)  (val        & 0xff);

    return bytes;
  }
}
