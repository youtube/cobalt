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

import com.google.security.annotations.SuppressInsecureCipherModeCheckerReviewed;
import java.io.UnsupportedEncodingException;
import java.security.InvalidAlgorithmParameterException;
import java.security.InvalidKeyException;
import java.security.Key;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.security.PrivateKey;
import java.security.PublicKey;
import java.security.SecureRandom;
import java.security.Signature;
import java.security.SignatureException;
import javax.annotation.Nullable;
import javax.crypto.BadPaddingException;
import javax.crypto.Cipher;
import javax.crypto.IllegalBlockSizeException;
import javax.crypto.Mac;
import javax.crypto.NoSuchPaddingException;
import javax.crypto.SecretKey;
import javax.crypto.spec.IvParameterSpec;
import javax.crypto.spec.SecretKeySpec;

/**
 * Encapsulates the cryptographic operations used by the {@code SecureMessage*} classes.
 */
public class CryptoOps {
  
  private CryptoOps() {}  // Do not instantiate

  /**
   * Enum of supported signature types, with additional mappings to indicate the name of the
   * underlying JCA algorithm used to create the signature.
   * @see <a href=
   * "http://docs.oracle.com/javase/7/docs/technotes/guides/security/StandardNames.html">
   * Java Cryptography Architecture, Standard Algorithm Name Documentation</a>
   */
  public enum SigType {
    HMAC_SHA256(SecureMessageProto.SigScheme.HMAC_SHA256, "HmacSHA256", false),
    ECDSA_P256_SHA256(SecureMessageProto.SigScheme.ECDSA_P256_SHA256, "SHA256withECDSA", true),
    RSA2048_SHA256(SecureMessageProto.SigScheme.RSA2048_SHA256, "SHA256withRSA", true);

    public SecureMessageProto.SigScheme getSigScheme() {
      return sigScheme;
    }

    public String getJcaName() {
      return jcaName;
    }

    public boolean isPublicKeyScheme() {
      return publicKeyScheme;
    }

    public static SigType valueOf(SecureMessageProto.SigScheme sigScheme) {
      for (SigType value : values()) {
        if (value.sigScheme.equals(sigScheme)) {
          return value;
        }
      }
      throw new IllegalArgumentException("Unsupported SigType: " + sigScheme);
    }

    private final SecureMessageProto.SigScheme sigScheme;
    private final String jcaName;
    private final boolean publicKeyScheme;

    SigType(SecureMessageProto.SigScheme sigType, String jcaName, boolean publicKeyScheme) {
      this.sigScheme = sigType;
      this.jcaName = jcaName;
      this.publicKeyScheme = publicKeyScheme;
    }
  }

  /**
   * Enum of supported encryption types, with additional mappings to indicate the name of the
   * underlying JCA algorithm used to perform the encryption.
   * @see <a href=
   * "http://docs.oracle.com/javase/7/docs/technotes/guides/security/StandardNames.html">
   * Java Cryptography Architecture, Standard Algorithm Name Documentation</a>
   */
  public enum EncType {
    NONE(SecureMessageProto.EncScheme.NONE, "InvalidDoNotUseForJCA"),
    AES_256_CBC(SecureMessageProto.EncScheme.AES_256_CBC, "AES/CBC/PKCS5Padding");

    public SecureMessageProto.EncScheme getEncScheme() {
      return encScheme;
    }

    public String getJcaName() {
      return jcaName;
    }

    public static EncType valueOf(SecureMessageProto.EncScheme encScheme) {
      for (EncType value : values()) {
        if (value.encScheme.equals(encScheme)) {
          return value;
        }
      }
      throw new IllegalArgumentException("Unsupported EncType: " + encScheme);
    }

    private final SecureMessageProto.EncScheme encScheme;
    private final String jcaName;

    EncType(SecureMessageProto.EncScheme encScheme, String jcaName) {
      this.encScheme = encScheme;
      this.jcaName = jcaName;
    }
  }

  /**
   * Truncated hash output length, in bytes.
   */
  static final int DIGEST_LENGTH = 20;
  /**
   * A salt value specific to this library, generated as SHA-256("SecureMessage")
   */
  private static final byte[] SALT = sha256("SecureMessage");
  private static final byte[] CONSTANT_01 = { 0x01 };  // For convenience

