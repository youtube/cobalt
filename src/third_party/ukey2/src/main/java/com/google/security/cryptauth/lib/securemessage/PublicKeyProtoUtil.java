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

import com.google.protobuf.ByteString;
import com.google.security.annotations.SuppressInsecureCipherModeCheckerPendingReview;
import com.google.security.cryptauth.lib.securemessage.SecureMessageProto.DhPublicKey;
import com.google.security.cryptauth.lib.securemessage.SecureMessageProto.EcP256PublicKey;
import com.google.security.cryptauth.lib.securemessage.SecureMessageProto.GenericPublicKey;
import com.google.security.cryptauth.lib.securemessage.SecureMessageProto.SimpleRsaPublicKey;
import java.math.BigInteger;
import java.security.InvalidAlgorithmParameterException;
import java.security.KeyFactory;
import java.security.KeyPair;
import java.security.KeyPairGenerator;
import java.security.NoSuchAlgorithmException;
import java.security.PublicKey;
import java.security.SecureRandom;
import java.security.interfaces.ECPublicKey;
import java.security.interfaces.RSAPublicKey;
import java.security.spec.ECFieldFp;
import java.security.spec.ECGenParameterSpec;
import java.security.spec.ECParameterSpec;
import java.security.spec.ECPoint;
import java.security.spec.ECPublicKeySpec;
import java.security.spec.InvalidKeySpecException;
import java.security.spec.RSAPublicKeySpec;
import javax.crypto.interfaces.DHPrivateKey;
import javax.crypto.interfaces.DHPublicKey;
import javax.crypto.spec.DHParameterSpec;
import javax.crypto.spec.DHPublicKeySpec;

/**
 * Utility class containing static factory methods for a simple protobuf based representation of
 * EC public keys that is intended for use with the SecureMessage library.
 *
 * N.B.: Requires the availability of an EC security provider supporting the NIST P-256 curve.
 *
 */
public class PublicKeyProtoUtil {

  private PublicKeyProtoUtil() {}  // Do not instantiate

  /**
   * Caches state about whether the current platform supports Elliptic Curve algorithms.
   */
  private static final Boolean IS_LEGACY_CRYPTO_REQUIRED = determineIfLegacyCryptoRequired();

  private static final BigInteger ONE = new BigInteger("1");
  private static final BigInteger TWO = new BigInteger("2");

  /**
   * Name for Elliptic Curve cryptography algorithm suite, used by the security provider. If the
   * security provider does not implement the specified algorithm, runtime errors will ensue.
   */
  private static final String EC_ALG = "EC";

  /**
   * A common name for the NIST P-256 curve, used by most Java security providers.
   */
  private static final String EC_P256_COMMON_NAME = "secp256r1";

  /**
   * A name the NIST P-256 curve, used by the OpenSSL Java security provider (e.g,. on Android).
   */
  private static final String EC_P256_OPENSSL_NAME = "prime256v1";

  /**
   * The {@link ECParameterSpec} for the NIST P-256 Elliptic Curve.
   */
  private static final ECParameterSpec EC_P256_PARAMS = isLegacyCryptoRequired() ? null :
      ((ECPublicKey) generateEcP256KeyPair().getPublic()).getParams();

  /**
   * The prime {@code p} describing the field for the NIST P-256 curve.
   */
  private static final BigInteger EC_P256_P = isLegacyCryptoRequired() ? null :
      ((ECFieldFp) EC_P256_PARAMS.getCurve().getField()).getP();

  /**
   * The coefficient {@code a} for the NIST P-256 curve.
   */
  private static final BigInteger EC_P256_A = isLegacyCryptoRequired() ? null :
      EC_P256_PARAMS.getCurve().getA();

  /**
   * The coefficient {@code b} for the NIST P-256 curve.
   */
  private static final BigInteger EC_P256_B = isLegacyCryptoRequired() ? null :
      EC_P256_PARAMS.getCurve().getB();

  /**
   * Maximum number of bytes in a 2's complement encoding of a NIST P-256 elliptic curve point.
   */
  private static final int MAX_P256_ENCODING_BYTES = 33;

