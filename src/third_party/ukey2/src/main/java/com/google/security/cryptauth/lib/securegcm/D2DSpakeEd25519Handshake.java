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
import com.google.security.cryptauth.lib.securegcm.DeviceToDeviceMessagesProto.EcPoint;
import com.google.security.cryptauth.lib.securegcm.DeviceToDeviceMessagesProto.SpakeHandshakeMessage;
import com.google.security.cryptauth.lib.securegcm.Ed25519.Ed25519Exception;
import com.google.security.cryptauth.lib.securegcm.TransportCryptoOps.Payload;
import com.google.security.cryptauth.lib.securegcm.TransportCryptoOps.PayloadType;
import java.math.BigInteger;
import java.security.InvalidKeyException;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.security.SecureRandom;
import java.security.SignatureException;
import javax.crypto.spec.SecretKeySpec;

/**
 * Implements a {@link D2DHandshakeContext} by using SPAKE2 (Simple Password-Based Encrypted Key
 * Exchange Protocol) on top of the Ed25519 curve.
 * SPAKE2: http://www.di.ens.fr/~mabdalla/papers/AbPo05a-letter.pdf
 * Ed25519: http://ed25519.cr.yp.to/
 *
 * <p>Usage:
 * {@code
 *   // initiator:
 *   D2DHandshakeContext initiatorHandshakeContext =
 *       D2DSpakeEd25519Handshake.forInitiator(PASSWORD);
 *   byte[] initiatorMsg = initiatorHandshakeContext.getNextHandshakeMessage();
 *   // (send initiatorMsg to responder)
 *
 *   // responder:
 *   D2DHandshakeContext responderHandshakeContext =
 *       D2DSpakeEd25519Handshake.forResponder(PASSWORD);
 *   responderHandshakeContext.parseHandshakeMessage(initiatorMsg);
 *   byte[] responderMsg = responderHandshakeContext.getNextHandshakeMessage();
 *   // (send responderMsg to initiator)
 *
 *   // initiator:
 *   initiatorHandshakeContext.parseHandshakeMessage(responderMsg);
 *   initiatorMsg = initiatorHandshakeContext.getNextHandshakeMessage();
 *   // (send initiatorMsg to responder)
 *
 *   // responder:
 *   responderHandshakeContext.parseHandshakeMessage(initiatorMsg);
 *   responderMsg = responderHandshakeContext.getNextHandshakeMessage();
 *   D2DConnectionContext responderCtx = responderHandshakeContext.toConnectionContext();
 *   // (send responderMsg to initiator)
 *
 *   // initiator:
 *   initiatorHandshakeContext.parseHandshakeMessage(responderMsg);
 *   D2DConnectionContext initiatorCtx = initiatorHandshakeContext.toConnectionContext();
 * }
 *
 * <p>The initial computation is:
 *   Initiator                                              Responder
 *    has KM (pre-agreed point)                              has KM (pre-agreed point)
 *    has KN (pre-agreed point)                              has KN (pre-agreed point)
 *    has Password (pre-agreed)                              has Password (pre-agreed)
 *    picks random scalar Xi (private key)                   picks random scalar Xr (private key)
 *    computes the public key Pxi = G*Xi                     computes the public key Pxr = G*Xr
 *    computes commitment:                                   computes commitment:
 *      Ci = KM * password + Pxi                               Cr = KN * password + Pxr
 *
 * <p>The flow is:
 *   Initiator                                              Responder
 *              ----- Ci --------------------------------->
 *              <--------------------------------- Cr -----
 *   computes shared key K:                                 computes shared key K:
 *    (Cr - KN*password) * Xi                                (Ci - KM*password) * Xr
 *   computes hash:                                         computes hash:
 *    Hi = sha256(0|Cr|Ci|K)                                 Hr = sha256(1|Ci|Cr|K)
 *              ----- Hi --------------------------------->
 *                                                          Verify Hi
 *              <-------------- Hr (optional payload) -----
 *   Verify Hr
 */
public class D2DSpakeEd25519Handshake implements D2DHandshakeContext {
  // Minimum length password that is acceptable for the handshake
  public static final int MIN_PASSWORD_LENGTH = 4;
  /**
   * Creates a new SPAKE handshake object for the initiator.
   *
   * @param password the password that should be used in the handshake.  Note that this should be
   *     at least {@value #MIN_PASSWORD_LENGTH} bytes long
   */
  public static D2DSpakeEd25519Handshake forInitiator(byte[] password) throws HandshakeException {
    return new D2DSpakeEd25519Handshake(State.INITIATOR_START, password);
  }

