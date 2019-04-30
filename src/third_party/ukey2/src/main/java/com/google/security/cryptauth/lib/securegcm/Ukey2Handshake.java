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

import com.google.protobuf.ByteString;
import com.google.protobuf.InvalidProtocolBufferException;
import com.google.security.cryptauth.lib.securegcm.UkeyProto.Ukey2Alert;
import com.google.security.cryptauth.lib.securegcm.UkeyProto.Ukey2ClientFinished;
import com.google.security.cryptauth.lib.securegcm.UkeyProto.Ukey2ClientInit;
import com.google.security.cryptauth.lib.securegcm.UkeyProto.Ukey2ClientInit.CipherCommitment;
import com.google.security.cryptauth.lib.securegcm.UkeyProto.Ukey2Message;
import com.google.security.cryptauth.lib.securegcm.UkeyProto.Ukey2ServerInit;
import com.google.security.cryptauth.lib.securemessage.CryptoOps;
import com.google.security.cryptauth.lib.securemessage.PublicKeyProtoUtil;
import com.google.security.cryptauth.lib.securemessage.SecureMessageProto.GenericPublicKey;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.UnsupportedEncodingException;
import java.security.InvalidKeyException;
import java.security.KeyPair;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.security.PublicKey;
import java.security.SecureRandom;
import java.security.spec.InvalidKeySpecException;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import javax.annotation.Nullable;
import javax.crypto.SecretKey;
import javax.crypto.spec.SecretKeySpec;

/**
 * Implements UKEY2 and produces a {@link D2DConnectionContext}.
 *
 * <p>Client Usage:
 * <code>
 * try {
 *   Ukey2Handshake client = Ukey2Handshake.forInitiator(HandshakeCipher.P256_SHA512);
 *   byte[] handshakeMessage;
 *
 *   // Message 1 (Client Init)
 *   handshakeMessage = client.getNextHandshakeMessage();
 *   sendMessageToServer(handshakeMessage);
 *
 *   // Message 2 (Server Init)
 *   handshakeMessage = receiveMessageFromServer();
 *   client.parseHandshakeMessage(handshakeMessage);
 *
 *   // Message 3 (Client Finish)
 *   handshakeMessage = client.getNextHandshakeMessage();
 *   sendMessageToServer(handshakeMessage);
 *
 *   // Get the auth string
 *   byte[] clientAuthString = client.getVerificationString(STRING_LENGTH);
 *   showStringToUser(clientAuthString);
 *
 *   // Using out-of-band channel, verify auth string, then call:
 *   client.verifyHandshake();
 *
 *   // Make a connection context
 *   D2DConnectionContext clientContext = client.toConnectionContext();
 * } catch (AlertException e) {
 *   log(e.getMessage);
 *   sendMessageToServer(e.getAlertMessageToSend());
 * } catch (HandshakeException e) {
 *   log(e);
 *   // terminate handshake
 * }
 * </code>
 *
 * <p>Server Usage:
 * <code>
 * try {
 *   Ukey2Handshake server = Ukey2Handshake.forResponder(HandshakeCipher.P256_SHA512);
 *   byte[] handshakeMessage;
 *
 *   // Message 1 (Client Init)
 *   handshakeMessage = receiveMessageFromClient();
 *   server.parseHandshakeMessage(handshakeMessage);
 *
 *   // Message 2 (Server Init)
 *   handshakeMessage = server.getNextHandshakeMessage();
 *   sendMessageToServer(handshakeMessage);
 *
 *   // Message 3 (Client Finish)
 *   handshakeMessage = receiveMessageFromClient();
 *   server.parseHandshakeMessage(handshakeMessage);
 *
 *   // Get the auth string
 *   byte[] serverAuthString = server.getVerificationString(STRING_LENGTH);
 *   showStringToUser(serverAuthString);
 *
 *   // Using out-of-band channel, verify auth string, then call:
 *   server.verifyHandshake();
 *
 *   // Make a connection context
 *   D2DConnectionContext serverContext = server.toConnectionContext();
 * } catch (AlertException e) {
 *   log(e.getMessage);
 *   sendMessageToClient(e.getAlertMessageToSend());
 * } catch (HandshakeException e) {
 *   log(e);
 *   // terminate handshake
 * }
 * </code>
 */
public class Ukey2Handshake {

  /**
   * Creates a {@link Ukey2Handshake} with a particular cipher that can be used by an initiator /
   * client.
   *
   * @throws HandshakeException
   */
  public static Ukey2Handshake forInitiator(HandshakeCipher cipher) throws HandshakeException {
    return new Ukey2Handshake(InternalState.CLIENT_START, cipher);
  }

  /**
   * Creates a {@link Ukey2Handshake} with a particular cipher that can be used by an responder /
   * server.
   *
   * @throws HandshakeException
   */
  public static Ukey2Handshake forResponder(HandshakeCipher cipher) throws HandshakeException {
    return new Ukey2Handshake(InternalState.SERVER_START, cipher);
  }

