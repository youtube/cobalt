/*
 * Copyright 2020 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef skgpu_tessellate_WangsFormula_DEFINED
#define skgpu_tessellate_WangsFormula_DEFINED

#include "include/core/SkMatrix.h"
#include "include/core/SkPoint.h"
#include "include/core/SkString.h"
#include "include/private/SkFloatingPoint.h"
#include "src/gpu/tessellate/Tessellation.h"

#define AI SK_MAYBE_UNUSED SK_ALWAYS_INLINE

// Wang's formula gives the minimum number of evenly spaced (in the parametric sense) line segments
// that a bezier curve must be chopped into in order to guarantee all lines stay within a distance
// of "1/precision" pixels from the true curve. Its definition for a bezier curve of degree "n" is
// as follows:
//
//     maxLength = max([length(p[i+2] - 2p[i+1] + p[i]) for (0 <= i <= n-2)])
//     numParametricSegments = sqrt(maxLength * precision * n*(n - 1)/8)
//
// (Goldman, Ron. (2003). 5.6.3 Wang's Formula. "Pyramid Algorithms: A Dynamic Programming Approach
// to Curves and Surfaces for Geometric Modeling". Morgan Kaufmann Publishers.)
namespace skgpu::wangs_formula {

// Returns the value by which to multiply length in Wang's formula. (See above.)
template<int Degree> constexpr float length_term(float precision) {
    return (Degree * (Degree - 1) / 8.f) * precision;
}
template<int Degree> constexpr float length_term_pow2(float precision) {
    return ((Degree * Degree) * ((Degree - 1) * (Degree - 1)) / 64.f) * (precision * precision);
}

AI float root4(float x) {
    return sqrtf(sqrtf(x));
}

// Returns nextlog2(sqrt(x)):
//
//   log2(sqrt(x)) == log2(x^(1/2)) == log2(x)/2 == log2(x)/log2(4) == log4(x)
//
AI int nextlog4(float x) {
    return (sk_float_nextlog2(x) + 1) >> 1;
}

// Returns nextlog2(sqrt(sqrt(x))):
//
//   log2(sqrt(sqrt(x))) == log2(x^(1/4)) == log2(x)/4 == log2(x)/log2(16) == log16(x)
//
AI int nextlog16(float x) {
    return (sk_float_nextlog2(x) + 3) >> 2;
}

// Represents the upper-left 2x2 matrix of an affine transform for applying to vectors:
//
//     VectorXform(p1 - p0) == M * float3(p1, 1) - M * float3(p0, 1)
//
class VectorXform {
public:
    using float2 = skvx::Vec<2, float>;
    using float4 = skvx::Vec<4, float>;
    AI explicit VectorXform() : fType(Type::kIdentity) {}
    AI explicit VectorXform(const SkMatrix& m) { *this = m; }
    AI VectorXform& operator=(const SkMatrix& m) {
        SkASSERT(!m.hasPerspective());
        if (m.getType() & SkMatrix::kAffine_Mask) {
            fType = Type::kAffine;
            fScaleXSkewY = {m.getScaleX(), m.getSkewY()};
            fSkewXScaleY = {m.getSkewX(), m.getScaleY()};
            fScaleXYXY = {m.getScaleX(), m.getScaleY(), m.getScaleX(), m.getScaleY()};
            fSkewXYXY = {m.getSkewX(), m.getSkewY(), m.getSkewX(), m.getSkewY()};
        } else if (m.getType() & SkMatrix::kScale_Mask) {
            fType = Type::kScale;
            fScaleXY = {m.getScaleX(), m.getScaleY()};
            fScaleXYXY = {m.getScaleX(), m.getScaleY(), m.getScaleX(), m.getScaleY()};
        } else {
            SkASSERT(!(m.getType() & ~SkMatrix::kTranslate_Mask));
            fType = Type::kIdentity;
        }
        return *this;
    }
    AI float2 operator()(float2 vector) const {
        switch (fType) {
            case Type::kIdentity:
                return vector;
            case Type::kScale:
                return fScaleXY * vector;
            case Type::kAffine:
                return fScaleXSkewY * float2(vector[0]) + fSkewXScaleY * vector[1];
        }
        SkUNREACHABLE;
    }
    AI float4 operator()(float4 vectors) const {
        switch (fType) {
            case Type::kIdentity:
                return vectors;
            case Type::kScale:
                return vectors * fScaleXYXY;
            case Type::kAffine:
                return fScaleXYXY * vectors + fSkewXYXY * vectors.yxwz();
        }
        SkUNREACHABLE;
    }
private:
    enum class Type { kIdentity, kScale, kAffine } fType;
    union { float2 fScaleXY, fScaleXSkewY; };
    float2 fSkewXScaleY;
    float4 fScaleXYXY;
    float4 fSkewXYXY;
};

// Returns Wang's formula, raised to the 4th power, specialized for a quadratic curve.
AI float quadratic_pow4(float precision,
                        float2 p0, float2 p1, float2 p2,
                        const VectorXform& vectorXform = VectorXform()) {
    float2 v = -2*p1 + p0 + p2;
    v = vectorXform(v);
    float2 vv = v*v;
    return (vv[0] + vv[1]) * length_term_pow2<2>(precision);
}
AI float quadratic_pow4(float precision,
                        const SkPoint pts[],
                        const VectorXform& vectorXform = VectorXform()) {
    return quadratic_pow4(precision,
                          skvx::bit_pun<float2>(pts[0]),
                          skvx::bit_pun<float2>(pts[1]),
                          skvx::bit_pun<float2>(pts[2]),
                          vectorXform);
}

// Returns Wang's formula specialized for a quadratic curve.
AI float quadratic(float precision,
                   const SkPoint pts[],
                   const VectorXform& vectorXform = VectorXform()) {
    return root4(quadratic_pow4(precision, pts, vectorXform));
}

// Returns the log2 value of Wang's formula specialized for a quadratic curve, rounded up to the
// next int.
AI int quadratic_log2(float precision,
                      const SkPoint pts[],
                      const VectorXform& vectorXform = VectorXform()) {
    // nextlog16(x) == ceil(log2(sqrt(sqrt(x))))
    return nextlog16(quadratic_pow4(precision, pts, vectorXform));
}

// Returns Wang's formula, raised to the 4th power, specialized for a cubic curve.
AI float cubic_pow4(float precision,
                    float2 p0, float2 p1, float2 p2, float2 p3,
                    const VectorXform& vectorXform = VectorXform()) {
    float4 p01{p0, p1};
    float4 p12{p1, p2};
    float4 p23{p2, p3};
    float4 v = -2*p12 + p01 + p23;
    v = vectorXform(v);
    float4 vv = v*v;
    return std::max(vv[0] + vv[1], vv[2] + vv[3]) * length_term_pow2<3>(precision);
}
AI float cubic_pow4(float precision,
                    const SkPoint pts[],
                    const VectorXform& vectorXform = VectorXform()) {
    return cubic_pow4(precision,
                      skvx::bit_pun<float2>(pts[0]),
                      skvx::bit_pun<float2>(pts[1]),
                      skvx::bit_pun<float2>(pts[2]),
                      skvx::bit_pun<float2>(pts[3]),
                      vectorXform);
}

// Returns Wang's formula specialized for a cubic curve.
AI float cubic(float precision,
               const SkPoint pts[],
               const VectorXform& vectorXform = VectorXform()) {
    return root4(cubic_pow4(precision, pts, vectorXform));
}

// Returns the log2 value of Wang's formula specialized for a cubic curve, rounded up to the next
// int.
AI int cubic_log2(float precision,
                  const SkPoint pts[],
                  const VectorXform& vectorXform = VectorXform()) {
    // nextlog16(x) == ceil(log2(sqrt(sqrt(x))))
    return nextlog16(cubic_pow4(precision, pts, vectorXform));
}

// Returns the maximum number of line segments a cubic with the given device-space bounding box size
// would ever need to be divided into, raised to the 4th power. This is simply a special case of the
// cubic formula where we maximize its value by placing control points on specific corners of the
// bounding box.
AI float worst_case_cubic_pow4(float precision, float devWidth, float devHeight) {
    float kk = length_term_pow2<3>(precision);
    return 4*kk * (devWidth * devWidth + devHeight * devHeight);
}

// Returns the maximum number of line segments a cubic with the given device-space bounding box size
// would ever need to be divided into.
AI float worst_case_cubic(float precision, float devWidth, float devHeight) {
    return root4(worst_case_cubic_pow4(precision, devWidth, devHeight));
}

// Returns the maximum log2 number of line segments a cubic with the given device-space bounding box
// size would ever need to be divided into.
AI int worst_case_cubic_log2(float precision, float devWidth, float devHeight) {
    // nextlog16(x) == ceil(log2(sqrt(sqrt(x))))
    return nextlog16(worst_case_cubic_pow4(precision, devWidth, devHeight));
}

// Returns Wang's formula specialized for a conic curve, raised to the second power.
// Input points should be in projected space.
//
// This is not actually due to Wang, but is an analogue from (Theorem 3, corollary 1):
//   J. Zheng, T. Sederberg. "Estimating Tessellation Parameter Intervals for
//   Rational Curves and Surfaces." ACM Transactions on Graphics 19(1). 2000.
AI float conic_pow2(float precision,
                    float2 p0, float2 p1, float2 p2,
                    float w,
                    const VectorXform& vectorXform = VectorXform()) {
    p0 = vectorXform(p0);
    p1 = vectorXform(p1);
    p2 = vectorXform(p2);

    // Compute center of bounding box in projected space
    const float2 C = 0.5f * (skvx::min(skvx::min(p0, p1), p2) + skvx::max(skvx::max(p0, p1), p2));

    // Translate by -C. This improves translation-invariance of the formula,
    // see Sec. 3.3 of cited paper
    p0 -= C;
    p1 -= C;
    p2 -= C;

    // Compute max length
    const float max_len = sqrtf(std::max(dot(p0, p0), std::max(dot(p1, p1), dot(p2, p2))));

    // Compute forward differences
    const float2 dp = -2*w*p1 + p0 + p2;
    const float dw = fabsf(-2 * w + 2);

    // Compute numerator and denominator for parametric step size of linearization. Here, the
    // epsilon referenced from the cited paper is 1/precision.
    const float rp_minus_1 = std::max(0.f, max_len * precision - 1);
    const float numer = sqrtf(dot(dp, dp)) * precision + rp_minus_1 * dw;
    const float denom = 4 * std::min(w, 1.f);

    // Number of segments = sqrt(numer / denom).
    // This assumes parametric interval of curve being linearized is [t0,t1] = [0, 1].
    // If not, the number of segments is (tmax - tmin) / sqrt(denom / numer).
    return numer / denom;
}
AI float conic_pow2(float precision,
                    const SkPoint pts[],
                    float w,
                    const VectorXform& vectorXform = VectorXform()) {
    return conic_pow2(precision,
                      skvx::bit_pun<float2>(pts[0]),
                      skvx::bit_pun<float2>(pts[1]),
                      skvx::bit_pun<float2>(pts[2]),
                      w,
                      vectorXform);
}

// Returns the value of Wang's formula specialized for a conic curve.
AI float conic(float tolerance,
               const SkPoint pts[],
               float w,
               const VectorXform& vectorXform = VectorXform()) {
    return sqrtf(conic_pow2(tolerance, pts, w, vectorXform));
}

// Returns the log2 value of Wang's formula specialized for a conic curve, rounded up to the next
// int.
AI int conic_log2(float tolerance,
                  const SkPoint pts[],
                  float w,
                  const VectorXform& vectorXform = VectorXform()) {
    // nextlog4(x) == ceil(log2(sqrt(x)))
    return nextlog4(conic_pow2(tolerance, pts, w, vectorXform));
}

// Emits an SKSL function that calculates Wang's formula for the given set of 4 points. The points
// represent a cubic if w < 0, or if w >= 0, a conic defined by the first 3 points.
SK_MAYBE_UNUSED inline static SkString as_sksl() {
    SkString code;
    code.appendf(R"(
    // Returns the length squared of the largest forward difference from Wang's cubic formula.
    float wangs_formula_max_fdiff_pow2(float2 p0, float2 p1, float2 p2, float2 p3,
                                       float2x2 matrix) {
        float2 d0 = matrix * (fma(float2(-2), p1, p2) + p0);
        float2 d1 = matrix * (fma(float2(-2), p2, p3) + p1);
        return max(dot(d0,d0), dot(d1,d1));
    }
    float wangs_formula_cubic(float _precision_, float2 p0, float2 p1, float2 p2, float2 p3,
                              float2x2 matrix) {
        float m = wangs_formula_max_fdiff_pow2(p0, p1, p2, p3, matrix);
        return max(ceil(sqrt(%f * _precision_ * sqrt(m))), 1.0);
    }
    float wangs_formula_cubic_log2(float _precision_, float2 p0, float2 p1, float2 p2, float2 p3,
                                   float2x2 matrix) {
        float m = wangs_formula_max_fdiff_pow2(p0, p1, p2, p3, matrix);
        return ceil(log2(max(%f * _precision_ * _precision_ * m, 1.0)) * .25);
    })", length_term<3>(1), length_term_pow2<3>(1));

    code.appendf(R"(
    float wangs_formula_conic_pow2(float _precision_, float2 p0, float2 p1, float2 p2, float w) {
        // Translate the bounding box center to the origin.
        float2 C = (min(min(p0, p1), p2) + max(max(p0, p1), p2)) * 0.5;
        p0 -= C;
        p1 -= C;
        p2 -= C;

        // Compute max length.
        float m = sqrt(max(max(dot(p0,p0), dot(p1,p1)), dot(p2,p2)));

        // Compute forward differences.
        float2 dp = fma(float2(-2.0 * w), p1, p0) + p2;
        float dw = abs(fma(-2.0, w, 2.0));

        // Compute numerator and denominator for parametric step size of linearization. Here, the
        // epsilon referenced from the cited paper is 1/precision.
        float rp_minus_1 = max(0.0, fma(m, _precision_, -1.0));
        float numer = length(dp) * _precision_ + rp_minus_1 * dw;
        float denom = 4 * min(w, 1.0);

        return numer/denom;
    }
    float wangs_formula_conic(float _precision_, float2 p0, float2 p1, float2 p2, float w) {
        float n2 = wangs_formula_conic_pow2(_precision_, p0, p1, p2, w);
        return max(ceil(sqrt(n2)), 1.0);
    }
    float wangs_formula_conic_log2(float _precision_, float2 p0, float2 p1, float2 p2, float w) {
        float n2 = wangs_formula_conic_pow2(_precision_, p0, p1, p2, w);
        return ceil(log2(max(n2, 1.0)) * .5);
    })");

    return code;
}

}  // namespace skgpu::wangs_formula

#undef AI

#endif  // skgpu_tessellate_WangsFormula_DEFINED