  /**
   * Creates a new SPAKE handshake object for the responder.
   *
   * @param password the password that should be used in the handshake.  Note that this should be
   *     at least {@value #MIN_PASSWORD_LENGTH} bytes long
   */
  public static D2DSpakeEd25519Handshake forResponder(byte[] password) throws HandshakeException {
    return new D2DSpakeEd25519Handshake(State.RESPONDER_START, password);
  }

  //
  // The protocol requires two verifiable, randomly generated group point. They were generated
  // using the python code below. The algorithm is to first pick a random y in the group and solve
  // the elliptic curve equation for a value of x, if possible. We then use (x, y) as the random
  // point.
  // Source of ed25519 is here: http://ed25519.cr.yp.to/python/ed25519.py
  // import ed25519
  // import hashlib
  //
  // # Seeds
  // seed1 = 'D2D Ed25519 point generation seed (M)'
  // seed2 = 'D2D Ed25519 point generation seed (N)'
  //
  // def find_seed(seed):
  //   # generate a random scalar for the y coordinate
  //   y = hashlib.sha256(seed).hexdigest()
  //
  //   P = ed25519.scalarmult(ed25519.B, int(y, 16) % ed25519.q)
  //   if (not ed25519.isoncurve(P)):
  //       print 'Wat? P should be on curve!'
  //
  //   print '  x: ' + hex(P[0])
  //   print '  y: ' + hex(P[1])
  //   print
  //
  // find_seed(seed1)
  // find_seed(seed2)
  //
  // Output is:
  //   x: 0x1981fb43f103290ecf9772022db8b19bfaf389057ed91e8486eb368763435925L
  //   y: 0xa714c34f3b588aac92fd2587884a20964fd351a1f147d5c4bbf5c2f37a77c36L
  //
  //   x: 0x201a184f47d9a7973891d148e3d1c864d8084547131c2c1cefb7eebd26c63567L
  //   y: 0x6da2d3b18ec4f9aa3b08e39c997cd8bf6e9948ffd4feffecaf8dd0b3d648b7e8L
  //
  // To get extended representation X, Y, Z, T, do: Z = 1, T = X*Y mod P
  @VisibleForTesting
  static final BigInteger[] KM = new BigInteger[] {
    new BigInteger(new byte[] {(byte) 0x19, (byte) 0x81, (byte) 0xFB, (byte) 0x43,
        (byte) 0xF1, (byte) 0x03, (byte) 0x29, (byte) 0x0E, (byte) 0xCF, (byte) 0x97,
        (byte) 0x72, (byte) 0x02, (byte) 0x2D, (byte) 0xB8, (byte) 0xB1, (byte) 0x9B,
        (byte) 0xFA, (byte) 0xF3, (byte) 0x89, (byte) 0x05, (byte) 0x7E, (byte) 0xD9,
        (byte) 0x1E, (byte) 0x84, (byte) 0x86, (byte) 0xEB, (byte) 0x36, (byte) 0x87,
        (byte) 0x63, (byte) 0x43, (byte) 0x59, (byte) 0x25}),
    new BigInteger(new byte[] {(byte) 0x0A, (byte) 0x71, (byte) 0x4C, (byte) 0x34,
        (byte) 0xF3, (byte) 0xB5, (byte) 0x88, (byte) 0xAA, (byte) 0xC9, (byte) 0x2F,
        (byte) 0xD2, (byte) 0x58, (byte) 0x78, (byte) 0x84, (byte) 0xA2, (byte) 0x09,
        (byte) 0x64, (byte) 0xFD, (byte) 0x35, (byte) 0x1A, (byte) 0x1F, (byte) 0x14,
        (byte) 0x7D, (byte) 0x5C, (byte) 0x4B, (byte) 0xBF, (byte) 0x5C, (byte) 0x2F,
        (byte) 0x37, (byte) 0xA7, (byte) 0x7C, (byte) 0x36}),
    BigInteger.ONE,
    new BigInteger(new byte[] {(byte) 0x04, (byte) 0x8F, (byte) 0xC1, (byte) 0xCE,
        (byte) 0xE5, (byte) 0x83, (byte) 0x99, (byte) 0x25, (byte) 0xE5, (byte) 0x9B,
        (byte) 0x80, (byte) 0xEA, (byte) 0xAD, (byte) 0x82, (byte) 0xAC, (byte) 0x0A,
        (byte) 0x3C, (byte) 0xFE, (byte) 0xC5, (byte) 0x60, (byte) 0x93, (byte) 0x59,
        (byte) 0x8B, (byte) 0x48, (byte) 0x44, (byte) 0xDD, (byte) 0x2A, (byte) 0x3E,
        (byte) 0x24, (byte) 0x5D, (byte) 0x88, (byte) 0x33})};

