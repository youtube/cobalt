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

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import javax.crypto.SecretKey;

/**
 * Implementation of {@link D2DConnectionContext} for version 0 of the D2D protocol. In this
 * version, communication is half-duplex, as there is a shared key and a shared sequence number
 * between the two sides.
 */
public class D2DConnectionContextV0 extends D2DConnectionContext {
  public static final int PROTOCOL_VERSION = 0;

  private final SecretKey sharedKey;
  private int sequenceNumber;

  /**
   * Package private constructor. Should never be called directly except by the
   * {@link D2DHandshakeContext}
   *
   * @param sharedKey
   * @param initialSequenceNumber
   */
  D2DConnectionContextV0(SecretKey sharedKey, int initialSequenceNumber) {
    super(PROTOCOL_VERSION);
    this.sharedKey = sharedKey;
    this.sequenceNumber = initialSequenceNumber;
  }

  @Override
  public byte[] getSessionUnique() throws NoSuchAlgorithmException {
    if (sharedKey == null) {
      throw new IllegalStateException(
          "Connection has not been correctly initialized; shared key is null");
    }

    MessageDigest md = MessageDigest.getInstance("SHA-256");
    md.update(D2DCryptoOps.SALT);
    return md.digest(sharedKey.getEncoded());
  }

  @Override
  protected void incrementSequenceNumberForEncoding() {
    sequenceNumber++;
  }

  @Override
  protected void incrementSequenceNumberForDecoding() {
    sequenceNumber++;
  }

  @Override
  int getSequenceNumberForEncoding() {
    return sequenceNumber;
  }

  @Override
  int getSequenceNumberForDecoding() {
    return sequenceNumber;
  }

  @Override
  SecretKey getEncodeKey() {
    return sharedKey;
  }

  @Override
  SecretKey getDecodeKey() {
    return sharedKey;
  }

  /**
   * Structure of saved session is:
   * +-----------------------------------------------------+
   * |     1 Byte       | 4 Bytes (big endian) | 32 Bytes  |
   * +-----------------------------------------------------+
   * | Protocol Version |    sequence number   |    key    |
   * +-----------------------------------------------------+
   */
  @Override
  public byte[] saveSession() {
    ByteArrayOutputStream bytes = new ByteArrayOutputStream();

    try {
      // Protocol version
      bytes.write(0);

      // Sequence number
      bytes.write(signedIntToBytes(sequenceNumber));

      // Key
      bytes.write(sharedKey.getEncoded());
    } catch (IOException e) {
      // should not happen
      e.printStackTrace();
      return null;
    }

    return bytes.toByteArray();
  }
}
