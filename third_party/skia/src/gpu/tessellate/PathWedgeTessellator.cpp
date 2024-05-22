/*
 * Copyright 2021 Google LLC.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "src/gpu/tessellate/PathWedgeTessellator.h"

#include "src/core/SkPathPriv.h"
#include "src/gpu/tessellate/AffineMatrix.h"
#include "src/gpu/tessellate/PatchWriter.h"
#include "src/gpu/tessellate/PathCurveTessellator.h"
#include "src/gpu/tessellate/WangsFormula.h"

#if SK_GPU_V1
#include "src/gpu/GrMeshDrawTarget.h"
#include "src/gpu/GrOpFlushState.h"
#include "src/gpu/GrResourceProvider.h"
#endif

namespace skgpu {

namespace {

// Parses out each contour in a path and tracks the midpoint. Example usage:
//
//   SkTPathContourParser parser;
//   while (parser.parseNextContour()) {
//       SkPoint midpoint = parser.currentMidpoint();
//       for (auto [verb, pts] : parser.currentContour()) {
//           ...
//       }
//   }
//
class MidpointContourParser {
public:
    MidpointContourParser(const SkPath& path)
            : fPath(path)
            , fVerbs(SkPathPriv::VerbData(fPath))
            , fNumRemainingVerbs(fPath.countVerbs())
            , fPoints(SkPathPriv::PointData(fPath))
            , fWeights(SkPathPriv::ConicWeightData(fPath)) {}
    // Advances the internal state to the next contour in the path. Returns false if there are no
    // more contours.
    bool parseNextContour() {
        bool hasGeometry = false;
        for (; fVerbsIdx < fNumRemainingVerbs; ++fVerbsIdx) {
            switch (fVerbs[fVerbsIdx]) {
                case SkPath::kMove_Verb:
                    if (!hasGeometry) {
                        fMidpoint = {0,0};
                        fMidpointWeight = 0;
                        this->advance();  // Resets fPtsIdx to 0 and advances fPoints.
                        fPtsIdx = 1;  // Increment fPtsIdx past the kMove.
                        continue;
                    }
                    if (fPoints[0] != fPoints[fPtsIdx - 1]) {
                        // There's an implicit close at the end. Add the start point to our mean.
                        fMidpoint += fPoints[0];
                        ++fMidpointWeight;
                    }
                    return true;
                default:
                    continue;
                case SkPath::kLine_Verb:
                    ++fPtsIdx;
                    break;
                case SkPath::kConic_Verb:
                    ++fWtsIdx;
                    [[fallthrough]];
                case SkPath::kQuad_Verb:
                    fPtsIdx += 2;
                    break;
                case SkPath::kCubic_Verb:
                    fPtsIdx += 3;
                    break;
            }
            fMidpoint += fPoints[fPtsIdx - 1];
            ++fMidpointWeight;
            hasGeometry = true;
        }
        if (hasGeometry && fPoints[0] != fPoints[fPtsIdx - 1]) {
            // There's an implicit close at the end. Add the start point to our mean.
            fMidpoint += fPoints[0];
            ++fMidpointWeight;
        }
        return hasGeometry;
    }

    // Allows for iterating the current contour using a range-for loop.
    SkPathPriv::Iterate currentContour() {
        return SkPathPriv::Iterate(fVerbs, fVerbs + fVerbsIdx, fPoints, fWeights);
    }

    SkPoint currentMidpoint() { return fMidpoint * (1.f / fMidpointWeight); }

private:
    void advance() {
        fVerbs += fVerbsIdx;
        fNumRemainingVerbs -= fVerbsIdx;
        fVerbsIdx = 0;
        fPoints += fPtsIdx;
        fPtsIdx = 0;
        fWeights += fWtsIdx;
        fWtsIdx = 0;
    }

    const SkPath& fPath;

    const uint8_t* fVerbs;
    int fNumRemainingVerbs = 0;
    int fVerbsIdx = 0;

    const SkPoint* fPoints;
    int fPtsIdx = 0;

    const float* fWeights;
    int fWtsIdx = 0;

    SkPoint fMidpoint;
    int fMidpointWeight;
};

}  // namespace

int PathWedgeTessellator::patchPreallocCount(int totalCombinedPathVerbCnt) const {
    // Over-allocate enough wedges for 1 in 4 to chop.
    int maxWedges = MaxCombinedFanEdgesInPathDrawList(totalCombinedPathVerbCnt);
    return (maxWedges * 5 + 3) / 4;  // i.e., ceil(maxWedges * 5/4)
}

void PathWedgeTessellator::writePatches(PatchWriter& patchWriter,
                                        const SkMatrix& shaderMatrix,
                                        const PathDrawList& pathDrawList) {
    wangs_formula::VectorXform shaderXform(shaderMatrix);
    for (auto [pathMatrix, path, color] : pathDrawList) {
        AffineMatrix m(pathMatrix);
        if (fAttribs & PatchAttribs::kColor) {
            patchWriter.updateColorAttrib(color);
        }
        MidpointContourParser parser(path);
        while (parser.parseNextContour()) {
            patchWriter.updateFanPointAttrib(m.mapPoint(parser.currentMidpoint()));
            SkPoint lastPoint = {0, 0};
            SkPoint startPoint = {0, 0};
            for (auto [verb, pts, w] : parser.currentContour()) {
                switch (verb) {
                    case SkPathVerb::kMove: {
                        startPoint = lastPoint = pts[0];
                        break;
                    }

                    case SkPathVerb::kLine: {
                        // Explicitly convert the line to an equivalent cubic w/ four distinct
                        // control points because it fans better and avoids double-hitting pixels.
                        patchWriter.writeLine(m.map2Points(pts));
                        lastPoint = pts[1];
                        break;
                    }

                    case SkPathVerb::kQuad: {
                        auto [p0, p1] = m.map2Points(pts);
                        auto p2 = m.map1Point(pts+2);

                        patchWriter.writeQuadratic(p0, p1, p2, shaderXform);
                        lastPoint = pts[2];
                        break;
                    }

                    case SkPathVerb::kConic: {
                        auto [p0, p1] = m.map2Points(pts);
                        auto p2 = m.map1Point(pts+2);

                        patchWriter.writeConic(p0, p1, p2, *w, shaderXform);
                        lastPoint = pts[2];
                        break;
                    }

                    case SkPathVerb::kCubic: {
                        auto [p0, p1] = m.map2Points(pts);
                        auto [p2, p3] = m.map2Points(pts+2);

                        patchWriter.writeCubic(p0, p1, p2, p3, shaderXform);
                        lastPoint = pts[3];
                        break;
                    }

                    case SkPathVerb::kClose: {
                        break;  // Ignore. We can assume an implicit close at the end.
                    }
                }
            }
            if (lastPoint != startPoint) {
                SkPoint pts[2] = {lastPoint, startPoint};
                patchWriter.writeLine(m.map2Points(pts));
            }
        }
    }

    // We already chopped curves to make sure none needed a higher resolveLevel than
    // kMaxFixedResolveLevel.
    fFixedResolveLevel = SkTPin(patchWriter.requiredResolveLevel(),
                                fFixedResolveLevel,
                                int(kMaxFixedResolveLevel));
}

void PathWedgeTessellator::WriteFixedVertexBuffer(VertexWriter vertexWriter, size_t bufferSize) {
    SkASSERT(bufferSize >= sizeof(SkPoint));

    // Start out with the fan point. A negative resolve level indicates the fan point.
    vertexWriter << -1.f/*resolveLevel*/ << -1.f/*idx*/;

    // The rest is the same as for curves.
    PathCurveTessellator::WriteFixedVertexBuffer(std::move(vertexWriter),
                                                 bufferSize - sizeof(SkPoint));
}

