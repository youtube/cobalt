// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines a simple float vector class.  This class is used to indicate a
// distance in two dimensions between two points. Subtracting two points should
// produce a vector, and adding a vector to a point produces the point at the
// vector's distance from the original point.

#ifndef UI_GFX_GEOMETRY_VECTOR3D_F_H_
#define UI_GFX_GEOMETRY_VECTOR3D_F_H_

#include <iosfwd>
#include <string>

#include "ui/gfx/geometry/geometry_export.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace gfx {

class GEOMETRY_EXPORT Vector3dF {
 public:
  constexpr Vector3dF() : x_(0), y_(0), z_(0) {}
  constexpr Vector3dF(float x, float y, float z) : x_(x), y_(y), z_(z) {}

  constexpr explicit Vector3dF(const Vector2dF& other)
      : x_(other.x()), y_(other.y()), z_(0) {}

  constexpr float x() const { return x_; }
  void set_x(float x) { x_ = x; }

  constexpr float y() const { return y_; }
  void set_y(float y) { y_ = y; }

  constexpr float z() const { return z_; }
  void set_z(float z) { z_ = z; }

  // True if all components of the vector are 0.
  bool IsZero() const;

  // Add the components of the |other| vector to the current vector.
  void Add(const Vector3dF& other);
  // Subtract the components of the |other| vector from the current vector.
  void Subtract(const Vector3dF& other);

  void operator+=(const Vector3dF& other) { Add(other); }
  void operator-=(const Vector3dF& other) { Subtract(other); }

  void SetToMin(const Vector3dF& other) {
    x_ = x_ <= other.x_ ? x_ : other.x_;
    y_ = y_ <= other.y_ ? y_ : other.y_;
    z_ = z_ <= other.z_ ? z_ : other.z_;
  }

  void SetToMax(const Vector3dF& other) {
    x_ = x_ >= other.x_ ? x_ : other.x_;
    y_ = y_ >= other.y_ ? y_ : other.y_;
    z_ = z_ >= other.z_ ? z_ : other.z_;
  }

  // Gives the square of the diagonal length of the vector.
  double LengthSquared() const;
  // Gives the diagonal length of the vector.
  float Length() const;

  // Scale all components of the vector by |scale|.
  void Scale(float scale) { Scale(scale, scale, scale); }
  // Scale the each component of the vector by the given scale factors.
  void Scale(float x_scale, float y_scale, float z_scale);

  // Take the cross product of this vector with |other| and become the result.
  void Cross(const Vector3dF& other);

  // |out| is assigned a unit-length vector in the direction of |this| iff
  // this function returns true. It can return false if |this| is too short.
  bool GetNormalized(Vector3dF* out) const;

  std::string ToString() const;

 private:
  float x_;
  float y_;
  float z_;
};

inline bool operator==(const Vector3dF& lhs, const Vector3dF& rhs) {
  return lhs.x() == rhs.x() && lhs.y() == rhs.y() && lhs.z() == rhs.z();
}

inline bool operator!=(const Vector3dF& lhs, const Vector3dF& rhs) {
  return !(lhs == rhs);
}

inline Vector3dF operator-(const Vector3dF& v) {
  return Vector3dF(-v.x(), -v.y(), -v.z());
}

inline Vector3dF operator+(const Vector3dF& lhs, const Vector3dF& rhs) {
  Vector3dF result = lhs;
  result.Add(rhs);
  return result;
}

inline Vector3dF operator-(const Vector3dF& lhs, const Vector3dF& rhs) {
  Vector3dF result = lhs;
  result.Add(-rhs);
  return result;
}

// Return the cross product of two vectors.
inline Vector3dF CrossProduct(const Vector3dF& lhs, const Vector3dF& rhs) {
  Vector3dF result = lhs;
  result.Cross(rhs);
  return result;
}

// Return the dot product of two vectors.
GEOMETRY_EXPORT float DotProduct(const Vector3dF& lhs, const Vector3dF& rhs);

// Return a vector that is |v| scaled by the given scale factors along each
// axis.
GEOMETRY_EXPORT Vector3dF ScaleVector3d(const Vector3dF& v,
                                        float x_scale,
                                        float y_scale,
                                        float z_scale);

// Return a vector that is |v| scaled by the components of |s|
inline Vector3dF ScaleVector3d(const Vector3dF& v, const Vector3dF& s) {
  return ScaleVector3d(v, s.x(), s.y(), s.z());
}

// Return a vector that is |v| scaled by the given scale factor.
inline Vector3dF ScaleVector3d(const Vector3dF& v, float scale) {
  return ScaleVector3d(v, scale, scale, scale);
}

// Returns the angle between |base| and |other| in degrees.
GEOMETRY_EXPORT float AngleBetweenVectorsInDegrees(const gfx::Vector3dF& base,
                                                   const gfx::Vector3dF& other);

// Returns the clockwise angle between |base| and |other| where |normal| is the
// normal of the virtual surface to measure clockwise according to.
GEOMETRY_EXPORT float ClockwiseAngleBetweenVectorsInDegrees(
    const gfx::Vector3dF& base,
    const gfx::Vector3dF& other,
    const gfx::Vector3dF& normal);

// This is declared here for use in gtest-based unit tests but is defined in
// the //ui/gfx:test_support target. Depend on that to use this in your unit
// test. This should not be used in production code - call ToString() instead.
void PrintTo(const Vector3dF& vector, ::std::ostream* os);

}  // namespace gfx

#endif // UI_GFX_GEOMETRY_VECTOR3D_F_H_