  /**
   * Signs {@code data} using the algorithm specified by {@code sigType} with {@code signingKey}.
   *
   * @param rng is required for public key signature schemes
   * @return raw signature
   * @throws InvalidKeyException if {@code signingKey} is incompatible with {@code sigType}
   * @throws NoSuchAlgorithmException if the security provider is inadequate for {@code sigType}
   */
  static byte[] sign(
      SigType sigType, Key signingKey, @Nullable SecureRandom rng, byte[] data)
      throws InvalidKeyException, NoSuchAlgorithmException {
    if ((signingKey == null) || (data == null)) {
      throw new NullPointerException();
    }
    if (sigType.isPublicKeyScheme()) {
      if (rng == null) {
        throw new NullPointerException();
      }
      if (!(signingKey instanceof PrivateKey)) {
        throw new InvalidKeyException("Expected a PrivateKey");
      }
      Signature sigScheme = Signature.getInstance(sigType.getJcaName());
      sigScheme.initSign((PrivateKey) signingKey, rng);
      try {
        // We include a fixed magic value (salt) in the signature so that if the signing key is
        // reused in another context we can't be confused -- provided that the other user of the
        // signing key only signs statements that do not begin with this salt.
        sigScheme.update(SALT);
        sigScheme.update(data);
        return sigScheme.sign();
      } catch (SignatureException e) {
        throw new IllegalStateException(e);  // Consistent with failures in Mac.doFinal
      }
    } else {
      Mac macScheme = Mac.getInstance(sigType.getJcaName());
      // Note that an AES-256 SecretKey should work with most Mac schemes
      SecretKey derivedKey = deriveAes256KeyFor(getSecretKey(signingKey), getPurpose(sigType));
      macScheme.init(derivedKey);
      return macScheme.doFinal(data);
    }
  }

  /**
   * Verifies the {@code signature} on {@code data} using the algorithm specified by
   * {@code sigType} with {@code verificationKey}.
   *
   * @return true iff the signature is verified
   * @throws NoSuchAlgorithmException if the security provider is inadequate for {@code sigType}
   * @throws InvalidKeyException if {@code verificationKey} is incompatible with {@code sigType}
   * @throws SignatureException
   */
  static boolean verify(Key verificationKey, SigType sigType, byte[] signature, byte[] data)
      throws NoSuchAlgorithmException, InvalidKeyException, SignatureException {
    if ((verificationKey == null) || (signature == null) || (data == null)) {
      throw new NullPointerException();
    }
    if (sigType.isPublicKeyScheme()) {
      if (!(verificationKey instanceof PublicKey)) {
        throw new InvalidKeyException("Expected a PublicKey");
      }
      Signature sigScheme = Signature.getInstance(sigType.getJcaName());
      sigScheme.initVerify((PublicKey) verificationKey);
      sigScheme.update(SALT);  // See the comments in sign() for more on this
      sigScheme.update(data);
      return sigScheme.verify(signature);
    } else {
      Mac macScheme = Mac.getInstance(sigType.getJcaName());
      SecretKey derivedKey =
          deriveAes256KeyFor(getSecretKey(verificationKey), getPurpose(sigType));
      macScheme.init(derivedKey);
      return constantTimeArrayEquals(signature, macScheme.doFinal(data));
    }
  }

  /**
   * Generate a random IV appropriate for use with the algorithm specified in {@code encType}.
   *
   * @return a freshly generated IV (a random byte sequence of appropriate length)
   * @throws NoSuchAlgorithmException if the security provider is inadequate for {@code encType}
   */
  @SuppressInsecureCipherModeCheckerReviewed
  // See b/26525455 for security review.
  static byte[] generateIv(EncType encType, SecureRandom rng) throws NoSuchAlgorithmException {
    if (rng == null) {
      throw new NullPointerException();
    }
    try {
      Cipher encrypter = Cipher.getInstance(encType.getJcaName());
      byte[] iv = new byte[encrypter.getBlockSize()];
      rng.nextBytes(iv);
      return iv;
    } catch (NoSuchPaddingException e) {
      throw new NoSuchAlgorithmException(e);  // Consolidate into NoSuchAlgorithmException
    }
  }