  /**
   * The JCA name for the RSA cryptography suite.
   */
  private static final String RSA_ALG = "RSA";

  private static final int RSA2048_MODULUS_BITS = 2048;

  /**
   * Maximum number of bytes in a 2's complement encoding of a 2048-bit RSA key.
   */
  private static final int MAX_RSA2048_ENCODING_BYTES = 257;

  /**
   * The JCA name for the Diffie-Hellman cryptography suite.
   */
  private static final String DH_ALG = "DH";

  /**
   * The prime from the 2048-bit MODP Group (group 14) described in RFC 3526, to be used for
   * Diffie-Hellman computations. Use only if Elliptic Curve ciphers are unavailable.
   */
  public static final BigInteger DH_P = new BigInteger(
      "FFFFFFFFFFFFFFFFC90FDAA22168C234C4C6628B80DC1CD1" +
      "29024E088A67CC74020BBEA63B139B22514A08798E3404DD" +
      "EF9519B3CD3A431B302B0A6DF25F14374FE1356D6D51C245" +
      "E485B576625E7EC6F44C42E9A637ED6B0BFF5CB6F406B7ED" +
      "EE386BFB5A899FA5AE9F24117C4B1FE649286651ECE45B3D" +
      "C2007CB8A163BF0598DA48361C55D39A69163FA8FD24CF5F" +
      "83655D23DCA3AD961C62F356208552BB9ED529077096966D" +
      "670C354E4ABC9804F1746C08CA18217C32905E462E36CE3B" +
      "E39E772C180E86039B2783A2EC07A28FB5C55DF06F4C52C9" +
      "DE2BCBF6955817183995497CEA956AE515D2261898FA0510" +
      "15728E5A8AACAA68FFFFFFFFFFFFFFFF", 16);

  /**
   * The generator for the 2048-bit MODP Group (group 14) described in RFC 3526, to be used for
   * Diffie-Hellman computations. Use only if Elliptic Curve ciphers are unavailable.
   */
  public static final BigInteger DH_G = TWO;

  /**
   * The size of the Diffie-Hellman exponent to use, in bits.
   */
  public static final int DH_LEN = 512;

  /**
   * Maximum number of bytes in a 2's complement encoding of a
   * Diffie-Hellman key using {@link #DH_G}.
   */
  private static final int MAX_DH2048_ENCODING_BYTES = 257;

  /**
   * Version code for the Honeycomb release of Android, which is the first release supporting
   * Elliptic Curve.
   */
  public static final int ANDROID_HONEYCOMB_SDK_INT = 11;

  /**
   * Encodes any supported {@link PublicKey} type as a {@link GenericPublicKey} proto message.
   *
   * @see SecureMessageProto constants (defined in the .proto file) for supported types
   */
  public static GenericPublicKey encodePublicKey(PublicKey pk) {
    if (pk == null) {
      throw new NullPointerException();
    }
    if (pk instanceof ECPublicKey) {
      return GenericPublicKey.newBuilder()
          .setType(SecureMessageProto.PublicKeyType.EC_P256)
          .setEcP256PublicKey(encodeEcPublicKey(pk))
          .build();
    }
    if (pk instanceof RSAPublicKey) {
      return GenericPublicKey.newBuilder()
          .setType(SecureMessageProto.PublicKeyType.RSA2048)
          .setRsa2048PublicKey(encodeRsa2048PublicKey(pk))
          .build();
    }
    if (pk instanceof DHPublicKey) {
      return GenericPublicKey.newBuilder()
          .setType(SecureMessageProto.PublicKeyType.DH2048_MODP)
          .setDh2048PublicKey(encodeDh2048PublicKey(pk))
          .build();
    }
    throw new IllegalArgumentException("Unsupported PublicKey type");
  }

  /**
   * Encodes an {@link ECPublicKey} to an {@link EcP256PublicKey} proto message.
   */
  public static EcP256PublicKey encodeEcPublicKey(PublicKey pk) {
    ECPublicKey epk = pkToECPublicKey(pk);
    return EcP256PublicKey.newBuilder()
        .setX(extractX(epk))
        .setY(extractY(epk))
        .build();
  }