  @VisibleForTesting
  static final BigInteger[] KN = new BigInteger[] {
    new BigInteger(new byte[] {(byte) 0x20, (byte) 0x1A, (byte) 0x18, (byte) 0x4F,
        (byte) 0x47, (byte) 0xD9, (byte) 0xA7, (byte) 0x97, (byte) 0x38, (byte) 0x91,
        (byte) 0xD1, (byte) 0x48, (byte) 0xE3, (byte) 0xD1, (byte) 0xC8, (byte) 0x64,
        (byte) 0xD8, (byte) 0x08, (byte) 0x45, (byte) 0x47, (byte) 0x13, (byte) 0x1C,
        (byte) 0x2C, (byte) 0x1C, (byte) 0xEF, (byte) 0xB7, (byte) 0xEE, (byte) 0xBD,
        (byte) 0x26, (byte) 0xC6, (byte) 0x35, (byte) 0x67}),
    new BigInteger(new byte[] {(byte) 0x6D, (byte) 0xA2, (byte) 0xD3, (byte) 0xB1,
        (byte) 0x8E, (byte) 0xC4, (byte) 0xF9, (byte) 0xAA, (byte) 0x3B, (byte) 0x08,
        (byte) 0xE3, (byte) 0x9C, (byte) 0x99, (byte) 0x7C, (byte) 0xD8, (byte) 0xBF,
        (byte) 0x6E, (byte) 0x99, (byte) 0x48, (byte) 0xFF, (byte) 0xD4, (byte) 0xFE,
        (byte) 0xFF, (byte) 0xEC, (byte) 0xAF, (byte) 0x8D, (byte) 0xD0, (byte) 0xB3,
        (byte) 0xD6, (byte) 0x48, (byte) 0xB7, (byte) 0xE8}),
    BigInteger.ONE,
    new BigInteger(new byte[] {(byte) 0x16, (byte) 0x40, (byte) 0xED, (byte) 0x5A,
        (byte) 0x54, (byte) 0xFA, (byte) 0x0B, (byte) 0x07, (byte) 0x22, (byte) 0x86,
        (byte) 0xE9, (byte) 0xD2, (byte) 0x2F, (byte) 0x46, (byte) 0x47, (byte) 0x63,
        (byte) 0xFB, (byte) 0xF6, (byte) 0x0D, (byte) 0x79, (byte) 0x1D, (byte) 0x37,
        (byte) 0xB9, (byte) 0x09, (byte) 0x3B, (byte) 0x58, (byte) 0x4D, (byte) 0xF4,
        (byte) 0xC9, (byte) 0x95, (byte) 0xF7, (byte) 0x81})};