  /**
   * Encrypts {@code plaintext} using the algorithm specified in {@code encType}, with the specified
   * {@code iv} and {@code encryptionKey}.
   *
   * @param rng source of randomness to be used with the specified cipher, if necessary
   * @return encrypted data
   * @throws NoSuchAlgorithmException if the security provider is inadequate for {@code encType}
   * @throws InvalidKeyException if {@code encryptionKey} is incompatible with {@code encType}
   */
  @SuppressInsecureCipherModeCheckerReviewed
  // See b/26525455 for security review.
  static byte[] encrypt(
      Key encryptionKey, EncType encType, @Nullable SecureRandom rng, byte[] iv, byte[] plaintext)
      throws NoSuchAlgorithmException, InvalidKeyException {
    if ((encryptionKey == null) || (iv == null) || (plaintext == null)) {
      throw new NullPointerException();
    }
    if (encType == EncType.NONE) {
      throw new NoSuchAlgorithmException("Cannot use NONE type here");
    }
    try {
      Cipher encrypter = Cipher.getInstance(encType.getJcaName());
      SecretKey derivedKey =
          deriveAes256KeyFor(getSecretKey(encryptionKey), getPurpose(encType));
      encrypter.init(Cipher.ENCRYPT_MODE, derivedKey, new IvParameterSpec(iv), rng);
      return encrypter.doFinal(plaintext);
    } catch (InvalidAlgorithmParameterException e) {
      throw new AssertionError(e);  // Should never happen
    } catch (IllegalBlockSizeException e) {
      throw new AssertionError(e);  // Should never happen
    } catch (BadPaddingException e) {
      throw new AssertionError(e);  // Should never happen
    } catch (NoSuchPaddingException e) {
      throw new NoSuchAlgorithmException(e);  // Consolidate into NoSuchAlgorithmException
    }
  }

  /**
   * Decrypts {@code ciphertext} using the algorithm specified in {@code encType}, with the
   * specified {@code iv} and {@code decryptionKey}.
   *
   * @return the plaintext (decrypted) data
   * @throws NoSuchAlgorithmException if the security provider is inadequate for {@code encType}
   * @throws InvalidKeyException if {@code decryptionKey} is incompatible with {@code encType}
   * @throws InvalidAlgorithmParameterException if {@code encType} exceeds legal cryptographic
   * strength limits in this jurisdiction
   * @throws IllegalBlockSizeException if {@code ciphertext} contains an illegal block
   * @throws BadPaddingException if {@code ciphertext} contains an illegal padding
   */
  @SuppressInsecureCipherModeCheckerReviewed
  // See b/26525455 for security review
  static byte[] decrypt(Key decryptionKey, EncType encType, byte[] iv, byte[] ciphertext)
      throws NoSuchAlgorithmException, InvalidKeyException, InvalidAlgorithmParameterException,
          IllegalBlockSizeException, BadPaddingException {
    if ((decryptionKey == null) || (iv == null) || (ciphertext == null)) {
      throw new NullPointerException();
    }
    if (encType == EncType.NONE) {
      throw new NoSuchAlgorithmException("Cannot use NONE type here");
    }
    try {
      Cipher decrypter = Cipher.getInstance(encType.getJcaName());
      SecretKey derivedKey =
          deriveAes256KeyFor(getSecretKey(decryptionKey), getPurpose(encType));
      decrypter.init(Cipher.DECRYPT_MODE, derivedKey, new IvParameterSpec(iv));
      return decrypter.doFinal(ciphertext);
    } catch (NoSuchPaddingException e) {
      throw new AssertionError(e);  // Should never happen
    }
  }

  /**
   * Computes a collision-resistant hash of {@link #DIGEST_LENGTH} bytes
   * (using a truncated SHA-256 output).
   */
  static byte[] digest(byte[] data) throws NoSuchAlgorithmException {
    MessageDigest sha256 = MessageDigest.getInstance("SHA-256");
    byte[] truncatedHash = new byte[DIGEST_LENGTH];
    System.arraycopy(sha256.digest(data), 0, truncatedHash, 0, DIGEST_LENGTH);
    return truncatedHash;
  }

