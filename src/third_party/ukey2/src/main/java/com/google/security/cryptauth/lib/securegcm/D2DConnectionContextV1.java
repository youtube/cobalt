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
import java.util.Arrays;
import javax.crypto.SecretKey;

/**
 * Implementation of {@link D2DConnectionContext} for version 1 of the D2D protocol. In this
 * version, communication is fully duplex, as separate keys and sequence nubmers are used for
 * encoding and decoding.
 */
public class D2DConnectionContextV1 extends D2DConnectionContext {
  public static final int PROTOCOL_VERSION = 1;

  private final SecretKey encodeKey;
  private final SecretKey decodeKey;
  private int encodeSequenceNumber;
  private int decodeSequenceNumber;

  /**
   * Package private constructor. Should never be called directly except by the
   * {@link D2DHandshakeContext}
   *
   * @param encodeKey
   * @param decodeKey
   * @param initialEncodeSequenceNumber
   * @param initialDecodeSequenceNumber
   */
  D2DConnectionContextV1(
      SecretKey encodeKey,
      SecretKey decodeKey,
      int initialEncodeSequenceNumber,
      int initialDecodeSequenceNumber) {
    super(PROTOCOL_VERSION);
    this.encodeKey = encodeKey;
    this.decodeKey = decodeKey;
    this.encodeSequenceNumber = initialEncodeSequenceNumber;
    this.decodeSequenceNumber = initialDecodeSequenceNumber;
  }

  @Override
  public byte[] getSessionUnique() throws NoSuchAlgorithmException {
    if (encodeKey == null || decodeKey == null) {
      throw new IllegalStateException(
          "Connection has not been correctly initialized; encode key or decode key is null");
    }

    // Ensure that the initator and responder keys are hashed in a deterministic order, so they have
    // the same session unique code.
    byte[] encodeKeyBytes = encodeKey.getEncoded();
    byte[] decodeKeyBytes = decodeKey.getEncoded();
    int encodeKeyHash = Arrays.hashCode(encodeKeyBytes);
    int decodeKeyHash = Arrays.hashCode(decodeKeyBytes);
    byte[] firstKeyBytes = encodeKeyHash < decodeKeyHash ? encodeKeyBytes : decodeKeyBytes;
    byte[] secondKeyBytes = firstKeyBytes == encodeKeyBytes ? decodeKeyBytes : encodeKeyBytes;

    MessageDigest md = MessageDigest.getInstance("SHA-256");
    md.update(D2DCryptoOps.SALT);
    md.update(firstKeyBytes);
    md.update(secondKeyBytes);
    return md.digest();
  }

  @Override
  protected void incrementSequenceNumberForEncoding() {
    encodeSequenceNumber++;
  }

  @Override
  protected void incrementSequenceNumberForDecoding() {
    decodeSequenceNumber++;
  }

  @Override
  int getSequenceNumberForEncoding() {
    return encodeSequenceNumber;
  }

  @Override
  int getSequenceNumberForDecoding() {
    return decodeSequenceNumber;
  }

  @Override
  SecretKey getEncodeKey() {
    return encodeKey;
  }

  @Override
  SecretKey getDecodeKey() {
    return decodeKey;
  }

  /**
   * Structure of saved session is:
   * +------------------------------------------------------------------------------------------+
   * |     1 Byte       | 4 Bytes (big endian) | 4 Bytes (big endian) |  32 Bytes  |  32 Bytes  |
   * +------------------------------------------------------------------------------------------+
   * | Protocol Version |   encode seq number  |   decode seq number  | encode key | decode key |
   * +------------------------------------------------------------------------------------------+
   */
  @Override
  public byte[] saveSession() {
    ByteArrayOutputStream bytes = new ByteArrayOutputStream();

    try {
      // Protocol version
      bytes.write(1);

      // Encode sequence number
      bytes.write(signedIntToBytes(encodeSequenceNumber));

      // Decode sequence number
      bytes.write(signedIntToBytes(decodeSequenceNumber));

      // Encode Key
      bytes.write(encodeKey.getEncoded());

      // Decode Key
      bytes.write(decodeKey.getEncoded());
    } catch (IOException e) {
      // should not happen
      e.printStackTrace();
      return null;
    }

    return bytes.toByteArray();
  }
}