  // Base point B as per ed25519.cr.yp.to
  @VisibleForTesting
  /* package */ static final BigInteger[] B = new BigInteger[] {
    new BigInteger(new byte[] {(byte) 0x21, (byte) 0x69, (byte) 0x36, (byte) 0xD3,
        (byte) 0xCD, (byte) 0x6E, (byte) 0x53, (byte) 0xFE, (byte) 0xC0, (byte) 0xA4,
        (byte) 0xE2, (byte) 0x31, (byte) 0xFD, (byte) 0xD6, (byte) 0xDC, (byte) 0x5C,
        (byte) 0x69, (byte) 0x2C, (byte) 0xC7, (byte) 0x60, (byte) 0x95, (byte) 0x25,
        (byte) 0xA7, (byte) 0xB2, (byte) 0xC9, (byte) 0x56, (byte) 0x2D, (byte) 0x60,
        (byte) 0x8F, (byte) 0x25, (byte) 0xD5, (byte) 0x1A}),
    new BigInteger(new byte[] {(byte) 0x66, (byte) 0x66, (byte) 0x66, (byte) 0x66,
        (byte) 0x66, (byte) 0x66, (byte) 0x66, (byte) 0x66, (byte) 0x66, (byte) 0x66,
        (byte) 0x66, (byte) 0x66, (byte) 0x66, (byte) 0x66, (byte) 0x66, (byte) 0x66,
        (byte) 0x66, (byte) 0x66, (byte) 0x66, (byte) 0x66, (byte) 0x66, (byte) 0x66,
        (byte) 0x66, (byte) 0x66, (byte) 0x66, (byte) 0x66, (byte) 0x66, (byte) 0x66,
        (byte) 0x66, (byte) 0x66, (byte) 0x66, (byte) 0x58}),
    BigInteger.ONE,
    new BigInteger(new byte[] {(byte) 0x67, (byte) 0x87, (byte) 0x5F, (byte) 0x0F,
        (byte) 0xD7, (byte) 0x8B, (byte) 0x76, (byte) 0x65, (byte) 0x66, (byte) 0xEA,
        (byte) 0x4E, (byte) 0x8E, (byte) 0x64, (byte) 0xAB, (byte) 0xE3, (byte) 0x7D,
        (byte) 0x20, (byte) 0xF0, (byte) 0x9F, (byte) 0x80, (byte) 0x77, (byte) 0x51,
        (byte) 0x52, (byte) 0xF5, (byte) 0x6D, (byte) 0xDE, (byte) 0x8A, (byte) 0xB3,
        (byte) 0xA5, (byte) 0xB7, (byte) 0xDD, (byte) 0xA3})};

  // Number of bits needed to represent a point
  private static final int POINT_SIZE_BITS = 256;

  // Java Message Digest name for SHA 256
  private static final String SHA256 = "SHA-256";

  // Pre-shared password hash represented as an integer
  private BigInteger passwordHash;

  // Current state of the handshake
  private State handshakeState;

  // Derived shared key
  private byte[] sharedKey;

  // Private key (random scalar)
  private BigInteger valueX;

  // Public key (random point, in extended notation, based on valueX)
  private BigInteger[] pointX;

  // Commitment we've received from the other party (their password-authenticated public key)
  private BigInteger[] theirCommitmentPointAffine;
  private BigInteger[] theirCommitmentPointExtended;

  // Commitment we've sent to the other party (our password-authenticated public key)
  private BigInteger[] ourCommitmentPointAffine;
  private BigInteger[] ourCommitmentPointExtended;

  private enum State {
    // Initiator state
    INITIATOR_START,
    INITIATOR_WAITING_FOR_RESPONDER_COMMITMENT,
    INITIATOR_AFTER_RESPONDER_COMMITMENT,
    INITIATOR_WAITING_FOR_RESPONDER_HASH,

    // Responder state
    RESPONDER_START,
    RESPONDER_AFTER_INITIATOR_COMMITMENT,
    RESPONDER_WAITING_FOR_INITIATOR_HASH,
    RESPONDER_AFTER_INITIATOR_HASH,

    // Common completion state
    HANDSHAKE_FINISHED,
    HANDSHAKE_ALREADY_USED
  }

  @VisibleForTesting
  D2DSpakeEd25519Handshake(State state, byte[] password) throws HandshakeException {
    if (password == null || password.length < MIN_PASSWORD_LENGTH) {
      throw new HandshakeException("Passwords must be at least " + MIN_PASSWORD_LENGTH + " bytes");
    }

    handshakeState = state;
    passwordHash = new BigInteger(1 /* positive */, hash(password));

    do {
      valueX = new BigInteger(POINT_SIZE_BITS, new SecureRandom());
    } while (valueX.equals(BigInteger.ZERO));

    try {
      pointX = Ed25519.scalarMultiplyExtendedPoint(B, valueX);
    } catch (Ed25519Exception e) {
      throw new HandshakeException("Could not make public key point", e);
    }
  }

  @Override
  public boolean isHandshakeComplete() {
    switch (handshakeState) {
      case HANDSHAKE_FINISHED:
        // fall-through intentional
      case HANDSHAKE_ALREADY_USED:
        return true;

      default:
        return false;
    }
  }

