/*
 * Copyright 2026 Google LLC
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include "src/gpu/graphite/sparse_strips/Flatten.h"

#include "src/gpu/graphite/sparse_strips/Polyline.h"

namespace skgpu::graphite {

namespace {

// An intermediate helper struct used to pass values between anonymous namespace functions and
// Flatten class functions. Used for flattenning both quads and cubics.
struct FlattenParams {
    // The starting value of the parabolic integral for each quadratic segment. Maps the start of
    // the curve into uniform arc-length space.
    double fA0;
    // The delta of the parabolic integral (a2 - a0) for each quadratic. Used to linearly
    // interpolate the integral 'a' across the segment.
    double fDa;
    // Inverse integral at fA0. Used to anchor the mapped parametric value back to 0.
    double fU0;
    // Normalization factor (1.0 / (u2 - u0)) to scale the final 't' into the [0, 1] range.
    double fUScale;
    // Total integrated curvature/error metric for the segment. Used to compute the required number
    // of subdivisions.
    double fCurvatureIntegral;
};

SK_ALWAYS_INLINE double approx_parabola_integral(double x) {
    constexpr double kD = 0.67;
    constexpr double kD_pow4 = kD * kD * kD * kD;
    return x / ( 1.0 - kD + std::sqrt(std::sqrt(kD_pow4 + 0.25 * x * x)));
}

SK_ALWAYS_INLINE double approx_parabola_inv_integral(double x) {
    constexpr double kB = 0.39;
    return x * (1.0 - kB + std::sqrt(kB * kB + 0.25 * x * x));
}

/*
 * Determing the number of quads from a cubic:
 *
 * 1. Error Budgeting (kCubicAccuracy):
 *    Flattening cubics happens in two steps: Cubic -> Quads, then Quads -> Lines. If the first step
 *    uses the full visual tolerance, the final lines will look jagged. kCubicAccuracy allocates
 *    only 10% (kCubicErrTolerance) of the total allowed pixel error to the Cubic -> Quads steps,
 *    saving 90% (kQuadToLineTolFromCubic) for the final lines.
 *
 * 2. Wang's Formula and the Vector Error (errV):
 *    Wang's formula gives the minimum number of evenly spaced (in parametric space) line segments
 *    that a bezier curve of degree "n" must be chopped into to guarantee all lines stay within a
 *    distance of "1/precision" pixels from the true curve:
 *
 *    maxLength = max([length(p[i+2] - 2p[i+1] + p[i]) for (0 <= i <= n-2)])
 *    numParametricSegments = sqrt(maxLength * precision * n*(n - 1)/8)
 *
 *    (Goldman, Ron. (2003). 5.6.3 Wang's Formula. "Pyramid Algorithms: A Dynamic Programming
 *    Approach to Curves and Surfaces for Geometric Modeling". Morgan Kaufmann Publishers.)
 *
 *    Since we are flattening a cubic to a *quadratic*, we shift the math up one degree and evaluate
 *    the third derivative instead: (3*P2 - P3) - (3*P1 - P0). If this evaluates to zero, the second
 *    derivative is constant, meaning the curve is a perfect parabola. We use this expression 'errV'
 *    as a measure of how much the cubic "bulges" away from an ideal quadratic shape.
 *
 * 3. Error Quantization and Lookup Table
 *    From Wang's formula, the maximum geometric error of the un-chopped cubic is
 *    ||errV|| / (12*sqrt(3)). By Taylor's theorem, if we chop the curve into n segments, the
 *    approximation error shrinks cubically (Base Error / n^3). Thus, to satisfy our desired
 *    tolerance, we must find the minimum n where:
 *
 *    ((||errV|| / (12*sqrt(3)) / n^3) <= kCubicErrTolerance.
 *
 *    To avoid calling sqrt, we square the equation: (12*sqrt(3))^2 = 432. Correspondingly,
 *    (n^3)^2 = n^6, so errDiv scales at n^6. To avoid calculating 6th roots, and because we know we
 *    can only have an integer number of subdivisions, we precompute a lookup table (kPowSixLut) of
 *    6th powers to find the required number of subdivisions.
 *
 * 4. Subdivision limit:
 *    As a guard against pathological inputs, and to avoid dynamic memory allocation, we bound the
 *    maximum number of quads that can be produced by a cubic. If errDiv exceeds the table, the
 *    function returns kMaxQuadsFromCubic.
 */
