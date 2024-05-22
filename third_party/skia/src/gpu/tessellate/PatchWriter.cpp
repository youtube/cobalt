/*
 * Copyright 2021 Google LLC.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "src/gpu/tessellate/PatchWriter.h"

#include "src/gpu/tessellate/MiddleOutPolygonTriangulator.h"

#if SK_GPU_V1
#include "src/gpu/tessellate/PathTessellator.h"
#include "src/gpu/tessellate/StrokeTessellator.h"
#endif

namespace skgpu {

SK_ALWAYS_INLINE SkPoint to_skpoint(float2 p) { return skvx::bit_pun<SkPoint>(p); }

void write_triangle_stack(PatchWriter* writer,
                          MiddleOutPolygonTriangulator::PoppedTriangleStack&& stack) {
    for (auto [p0, p1, p2] : stack) {
        writer->writeTriangle(p0, p1, p2);
    }
}

#if SK_GPU_V1
PatchWriter::PatchWriter(GrMeshDrawTarget* target,
                         PathTessellator* tessellator,
                         int maxTessellationSegments,
                         int initialPatchAllocCount)
        : PatchWriter(target,
                      &tessellator->fVertexChunkArray,
                      tessellator->fAttribs,
                      maxTessellationSegments,
                      sizeof(SkPoint) * 4 + PatchAttribsStride(tessellator->fAttribs),
                      initialPatchAllocCount) {
}

PatchWriter::PatchWriter(GrMeshDrawTarget* target,
                         StrokeTessellator* tessellator,
                         int maxTessellationSegments,
                         int initialPatchAllocCount)
        : PatchWriter(target,
                      &tessellator->fVertexChunkArray,
                      tessellator->fAttribs,
                      maxTessellationSegments,
                      sizeof(SkPoint) * 4 + PatchAttribsStride(tessellator->fAttribs),
                      initialPatchAllocCount) {
}
#endif

void PatchWriter::chopAndWriteQuads(float2 p0, float2 p1, float2 p2, int numPatches) {
    // If we aren't fanning or stroking, we need to fill the space between chops with triangles.
    const bool needsInnerTriangles = this->writesCurvesOnly();
    MiddleOutPolygonTriangulator innerTriangulator(numPatches, to_skpoint(p0));
    for (; numPatches >= 3; numPatches -= 2) {
        // Chop into 3 quads.
        float4 T = float4(1,1,2,2) / numPatches;
        float4 ab = mix(p0.xyxy(), p1.xyxy(), T);
        float4 bc = mix(p1.xyxy(), p2.xyxy(), T);
        float4 abc = mix(ab, bc, T);
        // p1 & p2 of the cubic representation of the middle quad.
        float4 middle = mix(ab, bc, mix(T, T.zwxy(), 2/3.f));

        this->writeQuadPatch(p0, ab.lo, abc.lo);  // Write the 1st quad.
        if (needsInnerTriangles) {
            this->writeTriangle(p0, abc.lo, abc.hi);
        }
        this->writeCubicPatch(abc.lo, middle, abc.hi);  // Write the 2nd quad (as a cubic already)
        if (needsInnerTriangles) {
            write_triangle_stack(this, innerTriangulator.pushVertex(to_skpoint(abc.hi)));
        }
        std::tie(p0, p1) = {abc.hi, bc.hi};  // Save the 3rd quad.
    }
    if (numPatches == 2) {
        // Chop into 2 quads.
        float2 ab = (p0 + p1) * .5f;
        float2 bc = (p1 + p2) * .5f;
        float2 abc = (ab + bc) * .5f;

        this->writeQuadPatch(p0, ab, abc);  // Write the 1st quad.
        if (needsInnerTriangles) {
            this->writeTriangle(p0, abc, p2);
        }
        this->writeQuadPatch(abc, bc, p2);  // Write the 2nd quad.
    } else {
        SkASSERT(numPatches == 1);
        this->writeQuadPatch(p0, p1, p2);  // Write the single remaining quad.
    }
    if (needsInnerTriangles) {
        write_triangle_stack(this, innerTriangulator.pushVertex(to_skpoint(p2)));
        write_triangle_stack(this, innerTriangulator.close());
    }
}

void PatchWriter::chopAndWriteConics(float2 p0, float2 p1, float2 p2, float w, int numPatches) {
    // If we aren't fanning or stroking, we need to fill the space between chops with triangles.
    const bool needsInnerTriangles = this->writesCurvesOnly();
    MiddleOutPolygonTriangulator innerTriangulator(numPatches, to_skpoint(p0));
    // Load the conic in 3d homogeneous (unprojected) space.
    float4 h0 = float4(p0,1,1);
    float4 h1 = float4(p1,1,1) * w;
    float4 h2 = float4(p2,1,1);
    for (; numPatches >= 2; --numPatches) {
        // Chop in homogeneous space.
        float T = 1.f/numPatches;
        float4 ab = mix(h0, h1, T);
        float4 bc = mix(h1, h2, T);
        float4 abc = mix(ab, bc, T);

        // Project and write the 1st conic.
        float2 midpoint = abc.xy() / abc.w();
        this->writeConicPatch(h0.xy() / h0.w(),
                              ab.xy() / ab.w(),
                              midpoint,
                              ab.w() / sqrtf(h0.w() * abc.w()));
        if (needsInnerTriangles) {
            write_triangle_stack(this, innerTriangulator.pushVertex(to_skpoint(midpoint)));
        }
        std::tie(h0, h1) = {abc, bc};  // Save the 2nd conic (in homogeneous space).
    }
    // Project and write the remaining conic.
    SkASSERT(numPatches == 1);
    this->writeConicPatch(h0.xy() / h0.w(),
                          h1.xy() / h1.w(),
                          h2.xy(), // h2.w == 1
                          h1.w() / sqrtf(h0.w()));
    if (needsInnerTriangles) {
        write_triangle_stack(this, innerTriangulator.pushVertex(to_skpoint(h2.xy())));
        write_triangle_stack(this, innerTriangulator.close());
    }
}

void PatchWriter::chopAndWriteCubics(float2 p0, float2 p1, float2 p2, float2 p3, int numPatches) {
    // If we aren't fanning or stroking, we need to fill the space between chops with triangles.
    const bool needsInnerTriangles = this->writesCurvesOnly();
    MiddleOutPolygonTriangulator innerTriangulator(numPatches, to_skpoint(p0));
    for (; numPatches >= 3; numPatches -= 2) {
        // Chop into 3 cubics.
        float4 T = float4(1,1,2,2) / numPatches;
        float4 ab = mix(p0.xyxy(), p1.xyxy(), T);
        float4 bc = mix(p1.xyxy(), p2.xyxy(), T);
        float4 cd = mix(p2.xyxy(), p3.xyxy(), T);
        float4 abc = mix(ab, bc, T);
        float4 bcd = mix(bc, cd, T);
        float4 abcd = mix(abc, bcd, T);
        float4 middle = mix(abc, bcd, T.zwxy());  // p1 & p2 of the middle cubic.

        this->writeCubicPatch(p0, ab.lo, abc.lo, abcd.lo);  // Write the 1st cubic.
        if (needsInnerTriangles) {
            this->writeTriangle(p0, abcd.lo, abcd.hi);
        }
        this->writeCubicPatch(abcd.lo, middle, abcd.hi);  // Write the 2nd cubic.
        if (needsInnerTriangles) {
            write_triangle_stack(this, innerTriangulator.pushVertex(to_skpoint(abcd.hi)));
        }
        std::tie(p0, p1, p2) = {abcd.hi, bcd.hi, cd.hi};  // Save the 3rd cubic.
    }
    if (numPatches == 2) {
        // Chop into 2 cubics.
        float2 ab = (p0 + p1) * .5f;
        float2 bc = (p1 + p2) * .5f;
        float2 cd = (p2 + p3) * .5f;
        float2 abc = (ab + bc) * .5f;
        float2 bcd = (bc + cd) * .5f;
        float2 abcd = (abc + bcd) * .5f;

        this->writeCubicPatch(p0, ab, abc, abcd);  // Write the 1st cubic.
        if (needsInnerTriangles) {
            this->writeTriangle(p0, abcd, p3);
        }
        this->writeCubicPatch(abcd, bcd, cd, p3);  // Write the 2nd cubic.
    } else {
        SkASSERT(numPatches == 1);
        this->writeCubicPatch(p0, p1, p2, p3);  // Write the single remaining cubic.
    }
    if (needsInnerTriangles) {
        write_triangle_stack(this, innerTriangulator.pushVertex(to_skpoint(p3)));
        write_triangle_stack(this, innerTriangulator.close());
    }
}

}  // namespace skgpu