  @Override
  public byte[] getNextHandshakeMessage() throws HandshakeException {
    byte[] nextMessage;

    switch(handshakeState) {
      case INITIATOR_START:
        nextMessage = makeCommitmentPointMessage(true /* is initiator */);
        handshakeState = State.INITIATOR_WAITING_FOR_RESPONDER_COMMITMENT;
        break;

      case RESPONDER_AFTER_INITIATOR_COMMITMENT:
        nextMessage = makeCommitmentPointMessage(false /* is initiator */);
        handshakeState = State.RESPONDER_WAITING_FOR_INITIATOR_HASH;
        break;

      case INITIATOR_AFTER_RESPONDER_COMMITMENT:
        nextMessage = makeSharedKeyHashMessage(true /* is initiator */, null /* no payload */);
        handshakeState = State.INITIATOR_WAITING_FOR_RESPONDER_HASH;
        break;

      case RESPONDER_AFTER_INITIATOR_HASH:
        nextMessage = makeSharedKeyHashMessage(false /* is initiator */, null /* no payload */);
        handshakeState = State.HANDSHAKE_FINISHED;
        break;

      default:
        throw new HandshakeException("Cannot get next message in state: " + handshakeState);
    }

    return nextMessage;
  }

  @Override
  public byte[] getNextHandshakeMessage(byte[] payload) throws HandshakeException {
    byte[] nextMessage;

    switch (handshakeState) {
      case RESPONDER_AFTER_INITIATOR_HASH:
        nextMessage = makeSharedKeyHashMessage(false /* is initiator */, payload);
        handshakeState = State.HANDSHAKE_FINISHED;
        break;

      default:
        throw new HandshakeException(
            "Cannot send handshake message with payload in state: " + handshakeState);
    }

    return nextMessage;
  }

  private byte[] makeCommitmentPointMessage(boolean isInitiator) throws HandshakeException {
    try {
      ourCommitmentPointExtended =
          Ed25519.scalarMultiplyExtendedPoint(isInitiator ? KM : KN, passwordHash);
      ourCommitmentPointExtended = Ed25519.addExtendedPoints(ourCommitmentPointExtended, pointX);
      ourCommitmentPointAffine = Ed25519.toAffine(ourCommitmentPointExtended);

      return SpakeHandshakeMessage.newBuilder()
          .setEcPoint(
              EcPoint.newBuilder()
                .setCurve(DeviceToDeviceMessagesProto.Curve.ED_25519)
                .setX(ByteString.copyFrom(ourCommitmentPointAffine[0].toByteArray()))
                .setY(ByteString.copyFrom(ourCommitmentPointAffine[1].toByteArray()))
                .build())
          .setFlowNumber(isInitiator ? 1 : 2 /* first or second message */)
          .build()
          .toByteArray();
    } catch (Ed25519Exception e) {
      throw new HandshakeException("Could not make commitment point message", e);
    }
  }

  private void makeSharedKey(boolean isInitiator) throws HandshakeException {

    if (handshakeState != State.RESPONDER_START
        && handshakeState != State.INITIATOR_WAITING_FOR_RESPONDER_COMMITMENT) {
      throw new HandshakeException("Cannot make shared key in state: " + handshakeState);
    }

    try {
      BigInteger[] kNMP = Ed25519.scalarMultiplyExtendedPoint(isInitiator ? KN : KM, passwordHash);

      // TheirPublicKey = TheirCommitment - kNMP = (TheirPublicKey + kNMP) - kNMP
      BigInteger[] theirPublicKey =
          Ed25519.subtractExtendedPoints(theirCommitmentPointExtended, kNMP);

      BigInteger[] sharedKeyPoint = Ed25519.scalarMultiplyExtendedPoint(theirPublicKey, valueX);
      sharedKey = hash(pointToByteArray(Ed25519.toAffine(sharedKeyPoint)));
    } catch (Ed25519Exception e) {
      throw new HandshakeException("Error computing shared key", e);
    }
  }