  /**
   * Handshake States. Meaning of states:
   * <ul>
   * <li>IN_PROGRESS: The handshake is in progress, caller should use
   * {@link Ukey2Handshake#getNextHandshakeMessage()} and
   * {@link Ukey2Handshake#parseHandshakeMessage(byte[])} to continue the handshake.
   * <li>VERIFICATION_NEEDED: The handshake is complete, but pending verification of the
   * authentication string. Clients should use {@link Ukey2Handshake#getVerificationString(int)} to
   * get the verification string and use out-of-band methods to authenticate the handshake.
   * <li>VERIFICATION_IN_PROGRESS: The handshake is complete, verification string has been
   * generated, but has not been confirmed. After authenticating the handshake out-of-band, use
   * {@link Ukey2Handshake#verifyHandshake()} to mark the handshake as verified.
   * <li>FINISHED: The handshake is finished, and caller can use
   * {@link Ukey2Handshake#toConnectionContext()} to produce a {@link D2DConnectionContext}.
   * <li>ALREADY_USED: The handshake has already been used and should be discarded / garbage
   * collected.
   * <li>ERROR: The handshake produced an error and should be destroyed.
   * </ul>
   */
  public enum State {
    IN_PROGRESS,
    VERIFICATION_NEEDED,
    VERIFICATION_IN_PROGRESS,
    FINISHED,
    ALREADY_USED,
    ERROR,
  }

  /**
   * Currently implemented UKEY2 handshake ciphers. Each cipher is a tuple consisting of a key
   * negotiation cipher and a hash function used for a commitment. Currently the ciphers are:
   * <code>
   *   +-----------------------------------------------------+
   *   | Enum        | Key negotiation       | Hash function |
   *   +-------------+-----------------------+---------------+
   *   | P256_SHA512 | ECDH using NIST P-256 | SHA512        |
   *   +-----------------------------------------------------+
   * </code>
   *
   * <p>Note that these should correspond to values in device_to_device_messages.proto.
   */
  public enum HandshakeCipher {
    P256_SHA512(UkeyProto.Ukey2HandshakeCipher.P256_SHA512);
    // TODO(aczeskis): add CURVE25519_SHA512

    private final UkeyProto.Ukey2HandshakeCipher value;

    HandshakeCipher(UkeyProto.Ukey2HandshakeCipher value) {
      // Make sure we only accept values that are valid as per the ukey protobuf.
      // NOTE: Don't use switch statement on value, as that will trigger a bug. b/30682989.
      if (value == UkeyProto.Ukey2HandshakeCipher.P256_SHA512) {
          this.value = value;
      } else {
          throw new IllegalArgumentException("Unknown cipher value: " + value);
      }
    }

    public UkeyProto.Ukey2HandshakeCipher getValue() {
      return value;
    }
  }

  /**
   * If thrown, this exception contains information that should be sent on the wire. Specifically,
   * the {@link #getAlertMessageToSend()} method returns a <code>byte[]</code> that communicates the
   * error to the other party in the handshake. Meanwhile, the {@link #getMessage()} method can be
   * used to get a log-able error message.
   */
  public static class AlertException extends Exception {
    private final Ukey2Alert alertMessageToSend;

    public AlertException(String alertMessageToLog, Ukey2Alert alertMessageToSend) {
      super(alertMessageToLog);
      this.alertMessageToSend = alertMessageToSend;
    }

    /**
     * @return a message suitable for sending to other member of handshake.
     */
    public byte[] getAlertMessageToSend() {
      return alertMessageToSend.toByteArray();
    }
  }

  // Maximum version of the handshake supported by this class.
  public static final int VERSION = 1;

  // Random nonce is fixed at 32 bytes (as per go/ukey2).
  private static final int NONCE_LENGTH_IN_BYTES = 32;

  private static final String UTF_8 = "UTF-8";

  // Currently, we only support one next protocol.
  private static final String NEXT_PROTOCOL = "AES_256_CBC-HMAC_SHA256";

  // Clients need to store a map of message 3's (client finishes) for each commitment.
  private final HashMap<HandshakeCipher, byte[]> rawMessage3Map = new HashMap<>();

  private final HandshakeCipher handshakeCipher;
  private final HandshakeRole handshakeRole;
  private InternalState handshakeState;
  private final KeyPair ourKeyPair;
  private PublicKey theirPublicKey;
  private SecretKey derivedSecretKey;

  // Servers need to store client commitments.
  private byte[] theirCommitment;

  // We store the raw messages sent for computing the authentication strings and next key.
  private byte[] rawMessage1;
  private byte[] rawMessage2;

  // Enums for internal state machinery
  private enum InternalState {
    // Initiator/client state
    CLIENT_START,
    CLIENT_WAITING_FOR_SERVER_INIT,
    CLIENT_AFTER_SERVER_INIT,

    // Responder/server state
    SERVER_START,
    SERVER_AFTER_CLIENT_INIT,
    SERVER_WAITING_FOR_CLIENT_FINISHED,

    // Common completion state
    HANDSHAKE_VERIFICATION_NEEDED,
    HANDSHAKE_VERIFICATION_IN_PROGRESS,
    HANDSHAKE_FINISHED,
    HANDSHAKE_ALREADY_USED,
    HANDSHAKE_ERROR,
  }