void PathWedgeTessellator::WriteFixedIndexBuffer(VertexWriter vertexWriter, size_t bufferSize) {
    SkASSERT(bufferSize >= sizeof(uint16_t) * 3);

    // Start out with the fan triangle.
    vertexWriter << (uint16_t)0 << (uint16_t)1 << (uint16_t)2;

    // The rest is the same as for curves, with a baseIndex of 1.
    PathCurveTessellator::WriteFixedIndexBufferBaseIndex(std::move(vertexWriter),
                                                         bufferSize - sizeof(uint16_t) * 3,
                                                         1);
}

#if SK_GPU_V1

SKGPU_DECLARE_STATIC_UNIQUE_KEY(gFixedVertexBufferKey);
SKGPU_DECLARE_STATIC_UNIQUE_KEY(gFixedIndexBufferKey);

void PathWedgeTessellator::prepareFixedCountBuffers(GrMeshDrawTarget* target) {
    GrResourceProvider* rp = target->resourceProvider();

    SKGPU_DEFINE_STATIC_UNIQUE_KEY(gFixedVertexBufferKey);

    fFixedVertexBuffer = rp->findOrMakeStaticBuffer(GrGpuBufferType::kVertex,
                                                    FixedVertexBufferSize(kMaxFixedResolveLevel),
                                                    gFixedVertexBufferKey,
                                                    WriteFixedVertexBuffer);

    SKGPU_DEFINE_STATIC_UNIQUE_KEY(gFixedIndexBufferKey);

    fFixedIndexBuffer = rp->findOrMakeStaticBuffer(GrGpuBufferType::kIndex,
                                                   FixedIndexBufferSize(kMaxFixedResolveLevel),
                                                   gFixedIndexBufferKey,
                                                   WriteFixedIndexBuffer);
}

void PathWedgeTessellator::drawTessellated(GrOpFlushState* flushState) const {
    for (const GrVertexChunk& chunk : fVertexChunkArray) {
        flushState->bindBuffers(nullptr, nullptr, chunk.fBuffer);
        flushState->draw(chunk.fCount * 5, chunk.fBase * 5);
    }
}

void PathWedgeTessellator::drawFixedCount(GrOpFlushState* flushState) const {
    if (!fFixedVertexBuffer || !fFixedIndexBuffer) {
        return;
    }
    // Emit 3 vertices per curve triangle, plus 3 more for the fan triangle.
    int fixedIndexCount = (NumCurveTrianglesAtResolveLevel(fFixedResolveLevel) + 1) * 3;
    for (const GrVertexChunk& chunk : fVertexChunkArray) {
        flushState->bindBuffers(fFixedIndexBuffer, chunk.fBuffer, fFixedVertexBuffer);
        flushState->drawIndexedInstanced(fixedIndexCount, 0, chunk.fCount, chunk.fBase, 0);
    }
}

#endif

}  // namespace skgpu