  private byte[] makeSharedKeyHashMessage(boolean isInitiator, byte[] payload)
      throws HandshakeException {
    SpakeHandshakeMessage.Builder handshakeMessage = SpakeHandshakeMessage.newBuilder()
        .setHashValue(ByteString.copyFrom(computeOurKeyHash(isInitiator)))
        .setFlowNumber(isInitiator ? 3 : 4 /* third or fourth message */);

    if (canSendPayloadInHandshakeMessage() && payload != null) {
      DeviceToDeviceMessage deviceToDeviceMessage =
          D2DConnectionContext.createDeviceToDeviceMessage(payload, 1 /* sequence number */);
      try {
        handshakeMessage.setPayload(ByteString.copyFrom(
            D2DCryptoOps.signcryptPayload(
              new Payload(PayloadType.DEVICE_TO_DEVICE_RESPONDER_HELLO_PAYLOAD,
                  deviceToDeviceMessage.toByteArray()),
              new SecretKeySpec(sharedKey, "AES"))));
      } catch (InvalidKeyException | NoSuchAlgorithmException e) {
        throw new HandshakeException("Cannot set payload", e);
      }
    }

    return handshakeMessage.build().toByteArray();
  }

  private byte[] computeOurKeyHash(boolean isInitiator) throws HandshakeException {
    return hash(concat(
        new byte[] { (byte) (isInitiator ? 0 : 1) },
        pointToByteArray(theirCommitmentPointAffine),
        pointToByteArray(ourCommitmentPointAffine),
        sharedKey));
  }

  private byte[] computeTheirKeyHash(boolean isInitiator) throws HandshakeException {
    return hash(concat(
        new byte[] { (byte) (isInitiator ? 1 : 0) },
        pointToByteArray(ourCommitmentPointAffine),
        pointToByteArray(theirCommitmentPointAffine),
        sharedKey));
  }

  private byte[] pointToByteArray(BigInteger[] p) {
    return concat(p[0].toByteArray(), p[1].toByteArray());
  }

  @Override
  public boolean canSendPayloadInHandshakeMessage() {
    return handshakeState == State.RESPONDER_AFTER_INITIATOR_HASH;
  }

  @Override
  public byte[] parseHandshakeMessage(byte[] handshakeMessage) throws HandshakeException {
    if (handshakeMessage == null || handshakeMessage.length == 0) {
      throw new HandshakeException("Handshake message too short");
    }

    byte[] payload = new byte[0];

    switch(handshakeState) {
      case RESPONDER_START:
        // no payload can be sent in this message
        parseCommitmentMessage(handshakeMessage, false /* is initiator */);
        makeSharedKey(false /* is initiator */);
        handshakeState = State.RESPONDER_AFTER_INITIATOR_COMMITMENT;
        break;

      case INITIATOR_WAITING_FOR_RESPONDER_COMMITMENT:
        // no payload can be sent in this message
        parseCommitmentMessage(handshakeMessage, true /* is initiator */);
        makeSharedKey(true /* is initiator */);
        handshakeState = State.INITIATOR_AFTER_RESPONDER_COMMITMENT;
        break;

      case RESPONDER_WAITING_FOR_INITIATOR_HASH:
        // no payload can be sent in this message
        parseHashMessage(handshakeMessage, false /* is initiator */);
        handshakeState = State.RESPONDER_AFTER_INITIATOR_HASH;
        break;

      case INITIATOR_WAITING_FOR_RESPONDER_HASH:
        payload = parseHashMessage(handshakeMessage, true /* is initiator */);
        handshakeState = State.HANDSHAKE_FINISHED;
        break;

      default:
        throw new HandshakeException("Cannot parse message in state: " + handshakeState);
    }

    return payload;
  }