  // Helps us remember our role in the handshake
  private enum HandshakeRole {
    CLIENT,
    SERVER
  }

  /**
   * Never invoked directly. Caller should use {@link #forInitiator(HandshakeCipher)} or
   * {@link #forResponder(HandshakeCipher)} instead.
   *
   * @throws HandshakeException if an unrecoverable error occurs and the connection should be shut
   * down.
   */
  private Ukey2Handshake(InternalState state, HandshakeCipher cipher) throws HandshakeException {
    if (cipher == null) {
      throwIllegalArgumentException("Invalid handshake cipher");
    }
    this.handshakeCipher = cipher;

    switch (state) {
      case CLIENT_START:
        handshakeRole = HandshakeRole.CLIENT;
        break;
      case SERVER_START:
        handshakeRole = HandshakeRole.SERVER;
        break;
      default:
        throwIllegalStateException("Invalid handshake state");
        handshakeRole = null; // unreachable, but makes compiler happy
    }
    this.handshakeState = state;

    this.ourKeyPair = genKeyPair(cipher);
  }

  /**
   * Get the next handshake message suitable for sending on the wire.
   *
   * @throws HandshakeException if an unrecoverable error occurs and the connection should be shut
   * down.
   */
  public byte[] getNextHandshakeMessage() throws HandshakeException {
    switch (handshakeState) {
      case CLIENT_START:
        rawMessage1 = makeUkey2Message(Ukey2Message.Type.CLIENT_INIT, makeClientInitMessage());
        handshakeState = InternalState.CLIENT_WAITING_FOR_SERVER_INIT;
        return rawMessage1;

      case SERVER_AFTER_CLIENT_INIT:
        rawMessage2 = makeUkey2Message(Ukey2Message.Type.SERVER_INIT, makeServerInitMessage());
        handshakeState = InternalState.SERVER_WAITING_FOR_CLIENT_FINISHED;
        return rawMessage2;

      case CLIENT_AFTER_SERVER_INIT:
        // Make sure we have a message 3 for the chosen cipher.
        if (!rawMessage3Map.containsKey(handshakeCipher)) {
          throwIllegalStateException(
              "Client state is CLIENT_AFTER_SERVER_INIT, and cipher is "
                  + handshakeCipher
                  + ", but no corresponding raw client finished message has been generated");
        }
        handshakeState = InternalState.HANDSHAKE_VERIFICATION_NEEDED;
        return rawMessage3Map.get(handshakeCipher);

      default:
        throwIllegalStateException("Cannot get next message in state: " + handshakeState);
        return null; // unreachable, but makes compiler happy
    }
  }

  /**
   * Returns an authentication string suitable for authenticating the handshake out-of-band. Note
   * that the authentication string can be short (e.g., a 6 digit visual confirmation code). Note:
   * this should only be called when the state returned byte {@link #getHandshakeState()} is
   * {@link State#VERIFICATION_NEEDED}, which means this can only be called once.
   *
   * @param byteLength length of output in bytes. Min length is 1; max length is 32.
   */
  public byte[] getVerificationString(int byteLength) throws HandshakeException {
    if (byteLength < 1 || byteLength > 32) {
      throwIllegalArgumentException("Minimum length is 1 byte, max is 32 bytes");
    }

    if (handshakeState != InternalState.HANDSHAKE_VERIFICATION_NEEDED) {
      throwIllegalStateException("Unexpected state: " + handshakeState);
    }

    try {
      derivedSecretKey =
          EnrollmentCryptoOps.doKeyAgreement(ourKeyPair.getPrivate(), theirPublicKey);
    } catch (InvalidKeyException e) {
      // unreachable in practice
      throwHandshakeException(e);
    }

    ByteArrayOutputStream byteStream = new ByteArrayOutputStream();
    try {
      byteStream.write(rawMessage1);
      byteStream.write(rawMessage2);
    } catch (IOException e) {
      // unreachable in practice
      throwHandshakeException(e);
    }
    byte[] info = byteStream.toByteArray();

    byte[] salt = null;

    try {
      salt = "UKEY2 v1 auth".getBytes(UTF_8);
    } catch (UnsupportedEncodingException e) {
      // unreachable in practice
      throwHandshakeException(e);
    }

    byte[] authString = null;
    try {
      authString = CryptoOps.hkdf(derivedSecretKey, salt, info);
    } catch (InvalidKeyException | NoSuchAlgorithmException e) {
      // unreachable in practice
      throwHandshakeException(e);
    }

    handshakeState = InternalState.HANDSHAKE_VERIFICATION_IN_PROGRESS;
    return Arrays.copyOf(authString, byteLength);
  }

  /**
   * Invoked to let handshake state machine know that caller has validated the authentication
   * string obtained via {@link #getVerificationString(int)}; Note: this should only be called when
   * the state returned byte {@link #getHandshakeState()} is {@link State#VERIFICATION_IN_PROGRESS}.
   */
  public void verifyHandshake() {
    if (handshakeState != InternalState.HANDSHAKE_VERIFICATION_IN_PROGRESS) {
      throwIllegalStateException("Unexpected state: " + handshakeState);
    }
    handshakeState = InternalState.HANDSHAKE_FINISHED;
  }

