// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cmath>
#include <limits>

#include "base/basictypes.h"
#include "cobalt/math/matrix3_f.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace math {
namespace {

TEST(Matrix3fTest, Constructors) {
  Matrix3F zeros = Matrix3F::Zeros();
  Matrix3F ones = Matrix3F::Ones();
  Matrix3F identity = Matrix3F::Identity();

  Matrix3F product_ones = Matrix3F::FromOuterProduct(
      Vector3dF(1.0f, 1.0f, 1.0f), Vector3dF(1.0f, 1.0f, 1.0f));
  Matrix3F product_zeros = Matrix3F::FromOuterProduct(
      Vector3dF(1.0f, 1.0f, 1.0f), Vector3dF(0.0f, 0.0f, 0.0f));

  const float kDataValues[9] = {0, 1, 2, 3, 4, 5, 6, 7, 8};
  Matrix3F from_array = Matrix3F::FromArray(kDataValues);
  Matrix3F from_values =
      Matrix3F::FromValues(kDataValues[0], kDataValues[1], kDataValues[2],
                           kDataValues[3], kDataValues[4], kDataValues[5],
                           kDataValues[6], kDataValues[7], kDataValues[8]);

  EXPECT_EQ(ones, product_ones);
  EXPECT_EQ(zeros, product_zeros);

  for (int i = 0; i < 3; ++i) {
    for (int j = 0; j < 3; ++j) {
      EXPECT_EQ(i == j ? 1.0f : 0.0f, identity(i, j));
      EXPECT_EQ(kDataValues[i * 3 + j], from_array(i, j));
      EXPECT_EQ(from_array(i, j), from_values(i, j));
    }
  }
}

TEST(Matrix3fTest, DataAccess) {
  Matrix3F matrix = Matrix3F::Ones();
  Matrix3F identity = Matrix3F::Identity();

  EXPECT_EQ(Vector3dF(0.0f, 1.0f, 0.0f), identity.column(1));
  matrix.SetMatrix(0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f);
  EXPECT_EQ(Vector3dF(2.0f, 5.0f, 8.0f), matrix.column(2));
  matrix.set_column(0, Vector3dF(0.1f, 0.2f, 0.3f));
  EXPECT_EQ(Vector3dF(0.1f, 0.2f, 0.3f), matrix.column(0));

  EXPECT_EQ(0.1f, matrix(0, 0));
  EXPECT_EQ(5.0f, matrix(1, 2));
}

TEST(Matrix3fTest, Determinant) {
  EXPECT_EQ(1.0f, Matrix3F::Identity().Determinant());
  EXPECT_EQ(0.0f, Matrix3F::Zeros().Determinant());
  EXPECT_EQ(0.0f, Matrix3F::Ones().Determinant());

  // Now for something non-trivial...
  Matrix3F matrix = Matrix3F::Zeros();
  matrix.SetMatrix(0, 5, 6, 8, 7, 0, 1, 9, 0);
  EXPECT_EQ(390.0f, matrix.Determinant());
  matrix(2, 0) = 3 * matrix(0, 0);
  matrix(2, 1) = 3 * matrix(0, 1);
  matrix(2, 2) = 3 * matrix(0, 2);
  EXPECT_EQ(0, matrix.Determinant());

  matrix.SetMatrix(0.57f, 0.205f, 0.942f, 0.314f, 0.845f, 0.826f, 0.131f,
                   0.025f, 0.962f);
  EXPECT_NEAR(0.3149f, matrix.Determinant(), 0.0001f);
}

TEST(Matrix3fTest, Inverse) {
  Matrix3F identity = Matrix3F::Identity();
  Matrix3F inv_identity = identity.Inverse();
  EXPECT_EQ(identity, inv_identity);

  Matrix3F singular = Matrix3F::Zeros();
  singular.SetMatrix(1.0f, 3.0f, 4.0f, 2.0f, 11.0f, 5.0f, 0.5f, 1.5f, 2.0f);
  EXPECT_EQ(0, singular.Determinant());
  EXPECT_EQ(Matrix3F::Zeros(), singular.Inverse());

  Matrix3F regular = Matrix3F::Zeros();
  regular.SetMatrix(0.57f, 0.205f, 0.942f, 0.314f, 0.845f, 0.826f, 0.131f,
                    0.025f, 0.962f);
  Matrix3F inv_regular = regular.Inverse();
  regular.SetMatrix(2.51540616f, -0.55138018f, -1.98968043f, -0.61552266f,
                    1.34920184f, -0.55573636f, -0.32653861f, 0.04002158f,
                    1.32488726f);
  EXPECT_TRUE(regular.IsNear(inv_regular, 0.00001f));
}

TEST(Matrix3fTest, EigenvectorsIdentity) {
  // This block tests the trivial case of eigenvalues of the identity matrix.
  Matrix3F identity = Matrix3F::Identity();
  Vector3dF eigenvals = identity.SolveEigenproblem(NULL);
  EXPECT_EQ(Vector3dF(1.0f, 1.0f, 1.0f), eigenvals);
}

TEST(Matrix3fTest, EigenvectorsDiagonal) {
  // This block tests the another trivial case of eigenvalues of a diagonal
  // matrix. Here we expect values to be sorted.
  Matrix3F matrix = Matrix3F::Zeros();
  matrix(0, 0) = 1.0f;
  matrix(1, 1) = -2.5f;
  matrix(2, 2) = 3.14f;
  Matrix3F eigenvectors = Matrix3F::Zeros();
  Vector3dF eigenvals = matrix.SolveEigenproblem(&eigenvectors);
  EXPECT_EQ(Vector3dF(3.14f, 1.0f, -2.5f), eigenvals);

  EXPECT_EQ(Vector3dF(0.0f, 0.0f, 1.0f), eigenvectors.column(0));
  EXPECT_EQ(Vector3dF(1.0f, 0.0f, 0.0f), eigenvectors.column(1));
  EXPECT_EQ(Vector3dF(0.0f, 1.0f, 0.0f), eigenvectors.column(2));
}

TEST(Matrix3fTest, EigenvectorsNiceNotPositive) {
  // This block tests computation of eigenvectors of a matrix where nice
  // round values are expected.
  Matrix3F matrix = Matrix3F::Zeros();
  // This is not a positive-definite matrix but eigenvalues and the first
  // eigenvector should nonetheless be computed correctly.
  matrix.SetMatrix(3, 2, 4, 2, 0, 2, 4, 2, 3);
  Matrix3F eigenvectors = Matrix3F::Zeros();
  Vector3dF eigenvals = matrix.SolveEigenproblem(&eigenvectors);

  // The below three EXPECT_NEARs are changed from
  //   EXPECT_EQ(Vector3dF(8.0f, -1.0f, -1.0f), eigenvals);
  // because the test fails on some platforms.
  //   Value of: eigenvals.y()
  //   Actual: -0.99999952
  //   Expected: -1.0f
  //   Which is: -1
  EXPECT_NEAR(8.0f, eigenvals.x(), 0.000001f);
  EXPECT_NEAR(-1.0f, eigenvals.y(), 0.000001f);
  EXPECT_NEAR(-1.0f, eigenvals.z(), 0.000001f);

  Vector3dF expected_principal(0.66666667f, 0.33333333f, 0.66666667f);
  EXPECT_NEAR(0.0f, (expected_principal - eigenvectors.column(0)).Length(),
              0.000001f);
}

TEST(Matrix3fTest, EigenvectorsPositiveDefinite) {
  // This block tests computation of eigenvectors of a matrix where output
  // is not as nice as above, but it actually meets the definition.
  Matrix3F matrix = Matrix3F::Zeros();
  Matrix3F eigenvectors = Matrix3F::Zeros();
  Matrix3F expected_eigenvectors = Matrix3F::Zeros();
  matrix.SetMatrix(1, -1, 2, -1, 4, 5, 2, 5, 0);
  Vector3dF eigenvals = matrix.SolveEigenproblem(&eigenvectors);
  Vector3dF expected_eigv(7.3996266f, 1.91197255f, -4.31159915f);
  expected_eigv -= eigenvals;
  EXPECT_NEAR(0, expected_eigv.LengthSquared(), 0.00001f);
  expected_eigenvectors.SetMatrix(0.04926317f, -0.92135662f, -0.38558414f,
                                  0.82134249f, 0.25703273f, -0.50924521f,
                                  0.56830419f, -0.2916096f, 0.76941158f);
  EXPECT_TRUE(expected_eigenvectors.IsNear(eigenvectors, 0.00001f));
}

TEST(Matrix3fTest, MatrixMultiplyIdentityByIdentity) {
  // The identity matrix times itself should give the identity matrix
  EXPECT_TRUE(Matrix3F::Identity().IsNear(
      Matrix3F::Identity() * Matrix3F::Identity(), 0.00001f));
}

TEST(Matrix3fTest, MatrixMultiplyIdentityByArbitrary) {
  // The identity matrix times an arbitrary matrix should return that same
  // arbitrary matrix.
  Matrix3F matrix_a = Matrix3F::FromValues(1, 2, 3, 4, 5, 6, 7, 8, 9);
  EXPECT_TRUE(matrix_a.IsNear(Matrix3F::Identity() * matrix_a, 0.00001f));
  EXPECT_TRUE(matrix_a.IsNear(matrix_a * Matrix3F::Identity(), 0.00001f));
}

TEST(Matrix3fTest, MatrixMultiplyArbitraryByArbitrary) {
  // Check that multiplying two arbitrary matrices together gives the expected
  // results.
  Matrix3F matrix_a = Matrix3F::FromValues(1, 2, 3, 4, 5, 6, 7, 8, 9);
  Matrix3F matrix_b = Matrix3F::FromValues(10, 11, 12, 13, 14, 15, 16, 17, 18);

  Matrix3F result = matrix_a * matrix_b;
  Matrix3F expected_result =
      Matrix3F::FromValues(84, 90, 96, 201, 216, 231, 318, 342, 366);
  EXPECT_TRUE(expected_result.IsNear(result, 0.00001f));
}

}  // namespace
}  // namespace math
}  // namespace cobalt