SK_ALWAYS_INLINE uint32_t estimate_num_quads_from_cubic(const SkPoint pts[4]) {
    // (12 * sqrt(3))^2 * kCubicErrTolerance^2, inverted to avoid division
    constexpr double kWangErrThreshold =
            1.0 / (432.0 * Flatten::kCubicErrTolerance * Flatten::kCubicErrTolerance);

    // A cubic is a perfect quadratic if (3*P1 - P0) == (3*P2 - P3)
    SkPoint leftQuadCtrlX2 = pts[1] * 3.0f - pts[0];
    SkPoint rightQuadCtrlX2 = pts[2] * 3.0f - pts[3];
    SkPoint errV = rightQuadCtrlX2 - leftQuadCtrlX2;

    // Squared error distance
    double errSq = SkPoint::DotProduct(errV, errV);

    // The relative error mapped into n^6
    double errDiv = errSq * kWangErrThreshold;

    constexpr double kPowSixLut[Flatten::kMaxQuadsFromCubic] = {
            1.0,
            64.0,
            729.0,
            4096.0,
            15625.0,
            46656.0,
            117649.0,
            262144.0,
            531441.0,
            1000000.0,
            1771561.0,
            2985984.0,
            4826809.0,
            7529536.0,
            11390625.0,
            16777216.0,
    };

    for (uint32_t i = 0; i < Flatten::kMaxQuadsFromCubic; ++i) {
        if (errDiv <= kPowSixLut[i]) {
            return i + 1;
        }
    }
    return Flatten::kMaxQuadsFromCubic;
}

SK_ALWAYS_INLINE FlattenParams estimate_lines_from_quad(const SkPoint pts[3],
                                                        double kSqrtErrTolerance) {
    SkPoint d01 = pts[1] - pts[0];
    SkPoint d12 = pts[2] - pts[1];
    SkPoint dd = d01 - d12;

    double cross = SkPoint::CrossProduct(pts[2] - pts[0], dd);
    // If the quad's points are sufficiently collinear, we simplify it to a single line spanning
    // the endpoints to prevent division by zero.
    //
    // For perfectly collinear quads, this produces a correct image; in a filled path, the
    // collapsed control point adds no area. While this shortcut is technically incorrect for
    // "needle-thin" nearly collinear quads, it is extremely unlikely such a path would enclose
    // an MSAA subsample point, rendering extra handling unnecessary.
    if (std::abs(cross) < Flatten::kEpsilonD) {
        return {0.0, 0.0, 0.0, 1.0, 0.0};
    }

    double x0 = SkPoint::DotProduct(d01, dd) / cross;
    double x2 = SkPoint::DotProduct(d12, dd) / cross;
    double scale = std::abs(cross / (dd.length() * (x2 - x0)));

    double a0 = approx_parabola_integral(x0);
    double a2 = approx_parabola_integral(x2);
    double da = a2 - a0;
    double val = 0.0;

    if (std::isfinite(scale)) {
        double absDa = std::abs(da);
        double sqrtScale = std::sqrt(scale);

        if (std::signbit(x0) == std::signbit(x2)) {
            val = absDa * sqrtScale;
        } else {
            double xMin = kSqrtErrTolerance / sqrtScale;
            val = kSqrtErrTolerance * absDa / approx_parabola_integral(xMin);
        }
    }

    double u0 = approx_parabola_inv_integral(a0);
    double u2 = approx_parabola_inv_integral(a2);
    double uScale = 1.0 / (u2 - u0);

    return {a0, da, u0, uScale, val};
}

SK_ALWAYS_INLINE double determine_quad_subdiv_t(const FlattenParams& params, double x) {
    double a = params.fA0 + params.fDa * x;
    double u = approx_parabola_inv_integral(a);
    return (u - params.fU0) * params.fUScale;
}

SK_ALWAYS_INLINE SkPoint eval_quad_scalar(const SkPoint pts[3], float t) {
    float mt = 1.0f - t;
    return pts[0] * (mt * mt) + pts[1] * (2.0f * mt * t) + pts[2] * (t * t);
}

SK_ALWAYS_INLINE SkPoint eval_cubic_scalar(const SkPoint pts[4], float t) {
    float mt = 1.0f - t;
    float mt2 = mt * mt;
    float t2 = t * t;
    return pts[0] * (mt2 * mt) + pts[1] * (3.0f * mt2 * t) + pts[2] * (3.0f * mt * t2) +
           pts[3] * (t2 * t);
}