  /**
   * Parses the given handshake message.
   * @throws AlertException if an error occurs that should be sent to other party.
   * @throws HandshakeException in an error occurs and the connection should be torn down.
   */
  public void parseHandshakeMessage(byte[] handshakeMessage)
      throws AlertException, HandshakeException {
    switch (handshakeState) {
      case SERVER_START:
        parseMessage1(handshakeMessage);
        handshakeState = InternalState.SERVER_AFTER_CLIENT_INIT;
        break;

      case CLIENT_WAITING_FOR_SERVER_INIT:
        parseMessage2(handshakeMessage);
        handshakeState = InternalState.CLIENT_AFTER_SERVER_INIT;
        break;

      case SERVER_WAITING_FOR_CLIENT_FINISHED:
        parseMessage3(handshakeMessage);
        handshakeState = InternalState.HANDSHAKE_VERIFICATION_NEEDED;
        break;

      default:
        throwIllegalStateException("Cannot parse message in state " + handshakeState);
    }
  }

  /**
   * Returns the current state of the handshake. See {@link State}.
   */
  public State getHandshakeState() {
    switch (handshakeState) {
      case CLIENT_START:
      case CLIENT_WAITING_FOR_SERVER_INIT:
      case CLIENT_AFTER_SERVER_INIT:
      case SERVER_START:
      case SERVER_WAITING_FOR_CLIENT_FINISHED:
      case SERVER_AFTER_CLIENT_INIT:
        // fallback intended -- these are all in-progress states
        return State.IN_PROGRESS;

      case HANDSHAKE_ERROR:
        return State.ERROR;

      case HANDSHAKE_VERIFICATION_NEEDED:
        return State.VERIFICATION_NEEDED;

      case HANDSHAKE_VERIFICATION_IN_PROGRESS:
        return State.VERIFICATION_IN_PROGRESS;

      case HANDSHAKE_FINISHED:
        return State.FINISHED;

      case HANDSHAKE_ALREADY_USED:
        return State.ALREADY_USED;

      default:
        // unreachable in practice
        throwIllegalStateException("Unknown state");
        return null; // really unreachable, but makes compiler happy
    }
  }

  /**
   * Can be called to generate a {@link D2DConnectionContext}. Note: this should only be called
   * when the state returned byte {@link #getHandshakeState()} is {@link State#FINISHED}.
   *
   * @throws HandshakeException
   */
  public D2DConnectionContext toConnectionContext() throws HandshakeException {
    switch (handshakeState) {
      case HANDSHAKE_ERROR:
        throwIllegalStateException("Cannot make context; handshake had error");
        return null; // makes linter happy
      case HANDSHAKE_ALREADY_USED:
        throwIllegalStateException("Cannot reuse handshake context; is has already been used");
        return null; // makes linter happy
      case HANDSHAKE_VERIFICATION_NEEDED:
        throwIllegalStateException("Handshake not verified, cannot create context");
        return null; // makes linter happy
      case HANDSHAKE_FINISHED:
        // We're done, okay to return a context
        break;
      default:
        // unreachable in practice
        throwIllegalStateException("Handshake is not complete; cannot create connection context");
    }

    if (derivedSecretKey == null) {
      throwIllegalStateException("Unexpected state error: derived key is null");
    }

    ByteArrayOutputStream byteStream = new ByteArrayOutputStream();
    try {
      byteStream.write(rawMessage1);
      byteStream.write(rawMessage2);
    } catch (IOException e) {
      // unreachable in practice
      throwHandshakeException(e);
    }
    byte[] info = byteStream.toByteArray();

    byte[] salt = null;
    try {
      salt = "UKEY2 v1 next".getBytes(UTF_8);
    } catch (UnsupportedEncodingException e) {
      // unreachable
      throwHandshakeException(e);
    }

    SecretKey nextProtocolKey = null;
    try {
      nextProtocolKey = new SecretKeySpec(CryptoOps.hkdf(derivedSecretKey, salt, info), "AES");
    } catch (InvalidKeyException | NoSuchAlgorithmException e) {
      // unreachable in practice
      throwHandshakeException(e);
    }

    SecretKey clientKey = null;
    SecretKey serverKey = null;
    try {
      clientKey = D2DCryptoOps.deriveNewKeyForPurpose(nextProtocolKey, "client");
      serverKey = D2DCryptoOps.deriveNewKeyForPurpose(nextProtocolKey, "server");
    } catch (InvalidKeyException | NoSuchAlgorithmException e) {
      // unreachable in practice
      throwHandshakeException(e);
    }

    handshakeState = InternalState.HANDSHAKE_ALREADY_USED;

    return new D2DConnectionContextV1(
        handshakeRole == HandshakeRole.CLIENT ? clientKey : serverKey,
        handshakeRole == HandshakeRole.CLIENT ? serverKey : clientKey,
        0 /* initial encode sequence number */,
        0 /* initial decode sequence number */);
  }

