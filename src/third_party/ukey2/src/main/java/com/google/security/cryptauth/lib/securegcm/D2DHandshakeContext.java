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

/**
 * Describes a cryptographic handshake with arbitrary number of round trips.  Some handshake
 * messages may also send a payload.
 *
 * <p>Generic usage for handshake Initiator:
 * {@code
 *   // Handshake Initiator
 *   D2DHandshakeContext handshake = <specific handshake>.forInitiator();
 *   while (!handshake.isHandshakeComplete()) {
 *     try {
 *       // Get the next handshake message to send
 *       byte[] initiatorMessage = handshake.getNextHandshakeMessage();
 *
 *       // Send the message out and get the response
 *       socket.send(initiatorMessage);
 *       byte[] responderMessage = socket.read();
 *
 *       // Handle the response and obtain the optional payload
 *       byte[] payload = handshake.parseHandshakeMessage(responderMessage);
 *
 *       // Handle the payload if one was sent
 *       if (payload.length > 0) {
 *         handlePayload(payload);
 *       }
 *     } catch (HandshakeException e) {
 *       // Handshake has failed, bail
 *       Log("Handshake failed!", e);
 *       return;
 *     }
 *   }
 *
 *   ConnectionContext connectionContext;
 *   try {
 *     // Upgrade handshake context to a full connection context
 *     connectionContext = handshake.toConnectionContext();
 *   } catch (HandshakeException e) {
 *     Log("Cannot convert handshake to connection context", e);
 *   }
 * }
 *
 * <p>Generic usage for handshake Responder:
 * {@code
 *   // Handshake Responder
 *   D2DHandshakeContext handshake = <specific handshake>.forResponder();
 *
 *   while (!handshake.isHandshakeComplete()) {
 *     try {
 *       // Get the message from the initiator
 *       byte[] initiatorMessage = socket.read();
 *
 *       // Handle the message and get the payload if it exists
 *       byte[] payload = handshake.parseHandshakeMessage(initiatorMessage);
 *
 *       // Handle the payload if one was sent
 *       if (payload.length > 0) {
 *         handlePayload(payload);
 *       }
 *
 *       // Make sure that wasn't the last message
 *       if (handshake.isHandshakeComplete()) {
 *         break;
 *       }
 *
 *       // Get next message to send and send it
 *       byte[] responderMessage = handshake.getNextHandshakeMessage();
 *       socket.send(responderMessage);
 *     } catch (HandshakeException e) {
 *       // Handshake has failed, bail
 *       Log("Handshake failed!", e);
 *       return;
 *     }
 *   }
 *
 *   ConnectionContext connectionContext;
 *   try {
 *     // Upgrade handshake context to a full connection context
 *     connectionContext = handshake.toConnectionContext();
 *   } catch (HandshakeException e) {
 *     Log("Cannot convert handshake to connection context", e);
 *   }
 * }
 */
public interface D2DHandshakeContext {

  /**
   * Tells the caller whether the handshake has completed or not. If the handshake is complete, the
   * caller may call {@link #toConnectionContext()} to obtain a connection context.
   *
   * @return true if the handshake is complete, false otherwise
   */
  boolean isHandshakeComplete();

  /**
   * Constructs the next message that should be sent in the handshake.
   *
   * @return the next message
   * @throws HandshakeException if the handshake is over or if the next handshake message can't be
   *     obtained (e.g., there is an internal error)
   */
  byte[] getNextHandshakeMessage() throws HandshakeException;

  /**
   * Tells the caller whether the next handshake message may carry a payload. If true, caller may
   * call {@link #getNextHandshakeMessage(byte[])} instead of the regular
   * {@link #getNextHandshakeMessage()}. If false, calling {@link #getNextHandshakeMessage(byte[])}
   * will result in a {@link HandshakeException}.
   *
   * @return true if the next handshake message can carry a payload, false otherwise
   */
  boolean canSendPayloadInHandshakeMessage();

  /**
   * Constructs the next message that should be sent in the handshake along with a payload. Caller
   * should verify that this method can be called by calling
   * {@link #canSendPayloadInHandshakeMessage()}.
   *
   * @param payload the payload to include in the handshake message
   * @return the next message
   * @throws HandshakeException if the handshake is over or if the next handshake message can't be
   *     obtained (e.g., there is an internal error) or if the payload may not be included in this
   *     message
   */
  byte[] getNextHandshakeMessage(byte[] payload) throws HandshakeException;

  /**
   * Parses a handshake message and returns the included payload (if any).
   *
   * @param handshakeMessage message received in the handshake
   * @return payload or empty byte[] if no payload was in the handshake message
   * @throws HandshakeException if an error occurs in parsing the handshake message
   */
  byte[] parseHandshakeMessage(byte[] handshakeMessage) throws HandshakeException;

  /**
   * Creates a full {@link D2DConnectionContext}. May only be called if
   * {@link #isHandshakeComplete()} returns true.
   *
   * @return a full {@link D2DConnectionContext}
   * @throws HandshakeException if a connection context cannot be created
   */
  D2DConnectionContext toConnectionContext() throws HandshakeException;
}