SK_ALWAYS_INLINE bool is_within_dist_sq(SkPoint p, SkPoint a, SkPoint b, float kErrTolerance) {
    SkPoint ab = b - a;
    SkPoint ap = p - a;
    float lenSq = SkPoint::DotProduct(ab, ab);
    float tNum = SkPoint::DotProduct(ap, ab);

    if (lenSq == 0.0f || tNum <= 0.0f) {
        return SkPoint::DotProduct(ap, ap) <= kErrTolerance;
    }
    if (tNum >= lenSq) {
        SkPoint bp = p - b;
        return SkPoint::DotProduct(bp, bp) <= kErrTolerance;
    }

    // Lagrange's identity: apSq*lenSq - tNum*tNum == cross^2
    float cross = SkPoint::CrossProduct(ap, ab);
    return (cross * cross) <= (kErrTolerance * lenSq);
}

template <bool kIsIdentity,
          typename ProcessQuadFn,
          typename ProcessConicFn,
          typename ProcessCubicFn>
SK_ALWAYS_INLINE void processPathsImpl(const SkPath& path,
                                       const SkMatrix& ctm,
                                       Polyline* polyline,
                                       ProcessQuadFn&& processQuad,
                                       ProcessConicFn&& processConic,
                                       ProcessCubicFn&& processCubic) {
    bool closed = true;
    SkPoint startPt = {0, 0};
    SkPoint lastPt = {0, 0};

    SkPath::Iter iter(path, false);
    SkPoint localPts[4];
    SkPoint mappedPts[4];
    SkPath::Verb verb;

    while ((verb = iter.next(localPts)) != SkPath::kDone_Verb) {
        switch (verb) {
            case SkPath::kMove_Verb: {
                const SkPoint* pts = localPts;
                if constexpr (!kIsIdentity) {
                    ctm.mapPoints(SkSpan(mappedPts, 1), SkSpan(localPts, 1));
                    pts = mappedPts;
                }

                if (!closed && lastPt != startPt) {
                    polyline->appendPoint(startPt);
                }
                polyline->appendSentinel();

                closed = false;
                lastPt = pts[0];
                startPt = pts[0];
                polyline->appendPoint(pts[0]);
                break;
            }
            case SkPath::kLine_Verb: {
                SkASSERT(!closed);
                const SkPoint* pts = localPts;
                if constexpr (!kIsIdentity) {
                    ctm.mapPoints(SkSpan(mappedPts, 2), SkSpan(localPts, 2));
                    pts = mappedPts;
                }

                lastPt = pts[1];
                polyline->appendPoint(pts[1]);
                break;
            }
            case SkPath::kQuad_Verb: {
                SkASSERT(!closed);
                const SkPoint* pts = localPts;
                if constexpr (!kIsIdentity) {
                    ctm.mapPoints(SkSpan(mappedPts, 3), SkSpan(localPts, 3));
                    pts = mappedPts;
                }

                processQuad(pts);
                lastPt = pts[2];
                break;
            }
            case SkPath::kConic_Verb: {
                SkASSERT(!closed);
                const SkPoint* pts = localPts;
                if constexpr (!kIsIdentity) {
                    ctm.mapPoints(SkSpan(mappedPts, 3), SkSpan(localPts, 3));
                    pts = mappedPts;
                }

                processConic(pts, iter.conicWeight());
                lastPt = pts[2];
                break;
            }
            case SkPath::kCubic_Verb: {
                SkASSERT(!closed);
                const SkPoint* pts = localPts;
                if constexpr (!kIsIdentity) {
                    ctm.mapPoints(SkSpan(mappedPts, 4), SkSpan(localPts, 4));
                    pts = mappedPts;
                }

                processCubic(pts);
                lastPt = pts[3];
                break;
            }
            case SkPath::kClose_Verb: {
                closed = true;
                if (lastPt != startPt) {
                    polyline->appendPoint(startPt);
                }
                break;
            }
            default:
                break;
        }
    }

    if (!closed && lastPt != startPt) {
        polyline->appendPoint(startPt);
    }

    polyline->appendSentinel();
}

}  // anonymous namespace