  /**
   * Encodes a 2048-bit {@link RSAPublicKey} to an {@link SimpleRsaPublicKey} proto message.
   */
  public static SimpleRsaPublicKey encodeRsa2048PublicKey(PublicKey pk) {
    RSAPublicKey rpk = pkToRSAPublicKey(pk);
    return SimpleRsaPublicKey.newBuilder()
        .setN(ByteString.copyFrom(rpk.getModulus().toByteArray()))
        .setE(rpk.getPublicExponent().intValue())
        .build();
  }

  /**
   * Encodes a 2048-bit {@link DhPublicKey} using the {@link #DH_G} group to a
   * {@link DhPublicKey} proto message.
   */
  public static DhPublicKey encodeDh2048PublicKey(PublicKey pk) {
    DHPublicKey dhpk = pkToDHPublicKey(pk);
    return DhPublicKey.newBuilder()
        .setY(ByteString.copyFrom(dhpk.getY().toByteArray()))
        .build();
  }

  /**
   * Extracts a {@link PublicKey} from an {@link GenericPublicKey} proto message.
   *
   * @throws InvalidKeySpecException if the input is not a valid and/or supported public key type
   */
  public static PublicKey parsePublicKey(GenericPublicKey gpk) throws InvalidKeySpecException {
    if (!gpk.hasType()) {
      // "required" means nothing in micro proto land. We have to check this ourselves.
      throw new InvalidKeySpecException("GenericPublicKey.type is a required field");
    }
    switch (gpk.getType()) {
      case EC_P256:
        if (!gpk.hasEcP256PublicKey()) {
          break;
        }
        return parseEcPublicKey(gpk.getEcP256PublicKey());
      case RSA2048:
        if (!gpk.hasRsa2048PublicKey()) {
          break;
        }
        return parseRsa2048PublicKey(gpk.getRsa2048PublicKey());
      case DH2048_MODP:
        if (!gpk.hasDh2048PublicKey()) {
          break;
        }
        return parseDh2048PublicKey(gpk.getDh2048PublicKey());
      default:
        throw new InvalidKeySpecException("Unsupported GenericPublicKey type: " + gpk.getType());
    }
    throw new InvalidKeySpecException("key object is missing for key type: " + gpk.getType());
  }

  /**
   * Extracts a {@link ECPublicKey} from an {@link EcP256PublicKey} proto message.
   *
   * @throws InvalidKeySpecException if the input is not a valid NIST P-256 public key or if
   *   this platform does not support Elliptic Curve keys
   */
  public static ECPublicKey parseEcPublicKey(EcP256PublicKey p256pk)
      throws InvalidKeySpecException {
    if (!p256pk.hasX() || !p256pk.hasY()) {
      throw new InvalidKeySpecException("Key is missing a required coordinate");
    }
    if (isLegacyCryptoRequired()) {
      throw new InvalidKeySpecException("Elliptic Curve keys not supported on this platform");
    }
    byte[] encodedX = p256pk.getX().toByteArray();
    byte[] encodedY = p256pk.getY().toByteArray();
    try {
      validateEcP256CoordinateEncoding(encodedX);
      validateEcP256CoordinateEncoding(encodedY);
      BigInteger wX = new BigInteger(encodedX);
      BigInteger wY = new BigInteger(encodedY);
      validateEcP256CurvePoint(wX, wY);
      return (ECPublicKey) KeyFactory.getInstance(EC_ALG).generatePublic(
          new ECPublicKeySpec(new ECPoint(wX, wY), EC_P256_PARAMS));
    } catch (NoSuchAlgorithmException e) {
      throw new RuntimeException(e);
    }
  }