  private byte[] parseHashMessage(byte[] handshakeMessage, boolean isInitiator)
      throws HandshakeException {
    SpakeHandshakeMessage hashMessage;

    // Parse the message
    try {
      hashMessage = SpakeHandshakeMessage.parseFrom(handshakeMessage);
    } catch (InvalidProtocolBufferException e) {
      throw new HandshakeException("Could not parse hash message", e);
    }

    // Check flow number
    if (!hashMessage.hasFlowNumber()) {
      throw new HandshakeException("Hash message missing flow number");
    }
    int expectedFlowNumber = isInitiator ? 4 : 3;
    int actualFlowNumber = hashMessage.getFlowNumber();
    if (actualFlowNumber != expectedFlowNumber) {
      throw new HandshakeException("Hash message has flow number " + actualFlowNumber
          + ", but expected flow number " + expectedFlowNumber);
    }

    // Check and extract hash
    if (!hashMessage.hasHashValue()) {
      throw new HandshakeException("Hash message missing hash value");
    }

    byte[] theirHash = hashMessage.getHashValue().toByteArray();
    byte[] theirCorrectHash = computeTheirKeyHash(isInitiator);

    if (!constantTimeArrayEquals(theirCorrectHash, theirHash)) {
      throw new HandshakeException("Hash message had incorrect hash value");
    }

    if (isInitiator && hashMessage.hasPayload()) {
      try {
        DeviceToDeviceMessage message = D2DCryptoOps.decryptResponderHelloMessage(
            new SecretKeySpec(sharedKey, "AES"),
            hashMessage.getPayload().toByteArray());

        if (message.getSequenceNumber() != 1) {
          throw new HandshakeException("Incorrect sequence number in responder hello");
        }

        return message.getMessage().toByteArray();

      } catch (SignatureException e) {
        throw new HandshakeException("Error recovering payload from hash message", e);
      }
    }

    // empty/no payload
    return new byte[0];
  }

  private void parseCommitmentMessage(byte[] handshakeMessage, boolean isInitiator)
      throws HandshakeException {
    SpakeHandshakeMessage commitmentMessage;

    // Parse the message
    try {
      commitmentMessage = SpakeHandshakeMessage.parseFrom(handshakeMessage);
    } catch (InvalidProtocolBufferException e) {
      throw new HandshakeException("Could not parse commitment message", e);
    }

    // Check flow number
    if (!commitmentMessage.hasFlowNumber()) {
      throw new HandshakeException("Commitment message missing flow number");
    }
    if (commitmentMessage.getFlowNumber() != (isInitiator ? 2 : 1)) {
      throw new HandshakeException("Commitment message has wrong flow number");
    }

    // Check point and curve; and extract point
    if (!commitmentMessage.hasEcPoint()) {
      throw new HandshakeException("Commitment message missing point");
    }
    EcPoint commitmentPoint = commitmentMessage.getEcPoint();
    if (!commitmentPoint.hasCurve()
        || commitmentPoint.getCurve() != DeviceToDeviceMessagesProto.Curve.ED_25519) {
      throw new HandshakeException("Commitment message has wrong curve");
    }

    if (!commitmentPoint.hasX()) {
      throw new HandshakeException("Commitment point missing x coordinate");
    }

    if (!commitmentPoint.hasY()) {
      throw new HandshakeException("Commitment point missing y coordinate");
    }

    // Build the point
    theirCommitmentPointAffine = new BigInteger[] {
        new BigInteger(commitmentPoint.getX().toByteArray()),
        new BigInteger(commitmentPoint.getY().toByteArray())
    };

    // Validate the point to be sure
    try {
      Ed25519.validateAffinePoint(theirCommitmentPointAffine);
      theirCommitmentPointExtended = Ed25519.toExtended(theirCommitmentPointAffine);
    } catch (Ed25519Exception e) {
      throw new HandshakeException("Error validating their commitment point", e);
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

    // Both sides start with an initial sequence number of 1 because the last message of the
    // handshake had an optional payload with sequence number 1.  D2DConnectionContext remembers
    // the last sequence number used.
    return new D2DConnectionContextV0(
        new SecretKeySpec(sharedKey, "AES"), 1 /* initialSequenceNumber */);
  }

  /**
   * Implementation of byte array concatenation copied from Guava.
   */
  private static byte[] concat(byte[]... arrays) {
    int length = 0;
    for (byte[] array : arrays) {
      length += array.length;
    }

    byte[] result = new byte[length];
    int pos = 0;
    for (byte[] array : arrays) {
      System.arraycopy(array, 0, result, pos, array.length);
      pos += array.length;
    }

    return result;
  }

  private static byte[] hash(byte[] message) throws HandshakeException {
    try {
      return MessageDigest.getInstance(SHA256).digest(message);
    } catch (NoSuchAlgorithmException e) {
      throw new HandshakeException("Error performing hash", e);
    }
  }

  private static boolean constantTimeArrayEquals(byte[] a, byte[] b) {
    if (a == null || b == null) {
      return (a == b);
    }
    if (a.length != b.length) {
      return false;
    }
    byte result = 0;
    for (int i = 0; i < b.length; i++) {
      result = (byte) (result | (a[i] ^ b[i]));
    }
    return (result == 0);
  }
}