SK_ALWAYS_INLINE void Flatten::flattenQuadScalar(const SkPoint pts[3], Polyline* polyline) {
    FlattenParams params = estimate_lines_from_quad(pts, kSqrtQuadTolerance);
    // Calculate the minimum number of linear subdivisions needed to stay within the visual
    // tolerance. The maximum geometric error of a straight line approximating a parabola is
    // (a * W^2) / 4. Taking the square root makes this easier: sqrt(Error) = (sqrt(a) * W) / 2
    // since fCurvatureIntegral = sqrt(a) * W_total. So the required number of segments 'n' to keep
    // the error per segment under `kSqrtQuadTolerance` is:
    // n = fCurvatureIntegral / (kSqrtQuadTolerance * 2).
    uint32_t numSegments = std::max(1u, static_cast<uint32_t>(
            std::ceil(0.5 / kSqrtQuadTolerance * params.fCurvatureIntegral)));
    double step = 1.0 / numSegments;

    for (uint32_t i = 1; i < numSegments; ++i) {
        double u = i * step;
        double t = determine_quad_subdiv_t(params, u);
        polyline->appendPoint(eval_quad_scalar(pts, static_cast<float>(t)));
    }
    polyline->appendPoint(pts[2]);
}

SK_ALWAYS_INLINE uint32_t Flatten::flattenCubicScalar(const SkPoint pts[4]) {
    uint32_t nQuads = estimate_num_quads_from_cubic(pts);
    fContext.fNumQuads = nQuads;
    static constexpr float kHalfOverN[kMaxQuadsFromCubic + 1] = {
        0.0f,
        0.5f/1.0f, 0.5f/2.0f, 0.5f/3.0f, 0.5f/4.0f,
        0.5f/5.0f, 0.5f/6.0f, 0.5f/7.0f, 0.5f/8.0f,
        0.5f/9.0f, 0.5f/10.f, 0.5f/11.f, 0.5f/12.f,
        0.5f/13.f, 0.5f/14.f, 0.5f/15.f, 0.5f/16.f
    };
    float dt = kHalfOverN[nQuads];

    float curvatureIntegralSum = 0.0f;

    for (uint32_t i = 0; i < nQuads; ++i) {
        float t0 = (2 * i) * dt;
        float t1 = (2 * i + 1) * dt;
        float t2 = (2 * i + 2) * dt;

        SkPoint p0 = eval_cubic_scalar(pts, t0);
        SkPoint pOneHalf = eval_cubic_scalar(pts, t1);
        SkPoint p2 = eval_cubic_scalar(pts, t2);
        SkPoint p1 = pOneHalf * 2.0f - (p0 + p2) * 0.5f;

        fContext.fEvenPts[i] = p0;
        fContext.fOddPts[i] = p1;
        fContext.fEvenPts[i + 1] = p2;

        SkPoint quad[3] = {p0, p1, p2};
        FlattenParams params = estimate_lines_from_quad(quad, kSqrtQuadTolerance);

        fContext.fA0[i] = params.fA0;
        fContext.fDa[i] = params.fDa;
        fContext.fU0[i] = params.fU0;
        fContext.fUScale[i] = params.fUScale;

        fContext.fCurvatureIntegral[i] =
                std::max(static_cast<float>(params.fCurvatureIntegral), kEpsilonF);

        curvatureIntegralSum += params.fCurvatureIntegral;
    }

    uint32_t numSegments = std::max<uint32_t>(
        1u, static_cast<uint32_t>(std::ceil(0.5 * curvatureIntegralSum / kSqrtQuadFromCubicTol)));
    uint32_t targetLen = numSegments + 4;
    fContext.fFlattenedCubics.resize(targetLen);

    float step = curvatureIntegralSum / numSegments;
    float stepRecip = 1.0f / step;
    float cumulativeCurvature = 0.0f;
    uint32_t lastN = 0;
    float x0Base = 0.0f;

    // Flatten each quad produced by the cubic.
    for (uint32_t i = 0; i < nQuads; ++i) {
        cumulativeCurvature += fContext.fCurvatureIntegral[i];
        float thisN = cumulativeCurvature * stepRecip;
        float thisNNext = 1.0f + std::floor(thisN);
        uint32_t dn = static_cast<uint32_t>(thisNNext) - lastN; // thisNNext always > lastN

        if (dn > 0) {
            float dx = step / fContext.fCurvatureIntegral[i];
            float x0 = x0Base * dx;

            SkPoint p0 = fContext.fEvenPts[i];
            SkPoint p1 = fContext.fOddPts[i];
            SkPoint p2 = fContext.fEvenPts[i + 1];

            float a0 = fContext.fA0[i];
            float da = fContext.fDa[i];
            float u0 = fContext.fU0[i];
            float uScale = fContext.fUScale[i];

            for (uint32_t j = 0; j < dn; ++j) {
                float x = x0 + j * dx;
                float a = a0 + da * x;
                float u = approx_parabola_inv_integral(a);
                float t = (u - u0) * uScale;

                float mt = 1.0f - t;
                SkPoint p = p0 * (mt * mt) + p1 * (2.0f * t * mt) + p2 * (t * t);
                fContext.fFlattenedCubics[lastN + j] = p;
            }
        }
        x0Base = thisNNext - thisN;
        lastN = static_cast<uint32_t>(thisNNext);
    }

    fContext.fFlattenedCubics[numSegments] = fContext.fEvenPts[nQuads];
    return numSegments + 1;
}