  /**
   * Returns {@code true} if the two arrays are equal to one another.
   * When the two arrays differ in length, trivially returns {@code false}.
   * When the two arrays are equal in length, does a constant-time comparison
   * of the two, i.e. does not abort the comparison when the first differing
   * element is found.
   *
   * <p>NOTE: This is a copy of {@code java/com/google/math/crypto/ConstantTime#arrayEquals}.
   *
   * @param a An array to compare
   * @param b Another array to compare
   * @return {@code true} if these arrays are both null or if they have equal
   *         length and equal bytes in all elements
   */
  static boolean constantTimeArrayEquals(@Nullable byte[] a, @Nullable byte[] b) {
    if (a == null || b == null) {
      return (a == b);
    }
    if (a.length != b.length) {
      return false;
    }
    byte result = 0;
    for (int i = 0; i < b.length; i++) {
      result = (byte) (result | a[i] ^ b[i]);
    }
    return (result == 0);
  }

  // @VisibleForTesting
  static String getPurpose(SigType sigType) {
    return "SIG:" + sigType.getSigScheme().getNumber();
  }

  // @VisibleForTesting
  static String getPurpose(EncType encType) {
    return "ENC:" + encType.getEncScheme().getNumber();
  }

  private static SecretKey getSecretKey(Key key) throws InvalidKeyException {
    if (!(key instanceof SecretKey)) {
      throw new InvalidKeyException("Expected a SecretKey");
    }
    return (SecretKey) key;
  }

  /**
   * @return the UTF-8 encoding of the given string
   * @throws RuntimeException if the UTF-8 charset is not present.
   */
  public static byte[] utf8StringToBytes(String input) {
    try {
      return input.getBytes("UTF-8");
    } catch (UnsupportedEncodingException e) {
      throw new RuntimeException(e);  // Shouldn't happen, UTF-8 is universal
    }
  }

  /**
   * @return SHA-256(UTF-8 encoded input)
   */
  public static byte[] sha256(String input) {
    MessageDigest sha256;
    try {
      sha256 = MessageDigest.getInstance("SHA-256");
      return sha256.digest(utf8StringToBytes(input));
    } catch (NoSuchAlgorithmException e) {
      throw new RuntimeException("No security provider initialized yet?", e);
    }
  }

  /**
   * A key derivation function specific to this library, which accepts a {@code masterKey} and an
   * arbitrary {@code purpose} describing the intended application of the derived sub-key,
   * and produces a derived AES-256 key safe to use as if it were independent of any other
   * derived key which used a different {@code purpose}.
   *
   * @param masterKey any key suitable for use with HmacSHA256
   * @param purpose a UTF-8 encoded string describing the intended purpose of derived key
   * @return a derived SecretKey suitable for use with AES-256
   * @throws InvalidKeyException if the encoded form of {@code masterKey} cannot be accessed
   */
  static SecretKey deriveAes256KeyFor(SecretKey masterKey, String purpose)
      throws NoSuchAlgorithmException, InvalidKeyException {
    return new SecretKeySpec(hkdf(masterKey, SALT, utf8StringToBytes(purpose)), "AES");
  }

  /**
   * Implements HKDF (RFC 5869) with the SHA-256 hash and a 256-bit output key length.
   *
   * Please make sure to select a salt that is fixed and unique for your codebase, and use the
   * {@code info} parameter to specify any additional bits that should influence the derived key.
   *
   * @param inputKeyMaterial master key from which to derive sub-keys
   * @param salt a (public) randomly generated 256-bit input that can be re-used
   * @param info arbitrary information that is bound to the derived key (i.e., used in its creation)
   * @return raw derived key bytes = HKDF-SHA256(inputKeyMaterial, salt, info)
   * @throws InvalidKeyException if the encoded form of {@code inputKeyMaterial} cannot be accessed
   */
  public static byte[] hkdf(SecretKey inputKeyMaterial, byte[] salt, byte[] info)
      throws NoSuchAlgorithmException, InvalidKeyException {
    if ((inputKeyMaterial == null) || (salt == null) || (info == null)) {
      throw new NullPointerException();
    }
    return hkdfSha256Expand(hkdfSha256Extract(inputKeyMaterial, salt), info);
  }