  /**
   * Generates the byte[] encoding of a {@link Ukey2ClientInit} message.
   *
   * @throws HandshakeException
   */
  private byte[] makeClientInitMessage() throws HandshakeException {
    Ukey2ClientInit.Builder clientInit = Ukey2ClientInit.newBuilder();
    clientInit.setVersion(VERSION);
    clientInit.setRandom(ByteString.copyFrom(generateRandomNonce()));
    clientInit.setNextProtocol(NEXT_PROTOCOL);

    // At the moment, we only support one cipher
    clientInit.addCipherCommitments(generateP256SHA512Commitment());

    return clientInit.build().toByteArray();
  }

  /**
   * Generates the byte[] encoding of a {@link Ukey2ServerInit} message.
   */
  private byte[] makeServerInitMessage() {
    Ukey2ServerInit.Builder serverInit = Ukey2ServerInit.newBuilder();
    serverInit.setVersion(VERSION);
    serverInit.setRandom(ByteString.copyFrom(generateRandomNonce()));
    serverInit.setHandshakeCipher(handshakeCipher.getValue());
    serverInit.setPublicKey(
        PublicKeyProtoUtil.encodePublicKey(ourKeyPair.getPublic()).toByteString());

    return serverInit.build().toByteArray();
  }

  /**
   * Generates a keypair for the provided handshake cipher. Currently only P256_SHA512 is
   * supported.
   *
   * @throws HandshakeException
   */
  private KeyPair genKeyPair(HandshakeCipher cipher) throws HandshakeException {
    switch (cipher) {
      case P256_SHA512:
        return PublicKeyProtoUtil.generateEcP256KeyPair();
      default:
        // Should never happen
        throwHandshakeException("unknown cipher: " + cipher);
    }
    return null; // unreachable, but makes compiler happy
  }

  /**
   * Attempts to parse message 1 (which is a wrapped {@link Ukey2ClientInit}). See go/ukey2 for
   * details.
   *
   * @throws AlertException if an error occurs
   */
  private void parseMessage1(byte[] handshakeMessage) throws AlertException, HandshakeException {
    // Deserialize the protobuf; send a BAD_MESSAGE message if deserialization fails
    Ukey2Message message = null;
    try {
      message = Ukey2Message.parseFrom(handshakeMessage);
    } catch (InvalidProtocolBufferException e) {
      throwAlertException(Ukey2Alert.AlertType.BAD_MESSAGE,
          "Can't parse message 1 " + e.getMessage());
    }

    // Verify that message_type == Type.CLIENT_INIT; send a BAD_MESSAGE_TYPE message if mismatch
    if (!message.hasMessageType() || message.getMessageType() != Ukey2Message.Type.CLIENT_INIT) {
      throwAlertException(
          Ukey2Alert.AlertType.BAD_MESSAGE_TYPE,
          "Expected, but did not find ClientInit message type");
    }

    // Deserialize message_data as a ClientInit message; send a BAD_MESSAGE_DATA message if
    // deserialization fails
    if (!message.hasMessageData()) {
      throwAlertException(Ukey2Alert.AlertType.BAD_MESSAGE_DATA,
          "Expected message data, but didn't find it");
    }
    Ukey2ClientInit clientInit = null;
    try {
      clientInit = Ukey2ClientInit.parseFrom(message.getMessageData());
    } catch (InvalidProtocolBufferException e) {
      throwAlertException(Ukey2Alert.AlertType.BAD_MESSAGE_DATA,
          "Can't parse message data into ClientInit");
    }

    // Check that version == VERSION; send BAD_VERSION message if mismatch
    if (!clientInit.hasVersion()) {
      throwAlertException(Ukey2Alert.AlertType.BAD_VERSION, "ClientInit missing version");
    }
    if (clientInit.getVersion() != VERSION) {
      throwAlertException(Ukey2Alert.AlertType.BAD_VERSION, "ClientInit version mismatch");
    }

    // Check that random is exactly NONCE_LENGTH_IN_BYTES bytes; send Alert.BAD_RANDOM message if
    // not.
    if (!clientInit.hasRandom()) {
      throwAlertException(Ukey2Alert.AlertType.BAD_RANDOM, "ClientInit missing random");
    }
    if (clientInit.getRandom().toByteArray().length != NONCE_LENGTH_IN_BYTES) {
      throwAlertException(Ukey2Alert.AlertType.BAD_RANDOM, "ClientInit has incorrect nonce length");
    }

    // Check to see if any of the handshake_cipher in cipher_commitment are acceptable. Servers
    // should select the first handshake_cipher that it finds acceptable to support clients
    // signaling deprecated but supported HandshakeCiphers. If no handshake_cipher is acceptable
    // (or there are no HandshakeCiphers in the message), the server sends a BAD_HANDSHAKE_CIPHER
    //  message
    List<Ukey2ClientInit.CipherCommitment> commitments = clientInit.getCipherCommitmentsList();
    if (commitments.isEmpty()) {
      throwAlertException(
          Ukey2Alert.AlertType.BAD_HANDSHAKE_CIPHER, "ClientInit is missing cipher commitments");
    }
    for (Ukey2ClientInit.CipherCommitment commitment : commitments) {
      if (!commitment.hasHandshakeCipher()
          || !commitment.hasCommitment()) {
        throwAlertException(
            Ukey2Alert.AlertType.BAD_HANDSHAKE_CIPHER,
            "ClientInit has improperly formatted cipher commitment");
      }

      // TODO(aczeskis): for now we only support one cipher, eventually support more
      if (commitment.getHandshakeCipher() == handshakeCipher.getValue()) {
        theirCommitment = commitment.getCommitment().toByteArray();
      }
    }
    if (theirCommitment == null) {
      throwAlertException(Ukey2Alert.AlertType.BAD_HANDSHAKE_CIPHER,
          "No acceptable commitments found");
    }

    // Checks that next_protocol contains a protocol that the server supports. Send a
    // BAD_NEXT_PROTOCOL message if not. We currently only support one protocol
    if (!clientInit.hasNextProtocol() || !NEXT_PROTOCOL.equals(clientInit.getNextProtocol())) {
      throwAlertException(Ukey2Alert.AlertType.BAD_NEXT_PROTOCOL, "Incorrect next protocol");
    }

    // Store raw message for AUTH_STRING computation
    rawMessage1 = handshakeMessage;
  }