  /**
   * Extracts a {@link RSAPublicKey} from an {@link SimpleRsaPublicKey} proto message.
   *
   * @throws InvalidKeySpecException when the input RSA public key is invalid
   */
  public static RSAPublicKey parseRsa2048PublicKey(SimpleRsaPublicKey pk)
      throws InvalidKeySpecException {
    if (!pk.hasN()) {
      throw new InvalidKeySpecException("required field is missing");
    }
    byte[] encodedN = pk.getN().toByteArray();
    validateSimpleRsaEncoding(encodedN);
    BigInteger n = new BigInteger(encodedN);
    if (n.bitLength() != RSA2048_MODULUS_BITS) {
      throw new InvalidKeySpecException();
    }
    BigInteger e = BigInteger.valueOf(pk.getE());
    try {
      return (RSAPublicKey) KeyFactory.getInstance(RSA_ALG).generatePublic(
          new RSAPublicKeySpec(n, e));
    } catch (NoSuchAlgorithmException e1) {
      throw new AssertionError(e1);  // Should never happen
    }
  }

  /**
   * Extracts a {@link DHPublicKey} from an {@link DhPublicKey} proto message.
   *
   * @throws InvalidKeySpecException when the input DH public key is invalid
   */
  @SuppressInsecureCipherModeCheckerPendingReview // b/32143855
  public static DHPublicKey parseDh2048PublicKey(DhPublicKey pk) throws InvalidKeySpecException {
    if (!pk.hasY()) {
      throw new InvalidKeySpecException("required field is missing");
    }
    byte[] encodedY = pk.getY().toByteArray();
    validateDhEncoding(encodedY);
    BigInteger y;
    try {
      y = new BigInteger(encodedY);
    } catch (NumberFormatException e) {
      throw new InvalidKeySpecException();
    }
    validateDhGroupElement(y);
    try {
      return (DHPublicKey) KeyFactory.getInstance(DH_ALG).generatePublic(
          new DHPublicKeySpec(y, DH_P, DH_G));
    } catch (NoSuchAlgorithmException e) {
      throw new AssertionError(e);  // Should never happen
    }
  }

  /**
   * @return a freshly generated NIST P-256 Elliptic Curve key pair.
   */
  public static KeyPair generateEcP256KeyPair() {
    return getEcKeyGen().generateKeyPair();
  }

  /**
   * @return a freshly generated 2048-bit RSA key pair.
   */
  public static KeyPair generateRSA2048KeyPair() {
    return getRsaKeyGen().generateKeyPair();
  }

  /**
   * @return a freshly generated Diffie-Hellman key pair for the 2048-bit group
   *   described by {@link #DH_G}
   */
  public static KeyPair generateDh2048KeyPair() {
    try {
      return getDhKeyGen().generateKeyPair();
    } catch (InvalidAlgorithmParameterException e) {
      // Construct an appropriate KeyPair manually, since this platform refuses to do it for us
      DHParameterSpec spec = new DHParameterSpec(DH_P, DH_G);
      BigInteger x = new BigInteger(DH_LEN, new SecureRandom());
      DHPrivateKey privateKey = new DHPrivateKeyShim(x, spec);
      DHPublicKey publicKey = new DHPublicKeyShim(DH_G.modPow(x, DH_P), spec);
      return new KeyPair(publicKey, privateKey);
    }
  }

  /**
   * A lightweight encoding for a {@link DHPrivateKey}. Strongly recommended over attempting to use
   * {@link DHPrivateKey#getEncoded()}, but not compatible with the standard encoding.
   *
   * @see #parseDh2048PrivateKey(byte[])
   */
  public static byte[] encodeDh2048PrivateKey(DHPrivateKey sk) {
    return sk.getX().toByteArray();
  }

  /**
   * Parses a {@link DHPrivateKey} encoded with {@link #encodeDh2048PrivateKey(DHPrivateKey)}.
   */
  public static DHPrivateKey parseDh2048PrivateKey(byte[] encodedX)
      throws InvalidKeySpecException {
    validateDhEncoding(encodedX);  // Could be stricter for x, but should be fine to use this
    BigInteger x;
    try {
      x = new BigInteger(encodedX);
    } catch (NumberFormatException e) {
      throw new InvalidKeySpecException();
    }
    validateDhGroupElement(x);  // Again, this validation should be good enough
    return new DHPrivateKeyShim(x, new DHParameterSpec(DH_P, DH_G));
  }

