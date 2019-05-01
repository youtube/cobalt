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

import static java.math.BigInteger.ONE;
import static java.math.BigInteger.ZERO;

import com.google.common.annotations.VisibleForTesting;

import java.math.BigInteger;

/**
 * Implements the Ed25519 twisted Edwards curve.  See http://ed25519.cr.yp.to/ for more details.
 */
public class Ed25519 {

  // Don't instantiate
  private Ed25519() { }

  // Curve parameters (http://ed25519.cr.yp.to/)
  private static final int HEX_RADIX = 16;
  private static final BigInteger Ed25519_P =
      new BigInteger("7FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFED", HEX_RADIX);
  private static final BigInteger Ed25519_D =
      new BigInteger("52036CEE2B6FFE738CC740797779E89800700A4D4141D8AB75EB4DCA135978A3", HEX_RADIX);

  // Helps to do fast addition k = 2*d
  private static final BigInteger Ed25519_K =
      new BigInteger("2406D9DC56DFFCE7198E80F2EEF3D13000E0149A8283B156EBD69B9426B2F159", HEX_RADIX);

  // Identity point in extended representation (0, 1, 1, 0)
  static final BigInteger[] IDENTITY_POINT = new BigInteger[] {ZERO, ONE, ONE, ZERO};

  // Helps for reading coordinate type in point representation
  private static final int X = 0;
  private static final int Y = 1;
  private static final int Z = 2;
  private static final int T = 3;

  // Number of bits that we need to represent a point.  Realistically, we only need 255, but using
  // 256 doesn't hurt.
  private static final int POINT_SIZE_BITS = 256;

  /**
   * Returns the result of multiplying point p by scalar k. A point is represented as a BigInteger
   * array of length 2 where the first element (at index 0) is the X coordinate and the second
   * element (at index 1) is the Y coordinate.
   */
  public static BigInteger[] scalarMultiplyAffinePoint(BigInteger[] p, BigInteger k)
      throws Ed25519Exception {
    return toAffine(scalarMultiplyExtendedPoint(toExtended(p), k));
  }

  /**
   * Returns the sum of two points in affine representation. A point is represented as a BigInteger
   * array of length 2 where the first element (at index 0) is the X coordinate and the second
   * element (at index 1) is the Y coordinate.
   */
  public static BigInteger[] addAffinePoints(BigInteger[] p1, BigInteger[] p2)
      throws Ed25519Exception {
    return toAffine(addExtendedPoints(toExtended(p1), toExtended(p2)));
  }

  /**
   * Returns the result of subtracting p2 from p1 (i.e., p1 - p2) in affine representation. A point
   * is represented as a BigInteger array of length 2 where the first element (at index 0) is the X
   * coordinate and the second element (at index 1) is the Y coordinate.
   */
  public static BigInteger[] subtractAffinePoints(BigInteger[] p1, BigInteger[] p2)
      throws Ed25519Exception {
    return toAffine(subtractExtendedPoints(toExtended(p1), toExtended(p2)));
  }

  /**
   * Validates that a given point in affine representation is on the curve and is positive.
   * @throws Ed25519Exception if the point does not validate
   */
  public static void validateAffinePoint(BigInteger[] p) throws Ed25519Exception {
    checkPointIsInAffineRepresentation(p);

    BigInteger x = p[X];
    BigInteger y = p[Y];

    if (x.signum() != 1 || y.signum() != 1) {
      throw new Ed25519Exception("Point encoding must use only positive integers");
    }

    if ((x.compareTo(Ed25519_P) >= 0) || (y.compareTo(Ed25519_P) >= 0)) {
      throw new Ed25519Exception("Point lies outside of the expected field");
    }

    BigInteger xx = x.multiply(x);
    BigInteger yy = y.multiply(y);
    BigInteger lhs = xx.negate().add(yy).mod(Ed25519_P);                           // -x*x + y*y
    BigInteger rhs = ONE.add(Ed25519_D.multiply(xx).multiply(yy)).mod(Ed25519_P);  // 1 + d*x*x*y*y

    if (!lhs.equals(rhs)) {
      throw new Ed25519Exception("Point does not lie on the expected curve");
    }
  }

  /**
   * Returns the result of multiplying point p by scalar k
   */
  static BigInteger[] scalarMultiplyExtendedPoint(BigInteger[] p, BigInteger k)
      throws Ed25519Exception {
    checkPointIsInExtendedRepresentation(p);
    if (k == null) {
      throw new Ed25519Exception("Can't multiply point by null");
    }

    if (k.bitLength() > POINT_SIZE_BITS) {
      throw new Ed25519Exception(
          "Refuse to multiply point by scalar with more than " + POINT_SIZE_BITS + " bits");
    }

    // Perform best effort time-constant accumulation
    BigInteger[] q = IDENTITY_POINT;
    BigInteger[] r = IDENTITY_POINT;
    BigInteger[] doubleAccumulator = p;
    for (int i = 0; i < POINT_SIZE_BITS; i++) {
      if (k.testBit(i)) {
        q = addExtendedPoints(q, doubleAccumulator);
      } else {
        r = addExtendedPoints(q, doubleAccumulator);
      }
      if (i < POINT_SIZE_BITS - 1) {
        doubleAccumulator = doubleExtendedPoint(doubleAccumulator);
      }
    }

    // Not needed, but we're just trying to fool the compiler into not optimizing away r
    r = subtractExtendedPoints(r, r);
    q = addExtendedPoints(q, r);
    return q;
  }