  /**
   * Attempts to parse message 2 (which is a wrapped {@link Ukey2ServerInit}). See go/ukey2 for
   * details.
   */
  private void parseMessage2(final byte[] handshakeMessage)
      throws AlertException, HandshakeException {
    // Deserialize the protobuf; send a BAD_MESSAGE message if deserialization fails
    Ukey2Message message = null;
    try {
      message = Ukey2Message.parseFrom(handshakeMessage);
    } catch (InvalidProtocolBufferException e) {
      throwAlertException(Ukey2Alert.AlertType.BAD_MESSAGE,
          "Can't parse message 2 " + e.getMessage());
    }

    // Verify that message_type == Type.SERVER_INIT; send a BAD_MESSAGE_TYPE message if mismatch
    if (!message.hasMessageType()) {
      throwAlertException(Ukey2Alert.AlertType.BAD_MESSAGE_TYPE,
          "Expected, but did not find message type");
    }
    if (message.getMessageType() == Ukey2Message.Type.ALERT) {
      handshakeState = InternalState.HANDSHAKE_ERROR;
      throwHandshakeMessageFromAlertMessage(message);
    }
    if (message.getMessageType() != Ukey2Message.Type.SERVER_INIT) {
      throwAlertException(
          Ukey2Alert.AlertType.BAD_MESSAGE_TYPE,
          "Expected, but did not find SERVER_INIT message type");
    }

    // Deserialize message_data as a ServerInit message; send a BAD_MESSAGE_DATA message if
    // deserialization fails
    if (!message.hasMessageData()) {

      throwAlertException(Ukey2Alert.AlertType.BAD_MESSAGE_DATA,
          "Expected message data, but didn't find it");
    }
    Ukey2ServerInit serverInit = null;
    try {
      serverInit = Ukey2ServerInit.parseFrom(message.getMessageData());
    } catch (InvalidProtocolBufferException e) {
      throwAlertException(Ukey2Alert.AlertType.BAD_MESSAGE_DATA,
          "Can't parse message data into ServerInit");
    }

    // Check that version == VERSION; send BAD_VERSION message if mismatch
    if (!serverInit.hasVersion()) {
      throwAlertException(Ukey2Alert.AlertType.BAD_VERSION, "ServerInit missing version");
    }
    if (serverInit.getVersion() != VERSION) {
      throwAlertException(Ukey2Alert.AlertType.BAD_VERSION, "ServerInit version mismatch");
    }

    // Check that random is exactly NONCE_LENGTH_IN_BYTES bytes; send Alert.BAD_RANDOM message if
    // not.
    if (!serverInit.hasRandom()) {
      throwAlertException(Ukey2Alert.AlertType.BAD_RANDOM, "ServerInit missing random");
    }
    if (serverInit.getRandom().toByteArray().length != NONCE_LENGTH_IN_BYTES) {
      throwAlertException(Ukey2Alert.AlertType.BAD_RANDOM, "ServerInit has incorrect nonce length");
    }

    // Check that handshake_cipher matches a handshake cipher that was sent in
    // ClientInit.cipher_commitments. If not, send a BAD_HANDSHAKECIPHER message
    if (!serverInit.hasHandshakeCipher()) {
      throwAlertException(Ukey2Alert.AlertType.BAD_HANDSHAKE_CIPHER, "No handshake cipher found");
    }
    HandshakeCipher serverCipher = null;
    for (HandshakeCipher cipher : HandshakeCipher.values()) {
      if (cipher.getValue() == serverInit.getHandshakeCipher()) {
        serverCipher = cipher;
        break;
      }
    }
    if (serverCipher == null || serverCipher != handshakeCipher) {
      throwAlertException(Ukey2Alert.AlertType.BAD_HANDSHAKE_CIPHER,
          "No acceptable handshake cipher found");
    }

    // Check that public_key parses into a correct public key structure. If not, send a
    // BAD_PUBLIC_KEY message.
    if (!serverInit.hasPublicKey()) {
      throwAlertException(Ukey2Alert.AlertType.BAD_PUBLIC_KEY, "No public key found in ServerInit");
    }
    theirPublicKey = parseP256PublicKey(serverInit.getPublicKey().toByteArray());

    // Store raw message for AUTH_STRING computation
    rawMessage2 = handshakeMessage;
  }