  /**
   * @throws InvalidKeySpecException if point ({@code x},{@code y}) isn't on the NIST P-256 curve
   */
  private static void validateEcP256CurvePoint(BigInteger x, BigInteger y)
      throws InvalidKeySpecException {
    if ((x.signum() == -1) || (y.signum() == -1)) {
      throw new InvalidKeySpecException("Point encoding must use only non-negative integers");
    }

    BigInteger p = EC_P256_P;
    if ((x.compareTo(p) >= 0) || (y.compareTo(p) >= 0)) {
      throw new InvalidKeySpecException("Point lies outside of the expected field");
    }

    // Points on the curve satisfy y^2 = x^3 + ax + b  (mod p)
    BigInteger lhs = squareMod(y, p);
    BigInteger rhs = squareMod(x, p).add(EC_P256_A) // = (x^2 + a)
        .multiply(x).mod(p) // = x(x^2 + a) = x^3 + ax
        .add(EC_P256_B) // = x^3 + ax + b
        .mod(p);
    if (!lhs.equals(rhs)) {
      throw new InvalidKeySpecException("Point does not lie on the expected curve");
    }
  }

  /**
   * @return value of {@code x}^2 (mod {@code p})
   */
  private static BigInteger squareMod(BigInteger x, BigInteger p) {
    return x.multiply(x).mod(p);
  }

  /**
   * @throws InvalidKeySpecException if the coordinate is too large for a 256-bit curve
   */
  private static void validateEcP256CoordinateEncoding(byte[] p) throws InvalidKeySpecException {
    if ((p.length == 0)
        || (p.length > MAX_P256_ENCODING_BYTES)
        || (p.length == MAX_P256_ENCODING_BYTES && p[0] != 0)) {
      throw new InvalidKeySpecException();  // Intentionally vague for security reasons
    }
  }

  /**
   * @throws InvalidKeySpecException if the input is too large for a 2048-bit RSA modulus
   */
  private static void validateSimpleRsaEncoding(byte[] n) throws InvalidKeySpecException {
    if (n.length == 0 || n.length > MAX_RSA2048_ENCODING_BYTES) {
      throw new InvalidKeySpecException();
    }
  }

  /**
   * @throws InvalidKeySpecException if the public key is too large for a 2048-bit DH group
   */
  private static void validateDhEncoding(byte[] y) throws InvalidKeySpecException {
    if (y.length == 0 || y.length > MAX_DH2048_ENCODING_BYTES) {
      throw new InvalidKeySpecException();
    }
  }

  /**
   * @throws InvalidKeySpecException if {@code y} is not a valid Diffie-Hellman public key
   */
  private static void validateDhGroupElement(BigInteger y) throws InvalidKeySpecException {
    // Check that 1 < y < p -1
    if ((y.compareTo(ONE) < 1) || (y.compareTo(DH_P.subtract(ONE)) > -1)) {
      throw new InvalidKeySpecException();
    }
  }

  private static ByteString extractY(ECPublicKey epk) {
    return ByteString.copyFrom(epk.getW().getAffineY().toByteArray());
  }

  private static ByteString extractX(ECPublicKey epk) {
    return ByteString.copyFrom(epk.getW().getAffineX().toByteArray());
  }

  private static ECPublicKey pkToECPublicKey(PublicKey pk) {
    if (pk == null) {
      throw new NullPointerException();
    }
    if (!(pk instanceof ECPublicKey)) {
      throw new IllegalArgumentException("Not an EC Public Key");
    }
    return (ECPublicKey) pk;
  }

  private static RSAPublicKey pkToRSAPublicKey(PublicKey pk) {
    if (pk == null) {
      throw new NullPointerException();
    }
    if (!(pk instanceof RSAPublicKey)) {
      throw new IllegalArgumentException("Not an RSA Public Key");
    }
    return (RSAPublicKey) pk;
  }

  private static DHPublicKey pkToDHPublicKey(PublicKey pk) {
    if (pk == null) {
      throw new NullPointerException();
    }
    if (!(pk instanceof DHPublicKey)) {
      throw new IllegalArgumentException("Not a DH Public Key");
    }
    return (DHPublicKey) pk;
  }