[[maybe_unused]]
SK_ALWAYS_INLINE void Flatten::flattenQuadSimd() {
    // Stub
}

[[maybe_unused]]
SK_ALWAYS_INLINE void Flatten::evalCubicsSimd() {
    // Stub
}

[[maybe_unused]]
SK_ALWAYS_INLINE void Flatten::estimateLinesFromQuadSimd() {
    // Stub
}

[[maybe_unused]]
SK_ALWAYS_INLINE void Flatten::outputLinesFromQuadSimd() {
    // Stub
}

[[maybe_unused]]
uint32_t Flatten::flattenCubicSimd() {
    // Stub
    return 0;
}

void Flatten::processPathsSimd(
        const SkPath& path, const SkMatrix& ctm, float width, float height, Polyline* polyline) {
    // Stub
}

void Flatten::processPathsScalar(
        const SkPath& path, const SkMatrix& ctm, float width, float height, Polyline* polyline) {
    fContext.fFlattenedCubics.clear();

    auto processQuad = [this, width, height, polyline](const SkPoint pts[3]) {
        // Is the quad completely outside of the viewport?
        bool culled = (pts[0].fX > width && pts[1].fX > width && pts[2].fX > width) ||
                      (pts[0].fX < 0.0f && pts[1].fX < 0.0f && pts[2].fX < 0.0f) ||
                      (pts[0].fY > height && pts[1].fY > height && pts[2].fY > height) ||
                      (pts[0].fY < 0.0f && pts[1].fY < 0.0f && pts[2].fY < 0.0f);
        // Is the quad sufficiently similar to a line?
        bool isLine = is_within_dist_sq(pts[1], pts[0], pts[2], kQuadSubdivThreshold);
        if (culled || isLine) {
            polyline->appendPoint(pts[2]);
        } else {
            this->flattenQuadScalar(pts, polyline);
        }
    };

    auto processConic = [this, &processQuad](const SkPoint pts[3], float weight) {
        const SkPoint* quadPts = fConicToQuad.computeQuads(pts, weight, kQuadErrTolerance);
        int quadCount = fConicToQuad.countQuads();

        for (int i = 0; i < quadCount; ++i) {
            processQuad(&quadPts[i * 2]);
        }
    };

    auto processCubic = [this, width, height, polyline](const SkPoint pts[4]) {
        // Is the cubic completely outside of the viewport?
        bool culled =
                (pts[0].fX > width && pts[1].fX > width && pts[2].fX > width &&
                 pts[3].fX > width) ||
                (pts[0].fX < 0.0f && pts[1].fX < 0.0f && pts[2].fX < 0.0f && pts[3].fX < 0.0f) ||
                (pts[0].fY > height && pts[1].fY > height && pts[2].fY > height &&
                 pts[3].fY > height) ||
                (pts[0].fY < 0.0f && pts[1].fY < 0.0f && pts[2].fY < 0.0f && pts[3].fY < 0.0f);
        // Is the cubic sufficiently similar to a line?
        bool isLine =
                (is_within_dist_sq(pts[1], pts[0], pts[3], kCubicSubdivThreshold) &&
                 is_within_dist_sq(pts[2], pts[0], pts[3], kCubicSubdivThreshold));

        if (culled || isLine) {
            polyline->appendPoint(pts[3]);
        } else {
            uint32_t numSegments = this->flattenCubicScalar(pts);
            for (uint32_t i = 1; i < numSegments; ++i) {
                polyline->appendPoint(fContext.fFlattenedCubics[i]);
            }
        }
    };

    if (ctm.isIdentity()) {
        processPathsImpl</*kIsIdentity=*/true>(
                path, ctm, polyline, processQuad, processConic, processCubic);
    } else {
        processPathsImpl</*kIsIdentity=*/false>(
                path, ctm, polyline, processQuad, processConic, processCubic);
    }
}

}  // namespace skgpu::graphite