  /**
   * Returns the doubling of a point in extended representation
   */
  private static BigInteger[] doubleExtendedPoint(BigInteger[] p) throws Ed25519Exception {
    // The Edwards curve is complete, so we can just add a point to itself.
    // Note that the currently best known algorithms for doubling have the same order as addition.
    // https://hyperelliptic.org/EFD/g1p/auto-twisted-extended-1.html
    checkPointIsInExtendedRepresentation(p);

    BigInteger c = p[T].pow(2).multiply(Ed25519_K);
    BigInteger d = p[Z].pow(2).shiftLeft(1);
    BigInteger e = p[Y].multiply(p[X]).shiftLeft(2);
    BigInteger f = d.subtract(c);
    BigInteger g = d.add(c);
    BigInteger h = p[Y].pow(2).add(p[X].pow(2)).shiftLeft(1);

    return new BigInteger[] {
        e.multiply(f).mod(Ed25519_P),
        g.multiply(h).mod(Ed25519_P),
        f.multiply(g).mod(Ed25519_P),
        e.multiply(h).mod(Ed25519_P)
    };
  }

  /**
   * Returns the result of subtracting p2 from p1 (p1 - p2)
   */
  static BigInteger[] subtractExtendedPoints(BigInteger[] p1, BigInteger[] p2)
      throws Ed25519Exception {
    checkPointIsInExtendedRepresentation(p1);
    checkPointIsInExtendedRepresentation(p2);

    return addExtendedPoints(p1, new BigInteger[] {p2[X].negate(), p2[Y], p2[Z], p2[T].negate()});
  }

  /**
   * Returns the sum of two points in extended representation
   * Uses: https://hyperelliptic.org/EFD/g1p/auto-twisted-extended-1.html#addition-add-2008-hwcd-3
   */
  static BigInteger[] addExtendedPoints(BigInteger[] p1, BigInteger[] p2)
      throws Ed25519Exception {
    checkPointIsInExtendedRepresentation(p1);
    checkPointIsInExtendedRepresentation(p2);

    BigInteger a = p1[Y].subtract(p1[X]).multiply(p2[Y].subtract(p2[X]));
    BigInteger b = p1[Y].add(p1[X]).multiply(p2[Y].add(p2[X]));
    BigInteger c = p1[T].multiply(Ed25519_K).multiply(p2[T]);
    BigInteger d = p1[Z].add(p1[Z]).multiply(p2[Z]);
    BigInteger e = b.subtract(a);
    BigInteger f = d.subtract(c);
    BigInteger g = d.add(c);
    BigInteger h = b.add(a);

    return new BigInteger[] {
        e.multiply(f).mod(Ed25519_P),
        g.multiply(h).mod(Ed25519_P),
        f.multiply(g).mod(Ed25519_P),
        e.multiply(h).mod(Ed25519_P)
    };
  }

  /**
   * Converts a point in affine representation to extended representation
   */
  @VisibleForTesting
  static BigInteger[] toExtended(BigInteger[] p) throws Ed25519Exception {
    checkPointIsInAffineRepresentation(p);

    return new BigInteger[] {p[X], p[Y], ONE, p[X].multiply(p[Y]).mod(Ed25519_P)}; // x, y, 1, x*y
  }

  /**
   * Converts a point in extended representation to affine representation
   */
  @VisibleForTesting
  static BigInteger[] toAffine(BigInteger[] p) throws Ed25519Exception {
    checkPointIsInExtendedRepresentation(p);

    return new BigInteger[] {p[X].multiply(p[Z].modInverse(Ed25519_P)).mod(Ed25519_P), // x = X / Z
        p[Y].multiply(p[Z].modInverse(Ed25519_P)).mod(Ed25519_P)};                     // y = Y / Z
  }

  /**
   * Checks that a given point is in the extended representation
   * @throws Ed25519Exception if the point is not in the extended representation
   */
  @VisibleForTesting
  static void checkPointIsInExtendedRepresentation(BigInteger[] p) throws Ed25519Exception {
    if (p == null || p.length != 4 || p[X] == null || p[Y] == null || p[Z] == null
        || p[T] == null) {
      throw new Ed25519Exception("Point is not in extended representation");
    }
  }

  /**
   * Checks that a given point is in the affine representation
   * @throws Ed25519Exception if the point is not in the affine representation
   */
  @VisibleForTesting
  static void checkPointIsInAffineRepresentation(BigInteger[] p) throws Ed25519Exception {
    if (p == null || p.length != 2 || p[X] == null || p[Y] == null) {
      throw new Ed25519Exception("Point is not in affine representation");
    }
  }

  /**
   * Represents an unrecoverable error that has occurred while performing a curve operation.
   */
  public static class Ed25519Exception extends Exception {
    public Ed25519Exception(String message) {
      super(message);
    }

    public Ed25519Exception(Exception e) {
      super(e);
    }

    public Ed25519Exception(String message, Exception e) {
      super(message, e);
    }
  }
}