  /**
   * @return the concatenation of {@code a} and {@code b}, treating {@code null} as the empty array.
   */
  static byte[] concat(@Nullable byte[] a, @Nullable byte[] b) {
    if ((a == null) && (b == null)) {
      return new byte[] { };
    }
    if (a == null) {
      return b;
    }
    if (b == null) {
      return a;
    }
    byte[] result = new byte[a.length + b.length];
    System.arraycopy(a, 0, result, 0, a.length);
    System.arraycopy(b, 0, result, a.length, b.length);
    return result;
  }

  /**
   * Since {@code Arrays.copyOfRange(...)} is not available on older Android platforms,
   * a custom method for computing a subarray is provided here.
   *
   * @return the substring of {@code in} from {@code beginIndex} (inclusive)
   *   up to {@code endIndex} (exclusive)
   */
  static byte[] subarray(byte[] in, int beginIndex, int endIndex) {
    if (in == null) {
      throw new NullPointerException();
    }
    int length = endIndex - beginIndex;
    if ((length < 0)
        || (beginIndex < 0)
        || (endIndex < 0)
        || (beginIndex >= in.length)
        || (endIndex > in.length)) {
      throw new IndexOutOfBoundsException();
    }
    byte[] result = new byte[length];
    if (length > 0) {
      System.arraycopy(in, beginIndex, result, 0, length);
    }
    return result;
  }

  /**
   * The HKDF (RFC 5869) extraction function, using the SHA-256 hash function. This function is
   * used to pre-process the inputKeyMaterial and mix it with the salt, producing output suitable
   * for use with HKDF expansion function (which produces the actual derived key).
   *
   * @see #hkdfSha256Expand(byte[], byte[])
   * @return HMAC-SHA256(salt, inputKeyMaterial)  (salt is the "key" for the HMAC)
   * @throws InvalidKeyException if the encoded form of {@code inputKeyMaterial} cannot be accessed
   * @throws NoSuchAlgorithmException if the HmacSHA256 or AES algorithms are unavailable
   */
  private static byte[] hkdfSha256Extract(SecretKey inputKeyMaterial, byte[] salt)
      throws NoSuchAlgorithmException, InvalidKeyException {
    Mac macScheme = Mac.getInstance("HmacSHA256");
    try {
      macScheme.init(new SecretKeySpec(salt, "AES"));
    } catch (InvalidKeyException e) {
      throw new AssertionError(e);  // This should never happen
    }
    // Note that the SecretKey encoding format is defined to be RAW, so the encoded form should be
    // consistent across implementations.
    byte[] encodedKeyMaterial = inputKeyMaterial.getEncoded();
    if (encodedKeyMaterial == null) {
      throw new InvalidKeyException("Cannot get encoded form of SecretKey");
    }
    return macScheme.doFinal(encodedKeyMaterial);
  }

  /**
   * Special case of HKDF (RFC 5869) expansion function, using the SHA-256 hash function and
   * allowing for a maximum output length of 256 bits.
   *
   * @param pseudoRandomKey should be generated by {@link #hkdfSha256Expand(byte[], byte[])}
   * @param info arbitrary information the derived key should be bound to
   * @return raw derived key bytes = HMAC-SHA256(pseudoRandomKey, info | 0x01)
   * @throws NoSuchAlgorithmException if the HmacSHA256 or AES algorithms are unavailable
   */
  private static byte[] hkdfSha256Expand(byte[] pseudoRandomKey, byte[] info)
      throws NoSuchAlgorithmException {
    Mac macScheme = Mac.getInstance("HmacSHA256");
    try {
      macScheme.init(new SecretKeySpec(pseudoRandomKey, "AES"));
    } catch (InvalidKeyException e) {
      throw new AssertionError(e);  // This should never happen
    }
    // Arbitrary "info" to be included in the MAC.
    macScheme.update(info);
    // Note that RFC 5869 computes number of blocks N = ceil(hash length / output length), but
    // here we only deal with a 256 bit hash up to a 256 bit output, yielding N=1.
    return macScheme.doFinal(CONSTANT_01);
  }

}