  /**
   * @return an EC {@link KeyPairGenerator} object initialized for NIST P-256.
   */
  private static KeyPairGenerator getEcKeyGen() {
    KeyPairGenerator keygen;
    try {
      keygen = KeyPairGenerator.getInstance(EC_ALG);
    } catch (NoSuchAlgorithmException e) {
      throw new RuntimeException(e);
    }
    try {
      // Try using the OpenSSL provider first, since we prefer it over BouncyCastle
      keygen.initialize(new ECGenParameterSpec(EC_P256_OPENSSL_NAME));
      return keygen;
    } catch (InvalidAlgorithmParameterException e) {
      // Try another name for NIST P-256
    }
    try {
      keygen.initialize(new ECGenParameterSpec(EC_P256_COMMON_NAME));
      return keygen;
    } catch (InvalidAlgorithmParameterException e) {
      throw new RuntimeException("Unable to find the NIST P-256 curve");
    }
  }

  /**
   * @return an RSA {@link KeyPairGenerator} object initialized for 2048-bit keys.
   */
  private static KeyPairGenerator getRsaKeyGen() {
    try {
      KeyPairGenerator keygen = KeyPairGenerator.getInstance(RSA_ALG);
      keygen.initialize(RSA2048_MODULUS_BITS);
      return keygen;
    } catch (NoSuchAlgorithmException e) {
      throw new AssertionError(e);  // This should never happen
    }
  }

  /**
   * @return a DH {@link KeyPairGenerator} object initialized for the group described by {@link
   *     #DH_G}.
   * @throws InvalidAlgorithmParameterException on some platforms that don't support large DH groups
   */
  @SuppressInsecureCipherModeCheckerPendingReview // b/32143855
  private static KeyPairGenerator getDhKeyGen() throws InvalidAlgorithmParameterException {
    try {
      KeyPairGenerator keygen = KeyPairGenerator.getInstance(DH_ALG);
      keygen.initialize(new DHParameterSpec(DH_P, DH_G, DH_LEN));
      return keygen;
    } catch (NoSuchAlgorithmException e) {
      throw new AssertionError(e);  // This should never happen
    }
  }

  /**
   * A lightweight shim class to enable the creation of {@link DHPublicKey} and {@link DHPrivateKey}
   * objects that accept arbitrary {@link DHParameterSpec}s -- unfortunately, many platforms do
   * not support using reasonably sized Diffie-Hellman groups any other way. For instance, see
   * <a href="http://bugs.sun.com/bugdatabase/view_bug.do?bug_id=6521495">Java bug 6521495</a>.
   */
  public abstract static class DHKeyShim {

    private BigInteger eitherXorY;
    private DHParameterSpec params;

    public DHKeyShim(BigInteger eitherXorY, DHParameterSpec params) {
      this.eitherXorY = eitherXorY;
      this.params = params;
    }

    public DHParameterSpec getParams() {
      return params;
    }

    public String getAlgorithm() {
      return "DH";
    }

    public String getFormat() {
      return null;
    }

    public byte[] getEncoded() {
      return null;
    }

    public BigInteger getX() {
      return eitherXorY;
    }

    public BigInteger getY() {
      return eitherXorY;
    }
  }

  /**
   * A simple {@link DHPublicKey} implementation.
   *
   * @see DHKeyShim
   */
  public static class DHPublicKeyShim extends DHKeyShim implements DHPublicKey {
    public DHPublicKeyShim(BigInteger y, DHParameterSpec params) {
      super(y, params);
    }
  }

  /**
   * A simple {@link DHPrivateKey} implementation.
   *
   * @see DHKeyShim
   */
  public static class DHPrivateKeyShim extends DHKeyShim implements DHPrivateKey {
    public DHPrivateKeyShim(BigInteger x, DHParameterSpec params) {
      super(x, params);
    }
  }

  /**
   * @return true if this platform does not support Elliptic Curve algorithms
   */
  public static boolean isLegacyCryptoRequired() {
    return IS_LEGACY_CRYPTO_REQUIRED;
  }

  /**
   * @return true if using the Elliptic Curve key generator fails on this platform
   */
  private static boolean determineIfLegacyCryptoRequired() {
    try {
      getEcKeyGen();
    } catch (Exception e) {
      return true;
    }
    return false;
  }
}