  /**
   * Attempts to parse message 3 (which is a wrapped {@link Ukey2ClientFinished}). See go/ukey2 for
   * details.
   */
  private void parseMessage3(final byte[] handshakeMessage) throws HandshakeException {
    // Deserialize the protobuf; terminate the connection if deserialization fails.
    Ukey2Message message = null;
    try {
      message = Ukey2Message.parseFrom(handshakeMessage);
    } catch (InvalidProtocolBufferException e) {
      throwHandshakeException("Can't parse message 3", e);
    }

    // Verify that message_type == Type.CLIENT_FINISH; terminate connection if mismatch occurs
    if (!message.hasMessageType()) {
      throw new HandshakeException("Expected, but did not find message type");
    }
    if (message.getMessageType() == Ukey2Message.Type.ALERT) {
      throwHandshakeMessageFromAlertMessage(message);
    }
    if (message.getMessageType() != Ukey2Message.Type.CLIENT_FINISH) {
      throwHandshakeException("Expected, but did not find CLIENT_FINISH message type");
    }

    // Verify that the hash of the ClientFinished matches the expected commitment from ClientInit.
    // Terminate the connection if the expected match fails.
    verifyCommitment(handshakeMessage);

    // Deserialize message_data as a ClientFinished message; terminate the connection if
    // deserialization fails.
    if (!message.hasMessageData()) {
      throwHandshakeException("Expected message data, but didn't find it");
    }
    Ukey2ClientFinished clientFinished = null;
    try {
      clientFinished = Ukey2ClientFinished.parseFrom(message.getMessageData());
    } catch (InvalidProtocolBufferException e) {
      throwHandshakeException(e);
    }

    // Check that public_key parses into a correct public key structure. If not, terminate the
    // connection.
    if (!clientFinished.hasPublicKey()) {
      throwHandshakeException("No public key found in ClientFinished");
    }
    try {
      theirPublicKey = parseP256PublicKey(clientFinished.getPublicKey().toByteArray());
    } catch (AlertException e) {
      // Wrap in a HandshakeException because error should not be sent on the wire.
      throwHandshakeException(e);
    }
  }

  private void verifyCommitment(byte[] handshakeMessage) throws HandshakeException {
    byte[] actualClientFinishHash = null;
    switch (handshakeCipher) {
      case P256_SHA512:
        actualClientFinishHash = sha512(handshakeMessage);
        break;
      default:
        // should be unreachable
        throwIllegalStateException("Unexpected handshakeCipher");
    }

    // Time constant after Java SE 6 Update 17
    // See http://www.oracle.com/technetwork/java/javase/6u17-141447.html
    if (!MessageDigest.isEqual(actualClientFinishHash, theirCommitment)) {
      throwHandshakeException("Commitment does not match");
    }
  }

  private void throwHandshakeMessageFromAlertMessage(Ukey2Message message)
      throws HandshakeException {
    if (message.hasMessageData()) {
      Ukey2Alert alert = null;
      try {
        alert = Ukey2Alert.parseFrom(message.getMessageData());
      } catch (InvalidProtocolBufferException e) {
        throwHandshakeException("Cannot parse alert message", e);
      }

      if (alert.hasType() && alert.hasErrorMessage()) {
        throwHandshakeException(
            "Received Alert message. Type: "
                + alert.getType()
                + " Error Message: "
                + alert.getErrorMessage());
      } else if (alert.hasType()) {
        throwHandshakeException("Received Alert message. Type: " + alert.getType());
      }
    }

    throwHandshakeException("Received empty Alert Message");
  }

  /**
   * Parses an encoded public P256 key.
   */
  private PublicKey parseP256PublicKey(byte[] encodedPublicKey)
      throws AlertException, HandshakeException {
    try {
      return PublicKeyProtoUtil.parsePublicKey(GenericPublicKey.parseFrom(encodedPublicKey));
    } catch (InvalidProtocolBufferException | InvalidKeySpecException e) {
      throwAlertException(Ukey2Alert.AlertType.BAD_PUBLIC_KEY,
          "Cannot parse public key: " + e.getMessage());
      return null; // unreachable, but makes compiler happy
    }
  }

  /**
   * Generates a {@link CipherCommitment} for the P256_SHA512 cipher.
   */
  private CipherCommitment generateP256SHA512Commitment() throws HandshakeException {
    // Generate the corresponding finished message if it's not done yet
    if (!rawMessage3Map.containsKey(HandshakeCipher.P256_SHA512)) {
      generateP256SHA512ClientFinished(ourKeyPair);
    }

    CipherCommitment.Builder cipherCommitment = CipherCommitment.newBuilder();
    cipherCommitment.setHandshakeCipher(UkeyProto.Ukey2HandshakeCipher.P256_SHA512);
    cipherCommitment.setCommitment(
        ByteString.copyFrom(sha512(rawMessage3Map.get(HandshakeCipher.P256_SHA512))));

    return cipherCommitment.build();
  }

