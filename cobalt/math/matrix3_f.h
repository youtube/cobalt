// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COBALT_MATH_MATRIX3_F_H_
#define COBALT_MATH_MATRIX3_F_H_

#include "base/logging.h"
#include "cobalt/math/point_f.h"
#include "cobalt/math/rect_f.h"
#include "cobalt/math/vector3d_f.h"

namespace cobalt {
namespace math {

class Matrix3F {
 public:
  ~Matrix3F();

  static Matrix3F Zeros();
  static Matrix3F Ones();
  static Matrix3F Identity();
  static Matrix3F FromOuterProduct(const Vector3dF& a, const Vector3dF& bt);
  static Matrix3F FromArray(const float data[9]);
  static Matrix3F FromValues(float m00, float m01, float m02, float m10,
                             float m11, float m12, float m20, float m21,
                             float m22);

  void SetMatrix(float m00, float m01, float m02, float m10, float m11,
                 float m12, float m20, float m21, float m22) {
    data_[0] = m00;
    data_[1] = m01;
    data_[2] = m02;
    data_[3] = m10;
    data_[4] = m11;
    data_[5] = m12;
    data_[6] = m20;
    data_[7] = m21;
    data_[8] = m22;
  }

  bool IsZeros() const;
  bool IsIdentity() const;

  bool IsEqual(const Matrix3F& rhs) const;

  // Element-wise comparison with given precision.
  bool IsNear(const Matrix3F& rhs, float precision) const;

  // Access data by row (i) and column (j).
  float Get(int i, int j) const { return (*this)(i, j); }
  void Set(int i, int j, float v) { (*this)(i, j) = v; }

  float operator()(int i, int j) const {
    return data_[MatrixToArrayCoords(i, j)];
  }
  float& operator()(int i, int j) { return data_[MatrixToArrayCoords(i, j)]; }

  Matrix3F operator*(const Matrix3F& other) const;

  // Returns the result of multiplying the given homogeneous 2D point by this
  // matrix.
  PointF operator*(const PointF& rhs) const;

  Vector3dF column(int i) const {
    return Vector3dF(data_[MatrixToArrayCoords(0, i)],
                     data_[MatrixToArrayCoords(1, i)],
                     data_[MatrixToArrayCoords(2, i)]);
  }

  void set_column(int i, const Vector3dF& c) {
    data_[MatrixToArrayCoords(0, i)] = c.x();
    data_[MatrixToArrayCoords(1, i)] = c.y();
    data_[MatrixToArrayCoords(2, i)] = c.z();
  }

  // Returns an inverse of this if the matrix is non-singular, zero (== Zero())
  // otherwise.
  Matrix3F Inverse() const;

  // Value of the determinant of the matrix.
  float Determinant() const;

  // Trace (sum of diagonal elements) of the matrix.
  float Trace() const {
    return data_[MatrixToArrayCoords(0, 0)] + data_[MatrixToArrayCoords(1, 1)] +
           data_[MatrixToArrayCoords(2, 2)];
  }

  // Compute eigenvalues and (optionally) normalized eigenvectors of
  // a positive definite matrix *this. Eigenvectors are computed only if
  // non-null |eigenvectors| matrix is passed. If it is NULL, the routine
  // will not attempt to compute eigenvectors but will still return eigenvalues
  // if they can be computed.
  // If eigenvalues cannot be computed (the matrix does not meet constraints)
  // the 0-vector is returned. Note that to retrieve eigenvalues, the matrix
  // only needs to be symmetric while eigenvectors require it to be
  // positive-definite. Passing a non-positive definite matrix will result in
  // NaNs in vectors which cannot be computed.
  // Eigenvectors are placed as column in |eigenvectors| in order corresponding
  // to eigenvalues.
  Vector3dF SolveEigenproblem(Matrix3F* eigenvectors) const;

  // Applies operator*(const PointF&) to each of the 4 points on the rectangle
  // and then returns a RectF that is the tightest axis-aligned bounding box
  // around those points.
  RectF MapRect(const RectF& rect) const;

 private:
  Matrix3F();  // Uninitialized default.

  static int MatrixToArrayCoords(int i, int j) {
    DCHECK(i >= 0 && i < 3);
    DCHECK(j >= 0 && j < 3);
    return i * 3 + j;
  }

  float data_[9];
};

inline bool operator==(const Matrix3F& lhs, const Matrix3F& rhs) {
  return lhs.IsEqual(rhs);
}

inline std::ostream& operator<<(std::ostream& stream, const Matrix3F& matrix) {
  stream << "[";
  stream << "[" << matrix(0, 0) << ", " << matrix(0, 1) << ", " << matrix(0, 2)
         << "], ";
  stream << "[" << matrix(1, 0) << ", " << matrix(1, 1) << ", " << matrix(1, 2)
         << "], ";
  stream << "[" << matrix(2, 0) << ", " << matrix(2, 1) << ", " << matrix(2, 2)
         << "]";
  stream << "]";
  return stream;
}

}  // namespace math
}  // namespace cobalt

#endif  // COBALT_MATH_MATRIX3_F_H_