  /**
   * Generates and records a {@link Ukey2ClientFinished} message for the P256_SHA512 cipher.
   */
  private Ukey2ClientFinished generateP256SHA512ClientFinished(KeyPair p256KeyPair) {
    byte[] encodedKey = PublicKeyProtoUtil.encodePublicKey(p256KeyPair.getPublic()).toByteArray();

    Ukey2ClientFinished.Builder clientFinished = Ukey2ClientFinished.newBuilder();
    clientFinished.setPublicKey(ByteString.copyFrom(encodedKey));

    rawMessage3Map.put(
        HandshakeCipher.P256_SHA512,
        makeUkey2Message(Ukey2Message.Type.CLIENT_FINISH, clientFinished.build().toByteArray()));

    return clientFinished.build();
  }

  /**
   * Generates the serialized representation of a {@link Ukey2Message} based on the provided type
   * and data.
   */
  private byte[] makeUkey2Message(Ukey2Message.Type messageType, byte[] messageData) {
    Ukey2Message.Builder message = Ukey2Message.newBuilder();

    switch (messageType) {
      case ALERT:
      case CLIENT_INIT:
      case SERVER_INIT:
      case CLIENT_FINISH:
        // fall through intentional; valid message types
        break;
      default:
        throwIllegalArgumentException("Invalid message type: " + messageType);
    }
    message.setMessageType(messageType);

    // Alerts a blank message data field
    if (messageType != Ukey2Message.Type.ALERT) {
      if (messageData == null || messageData.length == 0) {
        throwIllegalArgumentException("Cannot send empty message data for non-alert messages");
      }
      message.setMessageData(ByteString.copyFrom(messageData));
    }

    return message.build().toByteArray();
  }

  /**
   * Returns a {@link Ukey2Alert} message of given type and having the loggable additional data if
   * present.
   */
  private Ukey2Alert makeAlertMessage(Ukey2Alert.AlertType alertType,
      @Nullable String loggableAdditionalData) throws HandshakeException {
    switch (alertType) {
      case BAD_MESSAGE:
      case BAD_MESSAGE_TYPE:
      case INCORRECT_MESSAGE:
      case BAD_MESSAGE_DATA:
      case BAD_VERSION:
      case BAD_RANDOM:
      case BAD_HANDSHAKE_CIPHER:
      case BAD_NEXT_PROTOCOL:
      case BAD_PUBLIC_KEY:
      case INTERNAL_ERROR:
        // fall through intentional; valid alert types
        break;
      default:
        throwHandshakeException("Unknown alert type: " + alertType);
    }

    Ukey2Alert.Builder alert = Ukey2Alert.newBuilder();
    alert.setType(alertType);

    if (loggableAdditionalData != null) {
      alert.setErrorMessage(loggableAdditionalData);
    }

    return alert.build();
  }

  /**
   * Generates a cryptoraphically random nonce of NONCE_LENGTH_IN_BYTES bytes.
   */
  private static byte[] generateRandomNonce() {
    SecureRandom rng = new SecureRandom();
    byte[] randomNonce = new byte[NONCE_LENGTH_IN_BYTES];
    rng.nextBytes(randomNonce);
    return randomNonce;
  }

  /**
   * Handy wrapper to do SHA512.
   */
  private byte[] sha512(byte[] input) throws HandshakeException {
    MessageDigest sha512;
    try {
      sha512 = MessageDigest.getInstance("SHA-512");
      return sha512.digest(input);
    } catch (NoSuchAlgorithmException e) {
      throwHandshakeException("No security provider initialized yet?", e);
      return null; // unreachable in practice, but makes compiler happy
    }
  }

  // Exception wrappers that remember to set the handshake state to ERROR

  private void throwAlertException(Ukey2Alert.AlertType alertType, String alertLogStatement)
      throws AlertException, HandshakeException {
    handshakeState = InternalState.HANDSHAKE_ERROR;
    throw new AlertException(alertLogStatement, makeAlertMessage(alertType, alertLogStatement));
  }

  private void throwHandshakeException(String logMessage) throws HandshakeException {
    handshakeState = InternalState.HANDSHAKE_ERROR;
    throw new HandshakeException(logMessage);
  }

  private void throwHandshakeException(Exception e) throws HandshakeException {
    handshakeState = InternalState.HANDSHAKE_ERROR;
    throw new HandshakeException(e);
  }

  private void throwHandshakeException(String logMessage, Exception e) throws HandshakeException {
    handshakeState = InternalState.HANDSHAKE_ERROR;
    throw new HandshakeException(logMessage, e);
  }

  private void throwIllegalStateException(String logMessage) {
    handshakeState = InternalState.HANDSHAKE_ERROR;
    throw new IllegalStateException(logMessage);
  }

  private void throwIllegalArgumentException(String logMessage) {
    handshakeState = InternalState.HANDSHAKE_ERROR;
    throw new IllegalArgumentException(logMessage);
  }
}
