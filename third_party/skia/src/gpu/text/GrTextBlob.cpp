/*
 * Copyright 2015 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "include/core/SkColorFilter.h"
#include "include/gpu/GrRecordingContext.h"
#include "include/private/SkTemplates.h"
#include "include/private/chromium/GrSlug.h"
#include "include/private/chromium/SkChromeRemoteGlyphCache.h"
#include "src/core/SkFontPriv.h"
#include "src/core/SkMaskFilterBase.h"
#include "src/core/SkMatrixProvider.h"
#include "src/core/SkPaintPriv.h"
#include "src/core/SkReadBuffer.h"
#include "src/core/SkStrikeCache.h"
#include "src/core/SkStrikeSpec.h"
#include "src/gpu/GrClip.h"
#include "src/gpu/GrGlyph.h"
#include "src/gpu/GrMeshDrawTarget.h"
#include "src/gpu/GrRecordingContextPriv.h"
#include "src/gpu/GrStyle.h"
#include "src/gpu/SkGr.h"
#include "src/gpu/effects/GrDistanceFieldGeoProc.h"
#include "src/gpu/geometry/GrStyledShape.h"
#include "src/gpu/text/GrAtlasManager.h"
#include "src/gpu/text/GrGlyphVector.h"
#include "src/gpu/text/GrStrikeCache.h"
#include "src/gpu/text/GrTextBlob.h"

#include "src/gpu/GrBlurUtils.h"
#include "src/gpu/ops/AtlasTextOp.h"
#include "src/gpu/v1/Device_v1.h"
#include "src/gpu/v1/SurfaceDrawContext_v1.h"

using AtlasTextOp = skgpu::v1::AtlasTextOp;

// -- GPU Text -------------------------------------------------------------------------------------
// There are three broad types of SubRun implementations for drawing text using the GPU.
// GrTextBlob (runs with no postfix) - these runs support drawing for GrTextBlobs.
// GrSlug (Slug postfix) - these runs support drawing of GrSlugs.
// (NoCache postfix) - These runs support Canvas direct drawing like drawText, etc.
//
// Naming conventions
//  * drawMatrix - the CTM from the canvas.
//  * drawOrigin - the x, y location of the drawTextBlob call.
//  * positionMatrix - this is the combination of the drawMatrix and the drawOrigin:
//        positionMatrix = drawMatrix * TranslationMatrix(drawOrigin.x, drawOrigin.y);
//
// Note:
//   In order to use GrSlugs, you need to set the fSupportBilerpFromGlyphAtlas on GrContextOptions.

enum GrSubRun::SubRunType : int {
    kBad = 0,  // Make this 0 to line up with errors from readInt.
    kDirectMask,
    kSDFT,
    kTransformMask,
    kPath,
    kDrawable,
    kSubRunTypeCount,
};

// -- GrBlobSubRun ---------------------------------------------------------------------------------
class GrBlobSubRun {
public:
    virtual ~GrBlobSubRun() = default;
    // Given an already cached subRun, can this subRun handle this combination paint, matrix, and
    // position.
    virtual bool canReuse(const SkPaint& paint, const SkMatrix& positionMatrix) const = 0;

    // Return the underlying atlas SubRun if it exists. Otherwise, return nullptr.
    // * Don't use this API. It is only to support testing.
    virtual const GrAtlasSubRun* testingOnly_atlasSubRun() const = 0;
};

// -- GrSubRun -------------------------------------------------------------------------------------
GrSubRun::~GrSubRun() = default;
const GrBlobSubRun* GrSubRun::blobCast() const {
    SK_ABORT("This is not a subclass of GrBlobSubRun.");
}

namespace {
// -- TransformedMaskVertexFiller ------------------------------------------------------------------
class TransformedMaskVertexFiller {
public:
    TransformedMaskVertexFiller(GrMaskFormat maskFormat,
                                int dstPadding,
                                SkScalar strikeToSourceScale);

    struct PositionAndExtent {
        const SkPoint pos;
        // The rectangle of the glyphs in strike space. But, for kDirectMask this also implies a
        // device space rect.
        GrIRect16 rect;
    };

    size_t vertexStride(const SkMatrix& matrix) const {
        if (fMaskType != kARGB_GrMaskFormat) {
            // For formats kA565_GrMaskFormat and kA8_GrMaskFormat where A8 include SDFT.
            return matrix.hasPerspective() ? sizeof(Mask3DVertex) : sizeof(Mask2DVertex);
        } else {
            // For format kARGB_GrMaskFormat
            return matrix.hasPerspective() ? sizeof(ARGB3DVertex) : sizeof(ARGB2DVertex);
        }
    }

    void fillVertexData(SkSpan<const GrGlyph*> glyphs,
                        SkSpan<const PositionAndExtent> positioning,
                        GrColor color,
                        const SkMatrix& positionMatrix,
                        SkIRect clip,
                        void* vertexBuffer) const;

    AtlasTextOp::MaskType opMaskType() const;
    GrMaskFormat grMaskType() const {return fMaskType;}

private:
    struct AtlasPt {
        uint16_t u;
        uint16_t v;
    };

    // Normal text mask, SDFT, or color.
    struct Mask2DVertex {
        SkPoint devicePos;
        GrColor color;
        AtlasPt atlasPos;
    };

    struct ARGB2DVertex {
        ARGB2DVertex(SkPoint d, GrColor, AtlasPt a) : devicePos{d}, atlasPos{a} {}

        SkPoint devicePos;
        AtlasPt atlasPos;
    };

    // Perspective SDFT or SDFT forced to 3D or perspective color.
    struct Mask3DVertex {
        SkPoint3 devicePos;
        GrColor color;
        AtlasPt atlasPos;
    };

    struct ARGB3DVertex {
        ARGB3DVertex(SkPoint3 d, GrColor, AtlasPt a) : devicePos{d}, atlasPos{a} {}

        SkPoint3 devicePos;
        AtlasPt atlasPos;
    };

    std::array<SkScalar, 4> sourceRect(PositionAndExtent positionAndExtent) const;

    template<typename Quad, typename VertexData>
    void fill2D(SkZip<Quad, const GrGlyph*, const VertexData> quadData,
                GrColor color,
                const SkMatrix& matrix) const;

    template<typename Quad, typename VertexData>
    void fill3D(SkZip<Quad, const GrGlyph*, const VertexData> quadData,
                GrColor color,
                const SkMatrix& matrix) const;

    const GrMaskFormat fMaskType;
    const SkPoint fPaddingInset;
    const SkScalar fStrikeToSourceScale;
};

TransformedMaskVertexFiller::TransformedMaskVertexFiller(GrMaskFormat maskFormat,
                                                         int dstPadding,
                                                         SkScalar strikeToSourceScale)
        : fMaskType{maskFormat}
        , fPaddingInset{SkPoint::Make(dstPadding, dstPadding)}
        , fStrikeToSourceScale{strikeToSourceScale} {}

void TransformedMaskVertexFiller::fillVertexData(SkSpan<const GrGlyph*> glyphs,
                                                 SkSpan<const PositionAndExtent> positioning,
                                                 GrColor color,
                                                 const SkMatrix& positionMatrix,
                                                 SkIRect clip,
                                                 void* vertexBuffer) const {
    auto quadData = [&](auto dst) {
        return SkMakeZip(dst, glyphs, positioning);
    };

    if (!positionMatrix.hasPerspective()) {
        if (fMaskType == GrMaskFormat::kARGB_GrMaskFormat) {
            using Quad = ARGB2DVertex[4];
            SkASSERT(sizeof(ARGB2DVertex) == this->vertexStride(positionMatrix));
            this->fill2D(quadData((Quad*) vertexBuffer), color, positionMatrix);
        } else {
            using Quad = Mask2DVertex[4];
            SkASSERT(sizeof(Mask2DVertex) == this->vertexStride(positionMatrix));
            this->fill2D(quadData((Quad*) vertexBuffer), color, positionMatrix);
        }
    } else {
        if (fMaskType == GrMaskFormat::kARGB_GrMaskFormat) {
            using Quad = ARGB3DVertex[4];
            SkASSERT(sizeof(ARGB3DVertex) == this->vertexStride(positionMatrix));
            this->fill3D(quadData((Quad*) vertexBuffer), color, positionMatrix);
        } else {
            using Quad = Mask3DVertex[4];
            SkASSERT(sizeof(Mask3DVertex) == this->vertexStride(positionMatrix));
            this->fill3D(quadData((Quad*) vertexBuffer), color, positionMatrix);
        }
    }
}

std::array<SkScalar, 4>
TransformedMaskVertexFiller::sourceRect(PositionAndExtent positionAndExtent) const {
    auto[pos, rect] = positionAndExtent;
    auto[l, t, r, b] = rect;
    SkPoint LT = (SkPoint::Make(l, t) + fPaddingInset) * fStrikeToSourceScale + pos,
            RB = (SkPoint::Make(r, b) - fPaddingInset) * fStrikeToSourceScale + pos;
    return {LT.x(), LT.y(), RB.x(), RB.y()};
}

template<typename Quad, typename VertexData>
void TransformedMaskVertexFiller::fill2D(SkZip<Quad, const GrGlyph*, const VertexData> quadData,
                                         GrColor color,
                                         const SkMatrix& positionMatrix) const {
    for (auto[quad, glyph, positionAndExtent] : quadData) {
        auto [l, t, r, b] = this->sourceRect(positionAndExtent);
        SkPoint lt = positionMatrix.mapXY(l, t),
                lb = positionMatrix.mapXY(l, b),
                rt = positionMatrix.mapXY(r, t),
                rb = positionMatrix.mapXY(r, b);
        auto[al, at, ar, ab] = glyph->fAtlasLocator.getUVs();
        quad[0] = {lt, color, {al, at}};  // L,T
        quad[1] = {lb, color, {al, ab}};  // L,B
        quad[2] = {rt, color, {ar, at}};  // R,T
        quad[3] = {rb, color, {ar, ab}};  // R,B
    }
}

template<typename Quad, typename VertexData>
void TransformedMaskVertexFiller::fill3D(SkZip<Quad, const GrGlyph*, const VertexData> quadData,
                                         GrColor color,
                                         const SkMatrix& positionMatrix) const {
    auto mapXYZ = [&](SkScalar x, SkScalar y) {
        SkPoint pt{x, y};
        SkPoint3 result;
        positionMatrix.mapHomogeneousPoints(&result, &pt, 1);
        return result;
    };
    for (auto[quad, glyph, positionAndExtent] : quadData) {
        auto [l, t, r, b] = this->sourceRect(positionAndExtent);
        SkPoint3 lt = mapXYZ(l, t),
                 lb = mapXYZ(l, b),
                 rt = mapXYZ(r, t),
                 rb = mapXYZ(r, b);
        auto[al, at, ar, ab] = glyph->fAtlasLocator.getUVs();
        quad[0] = {lt, color, {al, at}};  // L,T
        quad[1] = {lb, color, {al, ab}};  // L,B
        quad[2] = {rt, color, {ar, at}};  // R,T
        quad[3] = {rb, color, {ar, ab}};  // R,B
    }
}

AtlasTextOp::MaskType TransformedMaskVertexFiller::opMaskType() const {
    switch (fMaskType) {
        case kA8_GrMaskFormat:   return AtlasTextOp::MaskType::kGrayscaleCoverage;
        case kA565_GrMaskFormat: return AtlasTextOp::MaskType::kLCDCoverage;
        case kARGB_GrMaskFormat: return AtlasTextOp::MaskType::kColorBitmap;
    }
    SkUNREACHABLE;
}

struct AtlasPt {
    uint16_t u;
    uint16_t v;
};

// Normal text mask, SDFT, or color.
struct Mask2DVertex {
    SkPoint devicePos;
    GrColor color;
    AtlasPt atlasPos;
};

struct ARGB2DVertex {
    ARGB2DVertex(SkPoint d, GrColor, AtlasPt a) : devicePos{d}, atlasPos{a} {}

    SkPoint devicePos;
    AtlasPt atlasPos;
};

// Perspective SDFT or SDFT forced to 3D or perspective color.
struct Mask3DVertex {
    SkPoint3 devicePos;
    GrColor color;
    AtlasPt atlasPos;
};

struct ARGB3DVertex {
    ARGB3DVertex(SkPoint3 d, GrColor, AtlasPt a) : devicePos{d}, atlasPos{a} {}

    SkPoint3 devicePos;
    AtlasPt atlasPos;
};

AtlasTextOp::MaskType op_mask_type(GrMaskFormat grMaskFormat) {
    switch (grMaskFormat) {
        case kA8_GrMaskFormat:   return AtlasTextOp::MaskType::kGrayscaleCoverage;
        case kA565_GrMaskFormat: return AtlasTextOp::MaskType::kLCDCoverage;
        case kARGB_GrMaskFormat: return AtlasTextOp::MaskType::kColorBitmap;
    }
    SkUNREACHABLE;
}

SkMatrix position_matrix(const SkMatrix& drawMatrix, SkPoint drawOrigin) {
    SkMatrix position_matrix = drawMatrix;
    return position_matrix.preTranslate(drawOrigin.x(), drawOrigin.y());
}

SkPMColor4f calculate_colors(skgpu::SurfaceContext* sc,
                             const SkPaint& paint,
                             const SkMatrixProvider& matrix,
                             GrMaskFormat grMaskFormat,
                             GrPaint* grPaint) {
    GrRecordingContext* rContext = sc->recordingContext();
    const GrColorInfo& colorInfo = sc->colorInfo();
    if (grMaskFormat == kARGB_GrMaskFormat) {
        SkPaintToGrPaintReplaceShader(rContext, colorInfo, paint, matrix, nullptr, grPaint);
        float a = grPaint->getColor4f().fA;
        return {a, a, a, a};
    }
    SkPaintToGrPaint(rContext, colorInfo, paint, matrix, grPaint);
    return grPaint->getColor4f();
}

template<typename Quad, typename VertexData>
void fill_transformed_vertices_2D(SkZip<Quad, const GrGlyph*, const VertexData> quadData,
                                  SkScalar dstPadding,
                                  SkScalar strikeToSource,
                                  GrColor color,
                                  const SkMatrix& matrix) {
    SkPoint inset = {dstPadding, dstPadding};
    for (auto[quad, glyph, vertexData] : quadData) {
        auto[pos, rect] = vertexData;
        auto[l, t, r, b] = rect;
        SkPoint sLT = (SkPoint::Make(l, t) + inset) * strikeToSource + pos,
                sRB = (SkPoint::Make(r, b) - inset) * strikeToSource + pos;
        SkPoint lt = matrix.mapXY(sLT.x(), sLT.y()),
                lb = matrix.mapXY(sLT.x(), sRB.y()),
                rt = matrix.mapXY(sRB.x(), sLT.y()),
                rb = matrix.mapXY(sRB.x(), sRB.y());
        auto[al, at, ar, ab] = glyph->fAtlasLocator.getUVs();
        quad[0] = {lt, color, {al, at}};  // L,T
        quad[1] = {lb, color, {al, ab}};  // L,B
        quad[2] = {rt, color, {ar, at}};  // R,T
        quad[3] = {rb, color, {ar, ab}};  // R,B
    }
}

// Check for integer translate with the same 2x2 matrix.
// Returns the translation, and true if the change from initial matrix to the position matrix
// support using direct glyph masks.
std::tuple<bool, SkVector> can_use_direct(
        const SkMatrix& initialPositionMatrix, const SkMatrix& positionMatrix) {
    // The existing direct glyph info can be used if the initialPositionMatrix, and the
    // positionMatrix have the same 2x2, and the translation between them is integer.
    // Calculate the translation in source space to a translation in device space by mapping
    // (0, 0) through both the initial position matrix and the position matrix; take the difference.
    SkVector translation = positionMatrix.mapOrigin() - initialPositionMatrix.mapOrigin();
    return {initialPositionMatrix.getScaleX() == positionMatrix.getScaleX() &&
            initialPositionMatrix.getScaleY() == positionMatrix.getScaleY() &&
            initialPositionMatrix.getSkewX()  == positionMatrix.getSkewX()  &&
            initialPositionMatrix.getSkewY()  == positionMatrix.getSkewY()  &&
            SkScalarIsInt(translation.x()) && SkScalarIsInt(translation.y()),
            translation};
}

// -- PathOpSubmitter ------------------------------------------------------------------------------
// Shared code for submitting GPU ops for drawing glyphs as paths.
class PathOpSubmitter {
    struct PathAndPosition;
public:
    PathOpSubmitter(bool isAntiAliased,
                    SkScalar strikeToSourceScale,
                    SkSpan<PathAndPosition> paths,
                    std::unique_ptr<PathAndPosition[], GrSubRunAllocator::ArrayDestroyer> pathData);

    PathOpSubmitter(PathOpSubmitter&& that);

    static PathOpSubmitter Make(const SkZip<SkGlyphVariant, SkPoint>& accepted,
                                bool isAntiAliased,
                                SkScalar strikeToSourceScale,
                                GrSubRunAllocator* alloc);

    void submitOps(SkCanvas*,
                   const GrClip* clip,
                   const SkMatrixProvider& viewMatrix,
                   SkPoint drawOrigin,
                   const SkPaint& paint,
                   skgpu::v1::SurfaceDrawContext* sdc) const;

private:
    struct PathAndPosition {
        SkPath fPath;
        SkPoint fPosition;
    };
    const bool fIsAntiAliased;
    const SkScalar fStrikeToSourceScale;
    const SkSpan<const PathAndPosition> fPaths;
    std::unique_ptr<PathAndPosition[], GrSubRunAllocator::ArrayDestroyer> fPathData;
};

PathOpSubmitter::PathOpSubmitter(
        bool isAntiAliased,
        SkScalar strikeToSourceScale,
        SkSpan<PathAndPosition> paths,
        std::unique_ptr<PathAndPosition[], GrSubRunAllocator::ArrayDestroyer> pathData)
            : fIsAntiAliased{isAntiAliased}
            , fStrikeToSourceScale{strikeToSourceScale}
            , fPaths{paths}
            , fPathData{std::move(pathData)} {
    SkASSERT(!fPaths.empty());
}

PathOpSubmitter::PathOpSubmitter(PathOpSubmitter&& that)
    : fIsAntiAliased{that.fIsAntiAliased}
    , fStrikeToSourceScale{that.fStrikeToSourceScale}
    , fPaths{that.fPaths}
    , fPathData{std::move(that.fPathData)} {}

PathOpSubmitter PathOpSubmitter::Make(const SkZip<SkGlyphVariant, SkPoint>& accepted,
                                      bool isAntiAliased,
                                      SkScalar strikeToSourceScale,
                                      GrSubRunAllocator* alloc) {
    auto pathData = alloc->makeUniqueArray<PathAndPosition>(
            accepted.size(),
            [&](int i){
                auto [variant, pos] = accepted[i];
                return PathAndPosition{*variant.path(), pos};
            });
    SkSpan<PathAndPosition> paths{pathData.get(), accepted.size()};

    return PathOpSubmitter{isAntiAliased, strikeToSourceScale, paths, std::move(pathData)};
}

void PathOpSubmitter::submitOps(SkCanvas* canvas,
                                const GrClip* clip,
                                const SkMatrixProvider& viewMatrix,
                                SkPoint drawOrigin,
                                const SkPaint& paint,
                                skgpu::v1::SurfaceDrawContext* sdc) const {
    SkPaint runPaint{paint};
    runPaint.setAntiAlias(fIsAntiAliased);
    // If there are shaders, blurs or styles, the path must be scaled into source
    // space independently of the CTM. This allows the CTM to be correct for the
    // different effects.
    GrStyle style(runPaint);

    bool needsExactCTM = runPaint.getShader()
                         || style.applies()
                         || runPaint.getMaskFilter();

    // Calculate the matrix that maps the path glyphs from their size in the strike to
    // the graphics source space.
    SkMatrix strikeToSource = SkMatrix::Scale(fStrikeToSourceScale, fStrikeToSourceScale);
    strikeToSource.postTranslate(drawOrigin.x(), drawOrigin.y());
    if (!needsExactCTM) {
        for (const auto& pathPos : fPaths) {
            const SkPath& path = pathPos.fPath;
            const SkPoint pos = pathPos.fPosition;
            // Transform the glyph to source space.
            SkMatrix pathMatrix = strikeToSource;
            pathMatrix.postTranslate(pos.x(), pos.y());

            SkAutoCanvasRestore acr(canvas, true);
            canvas->concat(pathMatrix);
            canvas->drawPath(path, runPaint);
        }
    } else {
        // Transform the path to device because the deviceMatrix must be unchanged to
        // draw effect, filter or shader paths.
        for (const auto& pathPos : fPaths) {
            const SkPath& path = pathPos.fPath;
            const SkPoint pos = pathPos.fPosition;
            // Transform the glyph to source space.
            SkMatrix pathMatrix = strikeToSource;
            pathMatrix.postTranslate(pos.x(), pos.y());

            SkPath deviceOutline;
            path.transform(pathMatrix, &deviceOutline);
            deviceOutline.setIsVolatile(true);
            canvas->drawPath(deviceOutline, runPaint);
        }
    }
}

// -- PathSubRun -----------------------------------------------------------------------------------
class PathSubRun final : public GrSubRun, public GrBlobSubRun {
public:
    PathSubRun(PathOpSubmitter&& pathDrawing) : fPathDrawing(std::move(pathDrawing)) {}

    static GrSubRunOwner Make(const SkZip<SkGlyphVariant, SkPoint>& accepted,
                              bool isAntiAliased,
                              SkScalar strikeToSourceScale,
                              GrSubRunAllocator* alloc) {
        return alloc->makeUnique<PathSubRun>(
                PathOpSubmitter::Make(accepted, isAntiAliased, strikeToSourceScale, alloc));
    }

    void draw(SkCanvas* canvas,
              const GrClip* clip,
              const SkMatrixProvider& viewMatrix,
              SkPoint drawOrigin,
              const SkPaint& paint,
              skgpu::v1::SurfaceDrawContext* sdc) const override {
        fPathDrawing.submitOps(canvas, clip, viewMatrix, drawOrigin, paint, sdc);
    }

    const GrBlobSubRun* blobCast() const override { return this; }
    int unflattenSize() const override { return 0; }

    bool canReuse(const SkPaint& paint, const SkMatrix& positionMatrix) const override {
        return true;
    }
    const GrAtlasSubRun* testingOnly_atlasSubRun() const override { return nullptr; }
    static GrSubRunOwner MakeFromBuffer(const GrTextReferenceFrame* referenceFrame,
                                        SkReadBuffer& buffer,
                                        GrSubRunAllocator* alloc,
                                        const SkStrikeClient* client) {
        return nullptr;
    }

protected:
    SubRunType subRunType() const override { return kPath; }
    void doFlatten(SkWriteBuffer& buffer) const override {
        SK_ABORT("Not implemented.");
    }

private:
    PathOpSubmitter fPathDrawing;
};

// -- DrawableOpSubmitter --------------------------------------------------------------------------
// Shared code for submitting GPU ops for drawing glyphs as drawables.
class DrawableOpSubmitter {
    struct DrawableAndPosition;
public:
    DrawableOpSubmitter(bool isAntiAliased,
                        SkScalar strikeToSourceScale,
                        SkSpan<DrawableAndPosition> drawables,
                        std::unique_ptr<DrawableAndPosition[],
                                        GrSubRunAllocator::ArrayDestroyer> drawableData);

    DrawableOpSubmitter(DrawableOpSubmitter&& that);

    static DrawableOpSubmitter Make(const SkZip<SkGlyphVariant, SkPoint>& accepted,
                                    bool isAntiAliased,
                                    SkScalar strikeToSourceScale,
                                    GrSubRunAllocator* alloc);

    void submitOps(SkCanvas*,
                   const GrClip* clip,
                   const SkMatrixProvider& viewMatrix,
                   SkPoint drawOrigin,
                   const SkPaint& paint,
                   skgpu::v1::SurfaceDrawContext* sdc) const;

private:
    struct DrawableAndPosition {
        sk_sp<SkDrawable> fDrawable;
        SkPoint fPosition;
    };
    const bool fIsAntiAliased;
    const SkScalar fStrikeToSourceScale;
    const SkSpan<const DrawableAndPosition> fDrawables;
    std::unique_ptr<DrawableAndPosition[], GrSubRunAllocator::ArrayDestroyer> fDrawableData;
};

DrawableOpSubmitter::DrawableOpSubmitter(
        bool isAntiAliased,
        SkScalar strikeToSourceScale,
        SkSpan<DrawableAndPosition> drawables,
        std::unique_ptr<DrawableAndPosition[], GrSubRunAllocator::ArrayDestroyer> drawableData)
            : fIsAntiAliased{isAntiAliased}
            , fStrikeToSourceScale{strikeToSourceScale}
            , fDrawables{drawables}
            , fDrawableData{std::move(drawableData)} {
    SkASSERT(!fDrawables.empty());
}

DrawableOpSubmitter::DrawableOpSubmitter(DrawableOpSubmitter&& that)
    : fIsAntiAliased{that.fIsAntiAliased}
    , fStrikeToSourceScale{that.fStrikeToSourceScale}
    , fDrawables{that.fDrawables}
    , fDrawableData{std::move(that.fDrawableData)} {}

DrawableOpSubmitter DrawableOpSubmitter::Make(const SkZip<SkGlyphVariant, SkPoint>& accepted,
                                              bool isAntiAliased,
                                              SkScalar strikeToSourceScale,
                                              GrSubRunAllocator* alloc) {
    auto drawableData = alloc->makeUniqueArray<DrawableAndPosition>(
            accepted.size(),
            [&](int i){
                auto [variant, pos] = accepted[i];
                return DrawableAndPosition{sk_ref_sp(variant.drawable()), pos};
            });
    SkSpan<DrawableAndPosition> drawables{drawableData.get(), accepted.size()};

    return DrawableOpSubmitter{isAntiAliased, strikeToSourceScale,
                               drawables, std::move(drawableData)};
}

void DrawableOpSubmitter::submitOps(SkCanvas* canvas,
                                    const GrClip* clip,
                                    const SkMatrixProvider& viewMatrix,
                                    SkPoint drawOrigin,
                                    const SkPaint& paint,
                                    skgpu::v1::SurfaceDrawContext* sdc) const {
    // Calculate the matrix that maps the path glyphs from their size in the strike to
    // the graphics source space.
    SkMatrix strikeToSource = SkMatrix::Scale(fStrikeToSourceScale, fStrikeToSourceScale);
    strikeToSource.postTranslate(drawOrigin.x(), drawOrigin.y());

    // Transform the path to device because the deviceMatrix must be unchanged to
    // draw effect, filter or shader paths.
    for (const auto& pathPos : fDrawables) {
        const sk_sp<SkDrawable>& drawable = pathPos.fDrawable;
        const SkPoint pos = pathPos.fPosition;
        // Transform the glyph to source space.
        SkMatrix pathMatrix = strikeToSource;
        pathMatrix.postTranslate(pos.x(), pos.y());

        SkAutoCanvasRestore acr(canvas, false);
        SkRect drawableBounds = drawable->getBounds();
        pathMatrix.mapRect(&drawableBounds);
        canvas->saveLayer(&drawableBounds, &paint);
        drawable->draw(canvas, &pathMatrix);
    }
}

template <typename SubRun>
GrSubRunOwner make_drawable_sub_run(const SkZip<SkGlyphVariant, SkPoint>& drawables,
                                   bool isAntiAliased,
                                   SkScalar strikeToSourceScale,
                                   GrSubRunAllocator* alloc) {
    return alloc->makeUnique<SubRun>(
            DrawableOpSubmitter::Make(drawables, isAntiAliased, strikeToSourceScale, alloc));
}

// -- DrawableSubRunSlug ---------------------------------------------------------------------------
class DrawableSubRunSlug : public GrSubRun {
public:
    DrawableSubRunSlug(DrawableOpSubmitter&& drawingDrawing)
            : fDrawingDrawing(std::move(drawingDrawing)) {}

    static GrSubRunOwner MakeFromBuffer(const GrTextReferenceFrame* referenceFrame,
                                        SkReadBuffer& buffer,
                                        GrSubRunAllocator* alloc,
                                        const SkStrikeClient* client) {
        return nullptr;
    }

    void draw(SkCanvas* canvas,
              const GrClip* clip,
              const SkMatrixProvider& viewMatrix,
              SkPoint drawOrigin,
              const SkPaint& paint,
              skgpu::v1::SurfaceDrawContext* sdc) const override {
        fDrawingDrawing.submitOps(canvas, clip, viewMatrix, drawOrigin, paint, sdc);
    }

    int unflattenSize() const override { return 0; }

protected:
    SubRunType subRunType() const override { return kDrawable; }
    void doFlatten(SkWriteBuffer& buffer) const override {
        SK_ABORT("Not implemented.");
    }

private:
    DrawableOpSubmitter fDrawingDrawing;
};

// -- DrawableSubRun -------------------------------------------------------------------------------
class DrawableSubRun final : public DrawableSubRunSlug, public GrBlobSubRun {
public:
    using DrawableSubRunSlug::DrawableSubRunSlug;
    const GrBlobSubRun* blobCast() const override { return this; }
    int unflattenSize() const override { return 0; }

    bool canReuse(const SkPaint& paint, const SkMatrix& positionMatrix) const override {
        return true;
    }
    const GrAtlasSubRun* testingOnly_atlasSubRun() const override { return nullptr; }
};

// -- DirectMaskSubRun -----------------------------------------------------------------------------
class DirectMaskSubRun final : public GrSubRun, public GrBlobSubRun, public GrAtlasSubRun {
public:
    using DevicePosition = skvx::Vec<2, int16_t>;

    DirectMaskSubRun(const GrTextReferenceFrame* referenceFrame,
                     GrMaskFormat format,
                     const SkGlyphRect& deviceBounds,
                     SkSpan<const DevicePosition> devicePositions,
                     GrGlyphVector&& glyphs,
                     bool glyphsOutOfBounds,
                     bool supportBilerpAtlas);

    static GrSubRunOwner Make(const GrTextBlob* blob,
                              const SkZip<SkGlyphVariant, SkPoint>& accepted,
                              sk_sp<SkStrike>&& strike,
                              GrMaskFormat format,
                              GrSubRunAllocator* alloc);

    void draw(SkCanvas*,
              const GrClip*,
              const SkMatrixProvider& viewMatrix,
              SkPoint drawOrigin,
              const SkPaint& paint,
              skgpu::v1::SurfaceDrawContext*) const override;

    std::tuple<const GrClip*, GrOp::Owner>
    makeAtlasTextOp(const GrClip* clip,
                    const SkMatrixProvider& viewMatrix,
                    SkPoint drawOrigin,
                    const SkPaint& paint,
                    skgpu::v1::SurfaceDrawContext* sdc,
                    GrAtlasSubRunOwner) const override;

    const GrBlobSubRun* blobCast() const override { return this; }
    int unflattenSize() const override { return 0; }

    bool canReuse(const SkPaint& paint, const SkMatrix& positionMatrix) const override;

    const GrAtlasSubRun* testingOnly_atlasSubRun() const override;

    size_t vertexStride(const SkMatrix& drawMatrix) const override;

    int glyphCount() const override;

    void testingOnly_packedGlyphIDToGrGlyph(GrStrikeCache *cache) const override;

    std::tuple<bool, int>
    regenerateAtlas(int begin, int end, GrMeshDrawTarget*) const override;

    void fillVertexData(void* vertexDst, int offset, int count,
                        GrColor color,
                        const SkMatrix& drawMatrix, SkPoint drawOrigin,
                        SkIRect clip) const override;

protected:
    SubRunType subRunType() const override { return kDirectMask; }
    void doFlatten(SkWriteBuffer& buffer) const override {
        SK_ABORT("Not implemented.");
    }

private:
    // The rectangle that surrounds all the glyph bounding boxes in device space.
    SkRect deviceRect(const SkMatrix& drawMatrix, SkPoint drawOrigin) const;

    const GrTextReferenceFrame* const fTextReferenceFrame;
    const GrMaskFormat fMaskFormat;

    // The union of all the glyph bounds in device space.
    const SkGlyphRect fGlyphDeviceBounds;
    const SkSpan<const DevicePosition> fLeftTopDevicePos;
    const bool fSomeGlyphsExcluded;
    const bool fSupportBilerpAtlas;

    // The regenerateAtlas method mutates fGlyphs. It should be called from onPrepare which must
    // be single threaded.
    mutable GrGlyphVector fGlyphs;
};

DirectMaskSubRun::DirectMaskSubRun(const GrTextReferenceFrame* referenceFrame,
                                   GrMaskFormat format,
                                   const SkGlyphRect& deviceBounds,
                                   SkSpan<const DevicePosition> devicePositions,
                                   GrGlyphVector&& glyphs,
                                   bool glyphsOutOfBounds,
                                   bool supportBilerpAtlas)
        : fTextReferenceFrame{referenceFrame}
        , fMaskFormat{format}
        , fGlyphDeviceBounds{deviceBounds}
        , fLeftTopDevicePos{devicePositions}
        , fSomeGlyphsExcluded{glyphsOutOfBounds}
        , fSupportBilerpAtlas{supportBilerpAtlas}
        , fGlyphs{std::move(glyphs)} {}

GrSubRunOwner DirectMaskSubRun::Make(const GrTextBlob* blob,
                                     const SkZip<SkGlyphVariant, SkPoint>& accepted,
                                     sk_sp<SkStrike>&& strike,
                                     GrMaskFormat format,
                                     GrSubRunAllocator* alloc) {
    auto glyphLeftTop = alloc->makePODArray<DevicePosition>(accepted.size());
    auto glyphIDs = alloc->makePODArray<GrGlyphVector::Variant>(accepted.size());

    // Because this is the direct case, the maximum width or height is the size that fits in the
    // atlas. This boundary is checked below to ensure that the call to SkGlyphRect below will
    // not overflow.
    constexpr SkScalar kMaxPos =
            std::numeric_limits<int16_t>::max() - SkStrikeCommon::kSkSideTooBigForAtlas;
    SkGlyphRect runBounds = skglyph::empty_rect();
    size_t goodPosCount = 0;
    for (auto [variant, pos] : accepted) {
        auto [x, y] = pos;
        // Ensure that the .offset() call below does not overflow. And, at this point none of the
        // rectangles are empty because they were culled before the run was created. Basically,
        // cull all the glyphs that can't appear on the screen.
        if (-kMaxPos < x && x < kMaxPos && -kMaxPos  < y && y < kMaxPos) {
            const SkGlyph* const skGlyph = variant;
            const SkGlyphRect deviceBounds =
                    skGlyph->glyphRect().offset(SkScalarRoundToInt(x), SkScalarRoundToInt(y));
            runBounds = skglyph::rect_union(runBounds, deviceBounds);
            glyphLeftTop[goodPosCount] = deviceBounds.topLeft();
            glyphIDs[goodPosCount].packedGlyphID = skGlyph->getPackedID();
            goodPosCount += 1;
        }
    }

    // Wow! no glyphs are in bounds and had non-empty bounds.
    if (goodPosCount == 0) {
        return nullptr;
    }

    // If some glyphs were excluded by the bounds, then this subrun can't be generally be used
    // for other draws. Mark the subrun as not general.
    bool glyphsExcluded = goodPosCount != accepted.size();
    SkSpan<const DevicePosition> leftTop{glyphLeftTop, goodPosCount};
    return alloc->makeUnique<DirectMaskSubRun>(
            blob, format, runBounds, leftTop,
            GrGlyphVector{std::move(strike), {glyphIDs, goodPosCount}},
            glyphsExcluded,
            blob->supportBilerpAtlas());
}

bool DirectMaskSubRun::canReuse(const SkPaint& paint, const SkMatrix& positionMatrix) const {
    auto [reuse, translation] =
            can_use_direct(fTextReferenceFrame->initialPositionMatrix(), positionMatrix);

    // If glyphs were excluded because of position bounds, then this subrun can only be reused if
    // there is no change in position.
    if (fSomeGlyphsExcluded) {
        return translation.x() == 0 && translation.y() == 0;
    }

    return reuse;
}

size_t DirectMaskSubRun::vertexStride(const SkMatrix&) const {
    if (fMaskFormat != kARGB_GrMaskFormat) {
        return sizeof(Mask2DVertex);
    } else {
        return sizeof(ARGB2DVertex);
    }
}

int DirectMaskSubRun::glyphCount() const {
    return SkCount(fGlyphs.glyphs());
}

void DirectMaskSubRun::draw(SkCanvas*,
                            const GrClip* clip,
                            const SkMatrixProvider& viewMatrix,
                            SkPoint drawOrigin,
                            const SkPaint& paint,
                            skgpu::v1::SurfaceDrawContext* sdc) const{
    auto[drawingClip, op] = this->makeAtlasTextOp(
            clip, viewMatrix, drawOrigin, paint, sdc, nullptr);
    if (op != nullptr) {
        sdc->addDrawOp(drawingClip, std::move(op));
    }
}

namespace {
enum ClipMethod {
    kClippedOut,
    kUnclipped,
    kGPUClipped,
    kGeometryClipped
};

std::tuple<ClipMethod, SkIRect>
calculate_clip(const GrClip* clip, SkRect deviceBounds, SkRect glyphBounds) {
    if (clip == nullptr && !deviceBounds.intersects(glyphBounds)) {
        return {kClippedOut, SkIRect::MakeEmpty()};
    } else if (clip != nullptr) {
        switch (auto result = clip->preApply(glyphBounds, GrAA::kNo); result.fEffect) {
            case GrClip::Effect::kClippedOut:
                return {kClippedOut, SkIRect::MakeEmpty()};
            case GrClip::Effect::kUnclipped:
                return {kUnclipped, SkIRect::MakeEmpty()};
            case GrClip::Effect::kClipped: {
                if (result.fIsRRect && result.fRRect.isRect()) {
                    SkRect r = result.fRRect.rect();
                    if (result.fAA == GrAA::kNo || GrClip::IsPixelAligned(r)) {
                        SkIRect clipRect = SkIRect::MakeEmpty();
                        // Clip geometrically during onPrepare using clipRect.
                        r.round(&clipRect);
                        if (clipRect.contains(glyphBounds)) {
                            // If fully within the clip, signal no clipping using the empty rect.
                            return {kUnclipped, SkIRect::MakeEmpty()};
                        }
                        // Use the clipRect to clip the geometry.
                        return {kGeometryClipped, clipRect};
                    }
                    // Partial pixel clipped at this point. Have the GPU handle it.
                }
            }
            break;
        }
    }
    return {kGPUClipped, SkIRect::MakeEmpty()};
}
}  // namespace

std::tuple<const GrClip*, GrOp::Owner>
DirectMaskSubRun::makeAtlasTextOp(const GrClip* clip,
                                  const SkMatrixProvider& viewMatrix,
                                  SkPoint drawOrigin,
                                  const SkPaint& paint,
                                  skgpu::v1::SurfaceDrawContext* sdc,
                                  GrAtlasSubRunOwner) const {
    SkASSERT(this->glyphCount() != 0);

    const SkMatrix& drawMatrix = viewMatrix.localToDevice();

    // We can clip geometrically using clipRect and ignore clip when an axis-aligned rectangular
    // non-AA clip is used. If clipRect is empty, and clip is nullptr, then there is no clipping
    // needed.
    const SkRect subRunBounds = this->deviceRect(drawMatrix, drawOrigin);
    const SkRect deviceBounds = SkRect::MakeWH(sdc->width(), sdc->height());
    auto [clipMethod, clipRect] = calculate_clip(clip, deviceBounds, subRunBounds);

    switch (clipMethod) {
        case kClippedOut:
            // Returning nullptr as op means skip this op.
            return {nullptr, nullptr};
        case kUnclipped:
        case kGeometryClipped:
            // GPU clip is not needed.
            clip = nullptr;
            break;
        case kGPUClipped:
            // Use the GPU clip; clipRect is ignored.
            break;
    }

    if (!clipRect.isEmpty()) { SkASSERT(clip == nullptr); }

    GrPaint grPaint;
    const SkPMColor4f drawingColor =
            calculate_colors(sdc, paint, viewMatrix, fMaskFormat, &grPaint);

    auto geometry = AtlasTextOp::Geometry::MakeForBlob(*this,
                                                       drawMatrix,
                                                       drawOrigin,
                                                       clipRect,
                                                       sk_ref_sp(fTextReferenceFrame),
                                                       drawingColor,
                                                       sdc->arenaAlloc());

    GrRecordingContext* const rContext = sdc->recordingContext();
    GrOp::Owner op = GrOp::Make<AtlasTextOp>(rContext,
                                             op_mask_type(fMaskFormat),
                                             false,
                                             this->glyphCount(),
                                             subRunBounds,
                                             geometry,
                                             std::move(grPaint));

    return {clip, std::move(op)};
}

void DirectMaskSubRun::testingOnly_packedGlyphIDToGrGlyph(GrStrikeCache *cache) const {
    fGlyphs.packedGlyphIDToGrGlyph(cache);
}

std::tuple<bool, int>
DirectMaskSubRun::regenerateAtlas(int begin, int end, GrMeshDrawTarget* target) const {
    int srcPadding = fSupportBilerpAtlas ? 1 : 0;
    return fGlyphs.regenerateAtlas(
            begin, end, fMaskFormat, srcPadding, target, fSupportBilerpAtlas);
}

// The 99% case. No clip. Non-color only.
void direct_2D(SkZip<Mask2DVertex[4],
               const GrGlyph*,
               const DirectMaskSubRun::DevicePosition> quadData,
               GrColor color,
               SkPoint originOffset) {
    for (auto[quad, glyph, leftTop] : quadData) {
        auto[al, at, ar, ab] = glyph->fAtlasLocator.getUVs();
        SkScalar dl = leftTop[0] + originOffset.x(),
                 dt = leftTop[1] + originOffset.y(),
                 dr = dl + (ar - al),
                 db = dt + (ab - at);

        quad[0] = {{dl, dt}, color, {al, at}};  // L,T
        quad[1] = {{dl, db}, color, {al, ab}};  // L,B
        quad[2] = {{dr, dt}, color, {ar, at}};  // R,T
        quad[3] = {{dr, db}, color, {ar, ab}};  // R,B
    }
}

template <typename Rect>
auto ltbr(const Rect& r) {
    return std::make_tuple(r.left(), r.top(), r.right(), r.bottom());
}

// Handle any combination of BW or color and clip or no clip.
template<typename Quad, typename VertexData>
void generalized_direct_2D(SkZip<Quad, const GrGlyph*, const VertexData> quadData,
                           GrColor color,
                           SkPoint originOffset,
                           SkIRect* clip = nullptr) {
    for (auto[quad, glyph, leftTop] : quadData) {
        auto[al, at, ar, ab] = glyph->fAtlasLocator.getUVs();
        uint16_t w = ar - al,
                 h = ab - at;
        SkScalar l = (SkScalar)leftTop[0] + originOffset.x(),
                 t = (SkScalar)leftTop[1] + originOffset.y();
        if (clip == nullptr) {
            auto[dl, dt, dr, db] = SkRect::MakeLTRB(l, t, l + w, t + h);
            quad[0] = {{dl, dt}, color, {al, at}};  // L,T
            quad[1] = {{dl, db}, color, {al, ab}};  // L,B
            quad[2] = {{dr, dt}, color, {ar, at}};  // R,T
            quad[3] = {{dr, db}, color, {ar, ab}};  // R,B
        } else {
            SkIRect devIRect = SkIRect::MakeLTRB(l, t, l + w, t + h);
            SkScalar dl, dt, dr, db;
            if (!clip->containsNoEmptyCheck(devIRect)) {
                if (SkIRect clipped; clipped.intersect(devIRect, *clip)) {
                    al += clipped.left()   - devIRect.left();
                    at += clipped.top()    - devIRect.top();
                    ar += clipped.right()  - devIRect.right();
                    ab += clipped.bottom() - devIRect.bottom();
                    std::tie(dl, dt, dr, db) = ltbr(clipped);
                } else {
                    // TODO: omit generating any vertex data for fully clipped glyphs ?
                    std::tie(dl, dt, dr, db) = std::make_tuple(0, 0, 0, 0);
                    std::tie(al, at, ar, ab) = std::make_tuple(0, 0, 0, 0);
                }
            } else {
                std::tie(dl, dt, dr, db) = ltbr(devIRect);
            }
            quad[0] = {{dl, dt}, color, {al, at}};  // L,T
            quad[1] = {{dl, db}, color, {al, ab}};  // L,B
            quad[2] = {{dr, dt}, color, {ar, at}};  // R,T
            quad[3] = {{dr, db}, color, {ar, ab}};  // R,B
        }
    }
}

void DirectMaskSubRun::fillVertexData(void* vertexDst, int offset, int count,
                                      GrColor color,
                                      const SkMatrix& drawMatrix, SkPoint drawOrigin,
                                      SkIRect clip) const {
    const SkMatrix positionMatrix = position_matrix(drawMatrix, drawOrigin);
    auto quadData = [&](auto dst) {
        return SkMakeZip(dst,
                         fGlyphs.glyphs().subspan(offset, count),
                         fLeftTopDevicePos.subspan(offset, count));
    };

    SkPoint originOffset =
            positionMatrix.mapOrigin() - fTextReferenceFrame->initialPositionMatrix().mapOrigin();

    if (clip.isEmpty()) {
        if (fMaskFormat != kARGB_GrMaskFormat) {
            using Quad = Mask2DVertex[4];
            SkASSERT(sizeof(Mask2DVertex) == this->vertexStride(positionMatrix));
            direct_2D(quadData((Quad*)vertexDst), color, originOffset);
        } else {
            using Quad = ARGB2DVertex[4];
            SkASSERT(sizeof(ARGB2DVertex) == this->vertexStride(positionMatrix));
            generalized_direct_2D(quadData((Quad*)vertexDst), color, originOffset);
        }
    } else {
        if (fMaskFormat != kARGB_GrMaskFormat) {
            using Quad = Mask2DVertex[4];
            SkASSERT(sizeof(Mask2DVertex) == this->vertexStride(positionMatrix));
            generalized_direct_2D(quadData((Quad*)vertexDst), color, originOffset, &clip);
        } else {
            using Quad = ARGB2DVertex[4];
            SkASSERT(sizeof(ARGB2DVertex) == this->vertexStride(positionMatrix));
            generalized_direct_2D(quadData((Quad*)vertexDst), color, originOffset, &clip);
        }
    }
}

SkRect DirectMaskSubRun::deviceRect(const SkMatrix& drawMatrix, SkPoint drawOrigin) const {
    SkIRect outBounds = fGlyphDeviceBounds.iRect();

    // Calculate the offset from the initial device origin to the current device origin.
    SkVector offset = drawMatrix.mapPoint(drawOrigin) -
                      fTextReferenceFrame->initialPositionMatrix().mapOrigin();

    // The offset should be integer, but make sure.
    SkIVector iOffset = {SkScalarRoundToInt(offset.x()), SkScalarRoundToInt(offset.y())};

    return SkRect::Make(outBounds.makeOffset(iOffset));
}

const GrAtlasSubRun* DirectMaskSubRun::testingOnly_atlasSubRun() const {
    return this;
}

// -- TransformedMaskSubRun ------------------------------------------------------------------------
class TransformedMaskSubRun final : public GrSubRun, public GrBlobSubRun, public GrAtlasSubRun {
public:
    using VertexData = TransformedMaskVertexFiller::PositionAndExtent;

    TransformedMaskSubRun(const GrTextReferenceFrame* referenceFrame,
                          GrMaskFormat format,
                          SkScalar strikeToSourceScale,
                          const SkRect& bounds,
                          SkSpan<const VertexData> vertexData,
                          GrGlyphVector&& glyphs);

    static GrSubRunOwner Make(const GrTextReferenceFrame* referenceFrame,
                              const SkZip<SkGlyphVariant, SkPoint>& accepted,
                              sk_sp<SkStrike>&& strike,
                              SkScalar strikeToSourceScale,
                              GrMaskFormat format,
                              GrSubRunAllocator* alloc);

    static GrSubRunOwner MakeFromBuffer(const GrTextReferenceFrame* referenceFrame,
                                        SkReadBuffer& buffer,
                                        GrSubRunAllocator* alloc,
                                        const SkStrikeClient* client) {
        return nullptr;
    }

    void draw(SkCanvas*,
              const GrClip*,
              const SkMatrixProvider& viewMatrix,
              SkPoint drawOrigin,
              const SkPaint& paint,
              skgpu::v1::SurfaceDrawContext*) const override;

    std::tuple<const GrClip*, GrOp::Owner>
    makeAtlasTextOp(const GrClip*,
                    const SkMatrixProvider& viewMatrix,
                    SkPoint drawOrigin,
                    const SkPaint&,
                    skgpu::v1::SurfaceDrawContext*,
                    GrAtlasSubRunOwner) const override;

    const GrBlobSubRun* blobCast() const override { return this; }
    int unflattenSize() const override { return 0; }

    bool canReuse(const SkPaint& paint, const SkMatrix& positionMatrix) const override;

    const GrAtlasSubRun* testingOnly_atlasSubRun() const override;

    void testingOnly_packedGlyphIDToGrGlyph(GrStrikeCache *cache) const override;

    std::tuple<bool, int> regenerateAtlas(int begin, int end, GrMeshDrawTarget*) const override;

    void fillVertexData(
            void* vertexDst, int offset, int count,
            GrColor color,
            const SkMatrix& drawMatrix, SkPoint drawOrigin,
            SkIRect clip) const override;

    size_t vertexStride(const SkMatrix& drawMatrix) const override;
    int glyphCount() const override;

protected:
    SubRunType subRunType() const override { return kTransformMask; }
    void doFlatten(SkWriteBuffer& buffer) const override {
        SK_ABORT("Not implemented.");
    }

private:
    // The rectangle that surrounds all the glyph bounding boxes in device space.
    SkRect deviceRect(const SkMatrix& drawMatrix, SkPoint drawOrigin) const;

    const TransformedMaskVertexFiller fVertexFiller;

    const GrTextReferenceFrame* const fReferenceFrame;

    // The bounds in source space. The bounds are the joined rectangles of all the glyphs.
    const SkRect fVertexBounds;
    const SkSpan<const VertexData> fVertexData;

    // The regenerateAtlas method mutates fGlyphs. It should be called from onPrepare which must
    // be single threaded.
    mutable GrGlyphVector fGlyphs;
};

TransformedMaskSubRun::TransformedMaskSubRun(const GrTextReferenceFrame* referenceFrame,
                                             GrMaskFormat format,
                                             SkScalar strikeToSourceScale,
                                             const SkRect& bounds,
                                             SkSpan<const VertexData> vertexData,
                                             GrGlyphVector&& glyphs)
        : fVertexFiller{format, 0, strikeToSourceScale}
        , fReferenceFrame{referenceFrame}
        , fVertexBounds{bounds}
        , fVertexData{vertexData}
        , fGlyphs{std::move(glyphs)} { }

GrSubRunOwner TransformedMaskSubRun::Make(const GrTextReferenceFrame* referenceFrame,
                                          const SkZip<SkGlyphVariant, SkPoint>& accepted,
                                          sk_sp<SkStrike>&& strike,
                                          SkScalar strikeToSourceScale,
                                          GrMaskFormat format,
                                          GrSubRunAllocator* alloc) {
    SkRect bounds = SkRectPriv::MakeLargestInverted();

    SkSpan<VertexData> vertexData = alloc->makePODArray<VertexData>(
            accepted,
            [&](auto e) {
                auto [variant, pos] = e;
                const SkGlyph* skGlyph = variant;
                int16_t l = skGlyph->left(),
                        t = skGlyph->top(),
                        r = l + skGlyph->width(),
                        b = t + skGlyph->height();
                SkPoint lt = SkPoint::Make(l, t) * strikeToSourceScale + pos,
                        rb = SkPoint::Make(r, b) * strikeToSourceScale + pos;

                bounds.joinPossiblyEmptyRect(SkRect::MakeLTRB(lt.x(), lt.y(), rb.x(), rb.y()));
                return VertexData{pos, {l, t, r, b}};
            });

    return alloc->makeUnique<TransformedMaskSubRun>(
            referenceFrame, format, strikeToSourceScale, bounds, vertexData,
            GrGlyphVector::Make(std::move(strike), accepted.get<0>(), alloc));
}

void TransformedMaskSubRun::draw(SkCanvas*,
                                 const GrClip* clip,
                                 const SkMatrixProvider& viewMatrix,
                                 SkPoint drawOrigin,
                                 const SkPaint& paint,
                                 skgpu::v1::SurfaceDrawContext* sdc) const {
    auto[drawingClip, op] = this->makeAtlasTextOp(
            clip, viewMatrix, drawOrigin, paint, sdc, nullptr);
    if (op != nullptr) {
        sdc->addDrawOp(drawingClip, std::move(op));
    }
}

std::tuple<const GrClip*, GrOp::Owner>
TransformedMaskSubRun::makeAtlasTextOp(const GrClip* clip,
                                       const SkMatrixProvider& viewMatrix,
                                       SkPoint drawOrigin,
                                       const SkPaint& paint,
                                       skgpu::v1::SurfaceDrawContext* sdc,
                                       GrAtlasSubRunOwner) const {
    SkASSERT(this->glyphCount() != 0);

    const SkMatrix& drawMatrix = viewMatrix.localToDevice();

    GrPaint grPaint;
    SkPMColor4f drawingColor = calculate_colors(
            sdc, paint, viewMatrix, fVertexFiller.grMaskType(), &grPaint);

    auto geometry = AtlasTextOp::Geometry::MakeForBlob(*this,
                                                       drawMatrix,
                                                       drawOrigin,
                                                       SkIRect::MakeEmpty(),
                                                       sk_ref_sp(fReferenceFrame),
                                                       drawingColor,
                                                       sdc->arenaAlloc());

    GrRecordingContext* const rContext = sdc->recordingContext();
    GrOp::Owner op = GrOp::Make<AtlasTextOp>(rContext,
                                             fVertexFiller.opMaskType(),
                                             true,
                                             this->glyphCount(),
                                             this->deviceRect(drawMatrix, drawOrigin),
                                             geometry,
                                             std::move(grPaint));
    return {clip, std::move(op)};
}

// If we are not scaling the cache entry to be larger, than a cache with smaller glyphs may be
// better.
bool TransformedMaskSubRun::canReuse(const SkPaint& paint, const SkMatrix& positionMatrix) const {
    if (fReferenceFrame->initialPositionMatrix().getMaxScale() < 1) {
        return false;
    }
    return true;
}

void TransformedMaskSubRun::testingOnly_packedGlyphIDToGrGlyph(GrStrikeCache *cache) const {
    fGlyphs.packedGlyphIDToGrGlyph(cache);
}

std::tuple<bool, int> TransformedMaskSubRun::regenerateAtlas(int begin, int end,
                                                             GrMeshDrawTarget* target) const {
    return fGlyphs.regenerateAtlas(begin, end, fVertexFiller.grMaskType(), 1, target, true);
}

void TransformedMaskSubRun::fillVertexData(void* vertexDst, int offset, int count,
                                           GrColor color,
                                           const SkMatrix& drawMatrix, SkPoint drawOrigin,
                                           SkIRect clip) const {
    const SkMatrix positionMatrix = position_matrix(drawMatrix, drawOrigin);
    fVertexFiller.fillVertexData(fGlyphs.glyphs().subspan(offset, count),
                                 fVertexData.subspan(offset, count),
                                 color,
                                 positionMatrix,
                                 clip,
                                 vertexDst);
}

size_t TransformedMaskSubRun::vertexStride(const SkMatrix& drawMatrix) const {
    return fVertexFiller.vertexStride(drawMatrix);
}

int TransformedMaskSubRun::glyphCount() const {
    return SkCount(fVertexData);
}

SkRect TransformedMaskSubRun::deviceRect(const SkMatrix& drawMatrix, SkPoint drawOrigin) const {
    SkRect outBounds = fVertexBounds;
    outBounds.offset(drawOrigin);
    return drawMatrix.mapRect(outBounds);
}

const GrAtlasSubRun* TransformedMaskSubRun::testingOnly_atlasSubRun() const {
    return this;
}

// -- SDFTSubRun -----------------------------------------------------------------------------------
class SDFTSubRun final : public GrSubRun, public GrBlobSubRun, public GrAtlasSubRun {
public:
    using VertexData = TransformedMaskVertexFiller::PositionAndExtent;

    SDFTSubRun(const GrTextReferenceFrame* referenceFrame,
               SkScalar strikeToSource,
               SkRect vertexBounds,
               SkSpan<const VertexData> vertexData,
               GrGlyphVector&& glyphs,
               bool useLCDText,
               bool antiAliased,
               const GrSDFTMatrixRange& matrixRange);

    static GrSubRunOwner Make(const GrTextReferenceFrame* referenceFrame,
                              const SkZip<SkGlyphVariant, SkPoint>& accepted,
                              const SkFont& runFont,
                              sk_sp<SkStrike>&& strike,
                              SkScalar strikeToSourceScale,
                              const GrSDFTMatrixRange& matrixRange,
                              GrSubRunAllocator* alloc);

    static GrSubRunOwner MakeFromBuffer(const GrTextReferenceFrame* referenceFrame,
                                        SkReadBuffer& buffer,
                                        GrSubRunAllocator* alloc,
                                        const SkStrikeClient* client) {
        return nullptr;
    }

    void draw(SkCanvas*,
              const GrClip*,
              const SkMatrixProvider& viewMatrix,
              SkPoint drawOrigin,
              const SkPaint&,
              skgpu::v1::SurfaceDrawContext*) const override;

    std::tuple<const GrClip*, GrOp::Owner>
    makeAtlasTextOp(const GrClip*,
                    const SkMatrixProvider& viewMatrix,
                    SkPoint drawOrigin,
                    const SkPaint&,
                    skgpu::v1::SurfaceDrawContext*,
                    GrAtlasSubRunOwner) const override;

    const GrBlobSubRun* blobCast() const override { return this; }
    int unflattenSize() const override { return 0; }

    bool canReuse(const SkPaint& paint, const SkMatrix& positionMatrix) const override;

    const GrAtlasSubRun* testingOnly_atlasSubRun() const override;

    void testingOnly_packedGlyphIDToGrGlyph(GrStrikeCache *cache) const override;

    std::tuple<bool, int> regenerateAtlas(int begin, int end, GrMeshDrawTarget*) const override;

    void fillVertexData(
            void* vertexDst, int offset, int count,
            GrColor color,
            const SkMatrix& drawMatrix, SkPoint drawOrigin,
            SkIRect clip) const override;

    size_t vertexStride(const SkMatrix& drawMatrix) const override;
    int glyphCount() const override;

protected:
    SubRunType subRunType() const override { return kSDFT; }
    void doFlatten(SkWriteBuffer& buffer) const override {
        SK_ABORT("Not implemented.");
    }

private:
    // The rectangle that surrounds all the glyph bounding boxes in device space.
    SkRect deviceRect(const SkMatrix& drawMatrix, SkPoint drawOrigin) const;

    const GrTextReferenceFrame* const fReferenceFrame;

    const TransformedMaskVertexFiller fVertexFiller;

    // The bounds in source space. The bounds are the joined rectangles of all the glyphs.
    const SkRect fVertexBounds;
    const SkSpan<const VertexData> fVertexData;

    // The regenerateAtlas method mutates fGlyphs. It should be called from onPrepare which must
    // be single threaded.
    mutable GrGlyphVector fGlyphs;

    const bool fUseLCDText;
    const bool fAntiAliased;
    const GrSDFTMatrixRange fMatrixRange;
};

SDFTSubRun::SDFTSubRun(const GrTextReferenceFrame* referenceFrame,
                       SkScalar strikeToSource,
                       SkRect vertexBounds,
                       SkSpan<const VertexData> vertexData,
                       GrGlyphVector&& glyphs,
                       bool useLCDText,
                       bool antiAliased,
                       const GrSDFTMatrixRange& matrixRange)
        : fReferenceFrame{referenceFrame}
        , fVertexFiller{kA8_GrMaskFormat, SK_DistanceFieldInset, strikeToSource}
        , fVertexBounds{vertexBounds}
        , fVertexData{vertexData}
        , fGlyphs{std::move(glyphs)}
        , fUseLCDText{useLCDText}
        , fAntiAliased{antiAliased}
        , fMatrixRange{matrixRange} {}

bool has_some_antialiasing(const SkFont& font ) {
    SkFont::Edging edging = font.getEdging();
    return edging == SkFont::Edging::kAntiAlias
           || edging == SkFont::Edging::kSubpixelAntiAlias;
}

GrSubRunOwner SDFTSubRun::Make(const GrTextReferenceFrame* referenceFrame,
                               const SkZip<SkGlyphVariant, SkPoint>& accepted,
                               const SkFont& runFont,
                               sk_sp<SkStrike>&& strike,
                               SkScalar strikeToSourceScale,
                               const GrSDFTMatrixRange& matrixRange,
                               GrSubRunAllocator* alloc) {
    SkRect bounds = SkRectPriv::MakeLargestInverted();
    auto mapper = [&](const auto& d) {
        auto& [variant, pos] = d;
        const SkGlyph* skGlyph = variant;
        int16_t l = skGlyph->left(),
                t = skGlyph->top(),
                r = l + skGlyph->width(),
                b = t + skGlyph->height();
        SkPoint lt = SkPoint::Make(l, t) * strikeToSourceScale + pos,
                rb = SkPoint::Make(r, b) * strikeToSourceScale + pos;

        bounds.joinPossiblyEmptyRect(SkRect::MakeLTRB(lt.x(), lt.y(), rb.x(), rb.y()));
        return VertexData{pos, {l, t, r, b}};
    };

    SkSpan<VertexData> vertexData = alloc->makePODArray<VertexData>(accepted, mapper);

    return alloc->makeUnique<SDFTSubRun>(
            referenceFrame,
            strikeToSourceScale,
            bounds,
            vertexData,
            GrGlyphVector::Make(std::move(strike), accepted.get<0>(), alloc),
            runFont.getEdging() == SkFont::Edging::kSubpixelAntiAlias,
            has_some_antialiasing(runFont),
            matrixRange);
}

void SDFTSubRun::draw(SkCanvas*,
                      const GrClip* clip,
                      const SkMatrixProvider& viewMatrix,
                      SkPoint drawOrigin,
                      const SkPaint& paint,
                      skgpu::v1::SurfaceDrawContext* sdc) const {
    auto[drawingClip, op] = this->makeAtlasTextOp(
            clip, viewMatrix, drawOrigin, paint, sdc, nullptr);
    if (op != nullptr) {
        sdc->addDrawOp(drawingClip, std::move(op));
    }
}

static std::tuple<AtlasTextOp::MaskType, uint32_t, bool> calculate_sdf_parameters(
        const skgpu::v1::SurfaceDrawContext& sdc,
        const SkMatrix& drawMatrix,
        bool useLCDText,
        bool isAntiAliased) {
    const GrColorInfo& colorInfo = sdc.colorInfo();
    const SkSurfaceProps& props = sdc.surfaceProps();
    bool isBGR = SkPixelGeometryIsBGR(props.pixelGeometry());
    bool isLCD = useLCDText && SkPixelGeometryIsH(props.pixelGeometry());
    using MT = AtlasTextOp::MaskType;
    MT maskType = !isAntiAliased ? MT::kAliasedDistanceField
                                 : isLCD ? (isBGR ? MT::kLCDBGRDistanceField
                                                  : MT::kLCDDistanceField)
                                         : MT::kGrayscaleDistanceField;

    bool useGammaCorrectDistanceTable = colorInfo.isLinearlyBlended();
    uint32_t DFGPFlags = drawMatrix.isSimilarity() ? kSimilarity_DistanceFieldEffectFlag : 0;
    DFGPFlags |= drawMatrix.isScaleTranslate() ? kScaleOnly_DistanceFieldEffectFlag : 0;
    DFGPFlags |= useGammaCorrectDistanceTable ? kGammaCorrect_DistanceFieldEffectFlag : 0;
    DFGPFlags |= MT::kAliasedDistanceField == maskType ? kAliased_DistanceFieldEffectFlag : 0;

    if (isLCD) {
        DFGPFlags |= kUseLCD_DistanceFieldEffectFlag;
        DFGPFlags |= MT::kLCDBGRDistanceField == maskType ? kBGR_DistanceFieldEffectFlag : 0;
    }
    return {maskType, DFGPFlags, useGammaCorrectDistanceTable};
}

std::tuple<const GrClip*, GrOp::Owner >
SDFTSubRun::makeAtlasTextOp(const GrClip* clip,
                            const SkMatrixProvider& viewMatrix,
                            SkPoint drawOrigin,
                            const SkPaint& paint,
                            skgpu::v1::SurfaceDrawContext* sdc,
                            GrAtlasSubRunOwner) const {
    SkASSERT(this->glyphCount() != 0);
    SkASSERT(!viewMatrix.localToDevice().hasPerspective());

    const SkMatrix& drawMatrix = viewMatrix.localToDevice();

    GrPaint grPaint;
    SkPMColor4f drawingColor = calculate_colors(sdc, paint, viewMatrix, kA8_GrMaskFormat, &grPaint);

    auto [maskType, DFGPFlags, useGammaCorrectDistanceTable] =
        calculate_sdf_parameters(*sdc, drawMatrix, fUseLCDText, fAntiAliased);

    auto geometry = AtlasTextOp::Geometry::MakeForBlob(*this,
                                                       drawMatrix,
                                                       drawOrigin,
                                                       SkIRect::MakeEmpty(),
                                                       sk_ref_sp(fReferenceFrame),
                                                       drawingColor,
                                                       sdc->arenaAlloc());

    GrRecordingContext* const rContext = sdc->recordingContext();
    GrOp::Owner op = GrOp::Make<AtlasTextOp>(rContext,
                                             maskType,
                                             true,
                                             this->glyphCount(),
                                             this->deviceRect(drawMatrix, drawOrigin),
                                             SkPaintPriv::ComputeLuminanceColor(paint),
                                             useGammaCorrectDistanceTable,
                                             DFGPFlags,
                                             geometry,
                                             std::move(grPaint));

    return {clip, std::move(op)};
}

bool SDFTSubRun::canReuse(const SkPaint& paint, const SkMatrix& positionMatrix) const {
    return fMatrixRange.matrixInRange(positionMatrix);
}

void SDFTSubRun::testingOnly_packedGlyphIDToGrGlyph(GrStrikeCache *cache) const {
    fGlyphs.packedGlyphIDToGrGlyph(cache);
}

std::tuple<bool, int> SDFTSubRun::regenerateAtlas(
        int begin, int end, GrMeshDrawTarget *target) const {
    return fGlyphs.regenerateAtlas(begin, end, kA8_GrMaskFormat, SK_DistanceFieldInset, target);
}

size_t SDFTSubRun::vertexStride(const SkMatrix& drawMatrix) const {
    return sizeof(Mask2DVertex);
}

void SDFTSubRun::fillVertexData(
        void *vertexDst, int offset, int count,
        GrColor color,
        const SkMatrix& drawMatrix, SkPoint drawOrigin,
        SkIRect clip) const {
    const SkMatrix positionMatrix = position_matrix(drawMatrix, drawOrigin);

    fVertexFiller.fillVertexData(fGlyphs.glyphs().subspan(offset, count),
                                 fVertexData.subspan(offset, count),
                                 color,
                                 positionMatrix,
                                 clip,
                                 vertexDst);
}

int SDFTSubRun::glyphCount() const {
    return SkCount(fVertexData);
}

SkRect SDFTSubRun::deviceRect(const SkMatrix& drawMatrix, SkPoint drawOrigin) const {
    SkRect outBounds = fVertexBounds;
    outBounds.offset(drawOrigin);
    return drawMatrix.mapRect(outBounds);
}

const GrAtlasSubRun* SDFTSubRun::testingOnly_atlasSubRun() const {
    return this;
}

template<typename AddSingleMaskFormat>
void add_multi_mask_format(
        AddSingleMaskFormat addSingleMaskFormat,
        const SkZip<SkGlyphVariant, SkPoint>& accepted,
        sk_sp<SkStrike>&& strike) {
    if (accepted.empty()) { return; }

    auto glyphSpan = accepted.get<0>();
    const SkGlyph* glyph = glyphSpan[0];
    GrMaskFormat format = GrGlyph::FormatFromSkGlyph(glyph->maskFormat());
    size_t startIndex = 0;
    for (size_t i = 1; i < accepted.size(); i++) {
        glyph = glyphSpan[i];
        GrMaskFormat nextFormat = GrGlyph::FormatFromSkGlyph(glyph->maskFormat());
        if (format != nextFormat) {
            auto glyphsWithSameFormat = accepted.subspan(startIndex, i - startIndex);
            // Take a ref on the strike. This should rarely happen.
            addSingleMaskFormat(glyphsWithSameFormat, format, sk_sp<SkStrike>(strike));
            format = nextFormat;
            startIndex = i;
        }
    }
    auto glyphsWithSameFormat = accepted.last(accepted.size() - startIndex);
    addSingleMaskFormat(glyphsWithSameFormat, format, std::move(strike));
}

}  // namespace

// -- GrTextBlob::Key ------------------------------------------------------------------------------

static SkColor compute_canonical_color(const SkPaint& paint, bool lcd) {
    SkColor canonicalColor = SkPaintPriv::ComputeLuminanceColor(paint);
    if (lcd) {
        // This is the correct computation for canonicalColor, but there are tons of cases where LCD
        // can be modified. For now we just regenerate if any run in a textblob has LCD.
        // TODO figure out where all of these modifications are and see if we can incorporate that
        //      logic at a higher level *OR* use sRGB
        //canonicalColor = SkMaskGamma::CanonicalColor(canonicalColor);

        // TODO we want to figure out a way to be able to use the canonical color on LCD text,
        // see the note above.  We pick a placeholder value for LCD text to ensure we always match
        // the same key
        return SK_ColorTRANSPARENT;
    } else {
        // A8, though can have mixed BMP text but it shouldn't matter because BMP text won't have
        // gamma corrected masks anyways, nor color
        U8CPU lum = SkComputeLuminance(SkColorGetR(canonicalColor),
                                       SkColorGetG(canonicalColor),
                                       SkColorGetB(canonicalColor));
        // reduce to our finite number of bits
        canonicalColor = SkMaskGamma::CanonicalColor(SkColorSetRGB(lum, lum, lum));
    }
    return canonicalColor;
}

auto GrTextBlob::Key::Make(const SkGlyphRunList& glyphRunList,
                           const SkPaint& paint,
                           const SkSurfaceProps& surfaceProps,
                           const GrColorInfo& colorInfo,
                           const SkMatrix& drawMatrix,
                           const GrSDFTControl& control) -> std::tuple<bool, Key> {
    SkMaskFilterBase::BlurRec blurRec;
    // It might be worth caching these things, but its not clear at this time
    // TODO for animated mask filters, this will fill up our cache.  We need a safeguard here
    const SkMaskFilter* maskFilter = paint.getMaskFilter();
    bool canCache = glyphRunList.canCache() &&
                    !(paint.getPathEffect() ||
                        (maskFilter && !as_MFB(maskFilter)->asABlur(&blurRec)));

    // If we're doing linear blending, then we can disable the gamma hacks.
    // Otherwise, leave them on. In either case, we still want the contrast boost:
    // TODO: Can we be even smarter about mask gamma based on the dest transfer function?
    SkScalerContextFlags scalerContextFlags = colorInfo.isLinearlyBlended()
                                              ? SkScalerContextFlags::kBoostContrast
                                              : SkScalerContextFlags::kFakeGammaAndBoostContrast;

    GrTextBlob::Key key;
    if (canCache) {
        bool hasLCD = glyphRunList.anyRunsLCD();

        // We canonicalize all non-lcd draws to use kUnknown_SkPixelGeometry
        SkPixelGeometry pixelGeometry =
                hasLCD ? surfaceProps.pixelGeometry() : kUnknown_SkPixelGeometry;

        GrColor canonicalColor = compute_canonical_color(paint, hasLCD);

        key.fPixelGeometry = pixelGeometry;
        key.fUniqueID = glyphRunList.uniqueID();
        key.fStyle = paint.getStyle();
        if (key.fStyle != SkPaint::kFill_Style) {
            key.fFrameWidth = paint.getStrokeWidth();
            key.fMiterLimit = paint.getStrokeMiter();
            key.fJoin = paint.getStrokeJoin();
        }
        key.fHasBlur = maskFilter != nullptr;
        if (key.fHasBlur) {
            key.fBlurRec = blurRec;
        }
        key.fCanonicalColor = canonicalColor;
        key.fScalerContextFlags = scalerContextFlags;

        // Do any runs use direct drawing types?.
        key.fHasSomeDirectSubRuns = false;
        for (auto& run : glyphRunList) {
            SkScalar approximateDeviceTextSize =
                    SkFontPriv::ApproximateTransformedTextSize(run.font(), drawMatrix);
            key.fHasSomeDirectSubRuns |= control.isDirect(approximateDeviceTextSize, paint);
        }

        if (key.fHasSomeDirectSubRuns) {
            // Store the fractional offset of the position. We know that the matrix can't be
            // perspective at this point.
            SkPoint mappedOrigin = drawMatrix.mapOrigin();
            key.fPositionMatrix = drawMatrix;
            key.fPositionMatrix.setTranslateX(
                    mappedOrigin.x() - SkScalarFloorToScalar(mappedOrigin.x()));
            key.fPositionMatrix.setTranslateY(
                    mappedOrigin.y() - SkScalarFloorToScalar(mappedOrigin.y()));
        } else {
            // For path and SDFT, the matrix doesn't matter.
            key.fPositionMatrix = SkMatrix::I();
        }
    }

    return {canCache, key};
}

bool GrTextBlob::Key::operator==(const GrTextBlob::Key& that) const {
    if (fUniqueID != that.fUniqueID) { return false; }
    if (fCanonicalColor != that.fCanonicalColor) { return false; }
    if (fStyle != that.fStyle) { return false; }
    if (fStyle != SkPaint::kFill_Style) {
        if (fFrameWidth != that.fFrameWidth ||
            fMiterLimit != that.fMiterLimit ||
            fJoin != that.fJoin) {
            return false;
        }
    }
    if (fPixelGeometry != that.fPixelGeometry) { return false; }
    if (fHasBlur != that.fHasBlur) { return false; }
    if (fHasBlur) {
        if (fBlurRec.fStyle != that.fBlurRec.fStyle || fBlurRec.fSigma != that.fBlurRec.fSigma) {
            return false;
        }
    }
    if (fScalerContextFlags != that.fScalerContextFlags) { return false; }

    // Just punt on perspective.
    if (fPositionMatrix.hasPerspective()) {
        return false;
    }

    if (fHasSomeDirectSubRuns != that.fHasSomeDirectSubRuns) {
        return false;
    }

    if (fHasSomeDirectSubRuns) {
        auto [compatible, _] = can_use_direct(fPositionMatrix, that.fPositionMatrix);
        return compatible;
    }

    return true;
}

// -- GrTextBlob -----------------------------------------------------------------------------------
void GrTextBlob::operator delete(void* p) { ::operator delete(p); }
void* GrTextBlob::operator new(size_t) { SK_ABORT("All blobs are created by placement new."); }
void* GrTextBlob::operator new(size_t, void* p) { return p; }

GrTextBlob::~GrTextBlob() = default;

sk_sp<GrTextBlob> GrTextBlob::Make(const SkGlyphRunList& glyphRunList,
                                   const SkPaint& paint,
                                   const SkMatrix& positionMatrix,
                                   bool supportBilerpAtlas,
                                   const GrSDFTControl& control,
                                   SkGlyphRunListPainter* painter) {
    // The difference in alignment from the per-glyph data to the SubRun;
    constexpr size_t alignDiff =
            alignof(DirectMaskSubRun) - alignof(DirectMaskSubRun::DevicePosition);
    constexpr size_t vertexDataToSubRunPadding = alignDiff > 0 ? alignDiff : 0;
    size_t totalGlyphCount = glyphRunList.totalGlyphCount();

    // The neededForSubRun is optimized for DirectMaskSubRun which is by far the most common case.
    size_t bytesNeededForSubRun = GrBagOfBytes::PlatformMinimumSizeWithOverhead(
            totalGlyphCount * sizeof(DirectMaskSubRun::DevicePosition)
            + GrGlyphVector::GlyphVectorSize(totalGlyphCount)
            + glyphRunList.runCount() * (sizeof(DirectMaskSubRun) + vertexDataToSubRunPadding),
            alignof(GrTextBlob));

    size_t allocationSize = sizeof(GrTextBlob) + bytesNeededForSubRun;

    void* allocation = ::operator new (allocationSize);

    SkColor initialLuminance = SkPaintPriv::ComputeLuminanceColor(paint);
    sk_sp<GrTextBlob> blob{
        new (allocation) GrTextBlob(
                bytesNeededForSubRun, supportBilerpAtlas, positionMatrix, initialLuminance)};

    const uint64_t uniqueID = glyphRunList.uniqueID();
    for (auto& glyphRun : glyphRunList) {
        painter->processGlyphRun(blob.get(),
                                 glyphRun,
                                 positionMatrix,
                                 paint,
                                 control,
                                 "GrTextBlob",
                                 uniqueID);
    }

    return blob;
}

void GrTextBlob::addKey(const Key& key) {
    fKey = key;
}

bool GrTextBlob::hasPerspective() const { return fInitialPositionMatrix.hasPerspective(); }

bool GrTextBlob::canReuse(const SkPaint& paint, const SkMatrix& positionMatrix) const {
    // A singular matrix will create a GrTextBlob with no SubRuns, but unknown glyphs can
    // also cause empty runs. If there are no subRuns or some glyphs were excluded or perspective,
    // then regenerate when the matrices don't match.
    if ((fSubRunList.isEmpty() || fSomeGlyphsExcluded || hasPerspective()) &&
        fInitialPositionMatrix != positionMatrix)
    {
        return false;
    }

    // If we have LCD text then our canonical color will be set to transparent, in this case we have
    // to regenerate the blob on any color change
    // We use the grPaint to get any color filter effects
    if (fKey.fCanonicalColor == SK_ColorTRANSPARENT &&
        fInitialLuminance != SkPaintPriv::ComputeLuminanceColor(paint)) {
        return false;
    }

    for (const GrSubRun& subRun : fSubRunList) {
        if (!subRun.blobCast()->canReuse(paint, positionMatrix)) {
            return false;
        }
    }

    return true;
}

const GrTextBlob::Key& GrTextBlob::key() const { return fKey; }
size_t GrTextBlob::size() const { return fSize; }

void GrTextBlob::draw(SkCanvas* canvas,
                      const GrClip* clip,
                      const SkMatrixProvider& viewMatrix,
                      SkPoint drawOrigin,
                      const SkPaint& paint,
                      skgpu::v1::SurfaceDrawContext* sdc) {
    for (const GrSubRun& subRun : fSubRunList) {
        subRun.draw(canvas, clip, viewMatrix, drawOrigin, paint, sdc);
    }
}

const GrAtlasSubRun* GrTextBlob::testingOnlyFirstSubRun() const {
    if (fSubRunList.isEmpty()) {
        return nullptr;
    }

    return fSubRunList.front().blobCast()->testingOnly_atlasSubRun();
}

GrTextBlob::GrTextBlob(int allocSize,
                       bool supportBilerpAtlas,
                       const SkMatrix& positionMatrix,
                       SkColor initialLuminance)
        : fAlloc{SkTAddOffset<char>(this, sizeof(GrTextBlob)), allocSize, allocSize/2}
        , fSize{allocSize}
        , fSupportBilerpAtlas{supportBilerpAtlas}
        , fInitialPositionMatrix{positionMatrix}
        , fInitialLuminance{initialLuminance} { }

void GrTextBlob::processDeviceMasks(
        const SkZip<SkGlyphVariant, SkPoint>& accepted, sk_sp<SkStrike>&& strike) {
    SkASSERT(strike != nullptr);
    auto addGlyphsWithSameFormat = [&] (const SkZip<SkGlyphVariant, SkPoint>& accepted,
                                        GrMaskFormat format,
                                        sk_sp<SkStrike>&& runStrike) {
        GrSubRunOwner subRun = DirectMaskSubRun::Make(
                this, accepted, std::move(runStrike), format, &fAlloc);
        if (subRun != nullptr) {
            fSubRunList.append(std::move(subRun));
        } else {
            fSomeGlyphsExcluded = true;
        }
    };
    add_multi_mask_format(addGlyphsWithSameFormat, accepted, std::move(strike));
}

void GrTextBlob::processSourcePaths(const SkZip<SkGlyphVariant, SkPoint>& accepted,
                                    const SkFont& runFont,
                                    SkScalar strikeToSourceScale) {
    fSubRunList.append(PathSubRun::Make(
            accepted, has_some_antialiasing(runFont), strikeToSourceScale, &fAlloc));
}

void GrTextBlob::processSourceDrawables(const SkZip<SkGlyphVariant, SkPoint>& accepted,
                                        const SkFont& runFont,
                                        SkScalar strikeToSourceScale) {
    fSubRunList.append(make_drawable_sub_run<DrawableSubRun>(
            accepted, has_some_antialiasing(runFont), strikeToSourceScale, &fAlloc));
}

void GrTextBlob::processSourceSDFT(const SkZip<SkGlyphVariant, SkPoint>& accepted,
                                   sk_sp<SkStrike>&& strike,
                                   SkScalar strikeToSourceScale,
                                   const SkFont& runFont,
                                   const GrSDFTMatrixRange& matrixRange) {
    fSubRunList.append(SDFTSubRun::Make(
            this, accepted, runFont, std::move(strike), strikeToSourceScale, matrixRange, &fAlloc));
}

void GrTextBlob::processSourceMasks(const SkZip<SkGlyphVariant, SkPoint>& accepted,
                                    sk_sp<SkStrike>&& strike,
                                    SkScalar strikeToSourceScale) {
    auto addGlyphsWithSameFormat = [&] (const SkZip<SkGlyphVariant, SkPoint>& accepted,
                                        GrMaskFormat format,
                                        sk_sp<SkStrike>&& runStrike) {
        GrSubRunOwner subRun = TransformedMaskSubRun::Make(
                this, accepted, std::move(runStrike), strikeToSourceScale, format, &fAlloc);
        if (subRun != nullptr) {
            fSubRunList.append(std::move(subRun));
        } else {
            fSomeGlyphsExcluded = true;
        }
    };
    add_multi_mask_format(addGlyphsWithSameFormat, accepted, std::move(strike));
}

// ----------------------------- Begin no cache implementation -------------------------------------
namespace {
// -- DirectMaskSubRunNoCache ----------------------------------------------------------------------
class DirectMaskSubRunNoCache final : public GrAtlasSubRun {
public:
    using DevicePosition = skvx::Vec<2, int16_t>;

    DirectMaskSubRunNoCache(GrMaskFormat format,
                            bool supportBilerpAtlas,
                            const SkRect& bounds,
                            SkSpan<const DevicePosition> devicePositions,
                            GrGlyphVector&& glyphs);

    static GrAtlasSubRunOwner Make(const SkZip<SkGlyphVariant, SkPoint>& accepted,
                                   sk_sp<SkStrike>&& strike,
                                   GrMaskFormat format,
                                   bool supportBilerpAtlas,
                                   GrSubRunAllocator* alloc);

    size_t vertexStride(const SkMatrix& drawMatrix) const override;

    int glyphCount() const override;

    std::tuple<const GrClip*, GrOp::Owner>
    makeAtlasTextOp(const GrClip*,
                    const SkMatrixProvider& viewMatrix,
                    SkPoint,
                    const SkPaint&,
                    skgpu::v1::SurfaceDrawContext*,
                    GrAtlasSubRunOwner) const override;

    void testingOnly_packedGlyphIDToGrGlyph(GrStrikeCache *cache) const override;

    std::tuple<bool, int>
    regenerateAtlas(int begin, int end, GrMeshDrawTarget*) const override;

    void fillVertexData(void* vertexDst, int offset, int count,
                        GrColor color,
                        const SkMatrix& drawMatrix, SkPoint drawOrigin,
                        SkIRect clip) const override;

private:
    const GrMaskFormat fMaskFormat;

    // Support bilerping from the atlas.
    const bool fSupportBilerpAtlas;

    // The vertex bounds in device space. The bounds are the joined rectangles of all the glyphs.
    const SkRect fGlyphDeviceBounds;
    const SkSpan<const DevicePosition> fLeftTopDevicePos;

    // Space for geometry
    alignas(alignof(AtlasTextOp::Geometry)) char fGeom[sizeof(AtlasTextOp::Geometry)];

    // The regenerateAtlas method mutates fGlyphs. It should be called from onPrepare which must
    // be single threaded.
    mutable GrGlyphVector fGlyphs;
};

DirectMaskSubRunNoCache::DirectMaskSubRunNoCache(GrMaskFormat format,
                                                 bool supportBilerpAtlas,
                                                 const SkRect& deviceBounds,
                                                 SkSpan<const DevicePosition> devicePositions,
                                                 GrGlyphVector&& glyphs)
        : fMaskFormat{format}
        , fSupportBilerpAtlas{supportBilerpAtlas}
        , fGlyphDeviceBounds{deviceBounds}
        , fLeftTopDevicePos{devicePositions}
        , fGlyphs{std::move(glyphs)} { }

GrAtlasSubRunOwner DirectMaskSubRunNoCache::Make(const SkZip<SkGlyphVariant, SkPoint>& accepted,
                                                 sk_sp<SkStrike>&& strike,
                                                 GrMaskFormat format,
                                                 bool supportBilerpAtlas,
                                                 GrSubRunAllocator* alloc) {
    auto glyphLeftTop = alloc->makePODArray<DevicePosition>(accepted.size());
    auto glyphIDs = alloc->makePODArray<GrGlyphVector::Variant>(accepted.size());

    // Because this is the direct case, the maximum width or height is the size that fits in the
    // atlas. This boundary is checked below to ensure that the call to SkGlyphRect below will
    // not overflow.
    constexpr SkScalar kMaxPos =
            std::numeric_limits<int16_t>::max() - SkStrikeCommon::kSkSideTooBigForAtlas;
    SkGlyphRect runBounds = skglyph::empty_rect();
    size_t goodPosCount = 0;
    for (auto [variant, pos] : accepted) {
        auto [x, y] = pos;
        // Ensure that the .offset() call below does not overflow. And, at this point none of the
        // rectangles are empty because they were culled before the run was created. Basically,
        // cull all the glyphs that can't appear on the screen.
        if (-kMaxPos < x && x < kMaxPos && -kMaxPos < y && y < kMaxPos) {
            const SkGlyph* const skGlyph = variant;
            const SkGlyphRect deviceBounds =
                    skGlyph->glyphRect().offset(SkScalarRoundToInt(x), SkScalarRoundToInt(y));
            runBounds = skglyph::rect_union(runBounds, deviceBounds);
            glyphLeftTop[goodPosCount] = deviceBounds.topLeft();
            glyphIDs[goodPosCount].packedGlyphID = skGlyph->getPackedID();
            goodPosCount += 1;
        }
    }

    // Wow! no glyphs are in bounds and had non-empty bounds.
    if (goodPosCount == 0) {
        return nullptr;
    }

    SkSpan<const DevicePosition> leftTop{glyphLeftTop, goodPosCount};
    return alloc->makeUnique<DirectMaskSubRunNoCache>(
            format, supportBilerpAtlas, runBounds.rect(), leftTop,
            GrGlyphVector{std::move(strike), {glyphIDs, goodPosCount}});
}

size_t DirectMaskSubRunNoCache::vertexStride(const SkMatrix&) const {
    if (fMaskFormat != kARGB_GrMaskFormat) {
        return sizeof(Mask2DVertex);
    } else {
        return sizeof(ARGB2DVertex);
    }
}

int DirectMaskSubRunNoCache::glyphCount() const {
    return SkCount(fGlyphs.glyphs());
}

std::tuple<const GrClip*, GrOp::Owner>
DirectMaskSubRunNoCache::makeAtlasTextOp(const GrClip* clip,
                                         const SkMatrixProvider& viewMatrix,
                                         SkPoint drawOrigin,
                                         const SkPaint& paint,
                                         skgpu::v1::SurfaceDrawContext* sdc,
                                         GrAtlasSubRunOwner subRunOwner) const {
    SkASSERT(this->glyphCount() != 0);

    const SkMatrix& drawMatrix = viewMatrix.localToDevice();

    // We can clip geometrically using clipRect and ignore clip when an axis-aligned rectangular
    // non-AA clip is used. If clipRect is empty, and clip is nullptr, then there is no clipping
    // needed.
    const SkRect deviceBounds = SkRect::MakeWH(sdc->width(), sdc->height());
    auto [clipMethod, clipRect] = calculate_clip(clip, deviceBounds, fGlyphDeviceBounds);

    switch (clipMethod) {
        case kClippedOut:
            // Returning nullptr as op means skip this op.
            return {nullptr, nullptr};
        case kUnclipped:
        case kGeometryClipped:
            // GPU clip is not needed.
            clip = nullptr;
            break;
        case kGPUClipped:
            // Use the the GPU clip; clipRect is ignored.
            break;
    }

    if (!clipRect.isEmpty()) { SkASSERT(clip == nullptr); }

    GrPaint grPaint;
    const SkPMColor4f drawingColor =
            calculate_colors(sdc, paint, viewMatrix, fMaskFormat, &grPaint);

    GrRecordingContext* const rContext = sdc->recordingContext();

    auto geometry = new ((void*)fGeom) AtlasTextOp::Geometry{
            *this,
            drawMatrix,
            drawOrigin,
            clipRect,
            nullptr,
            std::move(subRunOwner),
            drawingColor
    };

    GrOp::Owner op = GrOp::Make<AtlasTextOp>(rContext,
                                             op_mask_type(fMaskFormat),
                                             false,
                                             this->glyphCount(),
                                             fGlyphDeviceBounds,
                                             geometry,
                                             std::move(grPaint));

    return {clip, std::move(op)};
}

void DirectMaskSubRunNoCache::testingOnly_packedGlyphIDToGrGlyph(GrStrikeCache *cache) const {
    fGlyphs.packedGlyphIDToGrGlyph(cache);
}

std::tuple<bool, int>
DirectMaskSubRunNoCache::regenerateAtlas(int begin, int end, GrMeshDrawTarget* target) const {
    if (fSupportBilerpAtlas) {
        return fGlyphs.regenerateAtlas(begin, end, fMaskFormat, 1, target, true);
    } else {
        return fGlyphs.regenerateAtlas(begin, end, fMaskFormat, 0, target, false);
    }
}

// The 99% case. No clip. Non-color only.
void direct_2D2(SkZip<Mask2DVertex[4],
        const GrGlyph*,
        const DirectMaskSubRunNoCache::DevicePosition> quadData,
        GrColor color) {
    for (auto[quad, glyph, leftTop] : quadData) {
        auto[al, at, ar, ab] = glyph->fAtlasLocator.getUVs();
        SkScalar dl = leftTop[0],
                 dt = leftTop[1],
                 dr = dl + (ar - al),
                 db = dt + (ab - at);

        quad[0] = {{dl, dt}, color, {al, at}};  // L,T
        quad[1] = {{dl, db}, color, {al, ab}};  // L,B
        quad[2] = {{dr, dt}, color, {ar, at}};  // R,T
        quad[3] = {{dr, db}, color, {ar, ab}};  // R,B
    }
}

void DirectMaskSubRunNoCache::fillVertexData(void* vertexDst, int offset, int count,
                                             GrColor color,
                                             const SkMatrix& drawMatrix, SkPoint drawOrigin,
                                             SkIRect clip) const {
    auto quadData = [&](auto dst) {
        return SkMakeZip(dst,
                         fGlyphs.glyphs().subspan(offset, count),
                         fLeftTopDevicePos.subspan(offset, count));
    };

    // Notice that no matrix manipulation is needed because all the rectangles are already mapped
    // to device space.
    if (clip.isEmpty()) {
        if (fMaskFormat != kARGB_GrMaskFormat) {
            using Quad = Mask2DVertex[4];
            SkASSERT(sizeof(Mask2DVertex) == this->vertexStride(SkMatrix::I()));
            direct_2D2(quadData((Quad*)vertexDst), color);
        } else {
            using Quad = ARGB2DVertex[4];
            SkASSERT(sizeof(ARGB2DVertex) == this->vertexStride(SkMatrix::I()));
            generalized_direct_2D(quadData((Quad*)vertexDst), color, {0,0});
        }
    } else {
        if (fMaskFormat != kARGB_GrMaskFormat) {
            using Quad = Mask2DVertex[4];
            SkASSERT(sizeof(Mask2DVertex) == this->vertexStride(SkMatrix::I()));
            generalized_direct_2D(quadData((Quad*)vertexDst), color, {0,0}, &clip);
        } else {
            using Quad = ARGB2DVertex[4];
            SkASSERT(sizeof(ARGB2DVertex) == this->vertexStride(SkMatrix::I()));
            generalized_direct_2D(quadData((Quad*)vertexDst), color, {0,0}, &clip);
        }
    }
}

// -- TransformedMaskSubRunNoCache -----------------------------------------------------------------
class TransformedMaskSubRunNoCache final : public GrAtlasSubRun {
public:
    using VertexData = TransformedMaskVertexFiller::PositionAndExtent;

    TransformedMaskSubRunNoCache(GrMaskFormat format,
                                 SkScalar strikeToSourceScale,
                                 const SkRect& bounds,
                                 SkSpan<const VertexData> vertexData,
                                 GrGlyphVector&& glyphs);

    static GrAtlasSubRunOwner Make(const SkZip<SkGlyphVariant, SkPoint>& accepted,
                                   sk_sp<SkStrike>&& strike,
                                   SkScalar strikeToSourceScale,
                                   GrMaskFormat format,
                                   GrSubRunAllocator* alloc);

    std::tuple<const GrClip*, GrOp::Owner>
    makeAtlasTextOp(const GrClip*,
                    const SkMatrixProvider& viewMatrix,
                    SkPoint drawOrigin,
                    const SkPaint&,
                    skgpu::v1::SurfaceDrawContext*,
                    GrAtlasSubRunOwner) const override;

    void testingOnly_packedGlyphIDToGrGlyph(GrStrikeCache *cache) const override;

    std::tuple<bool, int> regenerateAtlas(int begin, int end, GrMeshDrawTarget*) const override;

    void fillVertexData(
            void* vertexDst, int offset, int count,
            GrColor color,
            const SkMatrix& drawMatrix, SkPoint drawOrigin,
            SkIRect clip) const override;

    size_t vertexStride(const SkMatrix& drawMatrix) const override;
    int glyphCount() const override;

private:
    // The rectangle that surrounds all the glyph bounding boxes in device space.
    SkRect deviceRect(const SkMatrix& drawMatrix, SkPoint drawOrigin) const;

    const TransformedMaskVertexFiller fVertexFiller;

    // The bounds in source space. The bounds are the joined rectangles of all the glyphs.
    const SkRect fVertexBounds;
    const SkSpan<const VertexData> fVertexData;

    // Space for geometry
    alignas(alignof(AtlasTextOp::Geometry)) char fGeom[sizeof(AtlasTextOp::Geometry)];

    // The regenerateAtlas method mutates fGlyphs. It should be called from onPrepare which must
    // be single threaded.
    mutable GrGlyphVector fGlyphs;
};

TransformedMaskSubRunNoCache::TransformedMaskSubRunNoCache(GrMaskFormat format,
                                                           SkScalar strikeToSourceScale,
                                                           const SkRect& bounds,
                                                           SkSpan<const VertexData> vertexData,
                                                           GrGlyphVector&& glyphs)
        : fVertexFiller{format, 0, strikeToSourceScale}
        , fVertexBounds{bounds}
        , fVertexData{vertexData}
        , fGlyphs{std::move(glyphs)} {}

GrAtlasSubRunOwner TransformedMaskSubRunNoCache::Make(
        const SkZip<SkGlyphVariant, SkPoint>& accepted,
        sk_sp<SkStrike>&& strike,
        SkScalar strikeToSourceScale,
        GrMaskFormat format,
        GrSubRunAllocator* alloc) {
    SkRect bounds = SkRectPriv::MakeLargestInverted();
    auto initializer = [&](auto acceptedGlyph) {
        auto [variant, pos] = acceptedGlyph;
        const SkGlyph* skGlyph = variant;
        int16_t l = skGlyph->left(),
                t = skGlyph->top(),
                r = l + skGlyph->width(),
                b = t + skGlyph->height();
        SkPoint lt = SkPoint::Make(l, t) * strikeToSourceScale + pos,
                rb = SkPoint::Make(r, b) * strikeToSourceScale + pos;

        bounds.joinPossiblyEmptyRect(SkRect::MakeLTRB(lt.x(), lt.y(), rb.x(), rb.y()));
        return VertexData{pos, {l, t, r, b}};
    };

    SkSpan<VertexData> vertexData = alloc->makePODArray<VertexData>(accepted, initializer);

    return alloc->makeUnique<TransformedMaskSubRunNoCache>(
            format, strikeToSourceScale, bounds, vertexData,
            GrGlyphVector::Make(std::move(strike), accepted.get<0>(), alloc));
}

std::tuple<const GrClip*, GrOp::Owner>
TransformedMaskSubRunNoCache::makeAtlasTextOp(const GrClip* clip,
                                              const SkMatrixProvider& viewMatrix,
                                              SkPoint drawOrigin,
                                              const SkPaint& paint,
                                              skgpu::v1::SurfaceDrawContext* sdc,
                                              GrAtlasSubRunOwner subRunOwner) const {
    SkASSERT(this->glyphCount() != 0);

    const SkMatrix& drawMatrix = viewMatrix.localToDevice();

    GrPaint grPaint;
    SkPMColor4f drawingColor = calculate_colors(
            sdc, paint, viewMatrix, fVertexFiller.grMaskType(), &grPaint);

    // We can clip geometrically using clipRect and ignore clip if we're not using SDFs or
    // transformed glyphs, and we have an axis-aligned rectangular non-AA clip.
    auto geometry = new ((void*)fGeom) AtlasTextOp::Geometry{
            *this,
            drawMatrix,
            drawOrigin,
            SkIRect::MakeEmpty(),
            nullptr,
            std::move(subRunOwner),
            drawingColor
    };

    GrRecordingContext* rContext = sdc->recordingContext();
    GrOp::Owner op = GrOp::Make<AtlasTextOp>(rContext,
                                             fVertexFiller.opMaskType(),
                                             true,
                                             this->glyphCount(),
                                             this->deviceRect(drawMatrix, drawOrigin),
                                             geometry,
                                             std::move(grPaint));
    return {clip, std::move(op)};
}

void TransformedMaskSubRunNoCache::testingOnly_packedGlyphIDToGrGlyph(GrStrikeCache *cache) const {
    fGlyphs.packedGlyphIDToGrGlyph(cache);
}

std::tuple<bool, int> TransformedMaskSubRunNoCache::regenerateAtlas(
        int begin, int end, GrMeshDrawTarget* target) const {
    return fGlyphs.regenerateAtlas(begin, end, fVertexFiller.grMaskType(), 1, target, true);
}

void TransformedMaskSubRunNoCache::fillVertexData(
        void* vertexDst, int offset, int count,
        GrColor color,
        const SkMatrix& drawMatrix, SkPoint drawOrigin,
        SkIRect clip) const {
    const SkMatrix positionMatrix = position_matrix(drawMatrix, drawOrigin);
    fVertexFiller.fillVertexData(fGlyphs.glyphs().subspan(offset, count),
                                 fVertexData.subspan(offset, count),
                                 color,
                                 positionMatrix,
                                 clip,
                                 vertexDst);
}

size_t TransformedMaskSubRunNoCache::vertexStride(const SkMatrix& drawMatrix) const {
    return fVertexFiller.vertexStride(drawMatrix);
}

int TransformedMaskSubRunNoCache::glyphCount() const {
    return SkCount(fVertexData);
}

SkRect TransformedMaskSubRunNoCache::deviceRect(
        const SkMatrix& drawMatrix, SkPoint drawOrigin) const {
    SkRect outBounds = fVertexBounds;
    outBounds.offset(drawOrigin);
    return drawMatrix.mapRect(outBounds);
}

// -- SDFTSubRunNoCache ----------------------------------------------------------------------------
class SDFTSubRunNoCache final : public GrAtlasSubRun {
public:
    struct VertexData {
        const SkPoint pos;
        // The rectangle of the glyphs in strike space.
        GrIRect16 rect;
    };

    SDFTSubRunNoCache(GrMaskFormat format,
                      SkScalar strikeToSourceScale,
                      SkRect vertexBounds,
                      SkSpan<const VertexData> vertexData,
                      GrGlyphVector&& glyphs,
                      bool useLCDText,
                      bool antiAliased);

    static GrAtlasSubRunOwner Make(const SkZip<SkGlyphVariant, SkPoint>& accepted,
                                   const SkFont& runFont,
                                   sk_sp<SkStrike>&& strike,
                                   SkScalar strikeToSourceScale,
                                   GrSubRunAllocator* alloc);

    std::tuple<const GrClip*, GrOp::Owner>
    makeAtlasTextOp(const GrClip*,
                    const SkMatrixProvider& viewMatrix,
                    SkPoint drawOrigin,
                    const SkPaint&,
                    skgpu::v1::SurfaceDrawContext*,
                    GrAtlasSubRunOwner) const override;

    void testingOnly_packedGlyphIDToGrGlyph(GrStrikeCache *cache) const override;

    std::tuple<bool, int> regenerateAtlas(int begin, int end, GrMeshDrawTarget*) const override;

    void fillVertexData(
            void* vertexDst, int offset, int count,
            GrColor color,
            const SkMatrix& drawMatrix, SkPoint drawOrigin,
            SkIRect clip) const override;

    size_t vertexStride(const SkMatrix& drawMatrix) const override;
    int glyphCount() const override;

private:
    // The rectangle that surrounds all the glyph bounding boxes in device space.
    SkRect deviceRect(const SkMatrix& drawMatrix, SkPoint drawOrigin) const;

    const GrMaskFormat fMaskFormat;

    // The scale factor between the strike size, and the source size.
    const SkScalar fStrikeToSourceScale;

    // The bounds in source space. The bounds are the joined rectangles of all the glyphs.
    const SkRect fVertexBounds;
    const SkSpan<const VertexData> fVertexData;

    // Space for geometry
    alignas(alignof(AtlasTextOp::Geometry)) char fGeom[sizeof(AtlasTextOp::Geometry)];

    // The regenerateAtlas method mutates fGlyphs. It should be called from onPrepare which must
    // be single threaded.
    mutable GrGlyphVector fGlyphs;

    const bool fUseLCDText;
    const bool fAntiAliased;
};

SDFTSubRunNoCache::SDFTSubRunNoCache(GrMaskFormat format,
                                     SkScalar strikeToSourceScale,
                                     SkRect vertexBounds,
                                     SkSpan<const VertexData> vertexData,
                                     GrGlyphVector&& glyphs,
                                     bool useLCDText,
                                     bool antiAliased)
        : fMaskFormat{format}
        , fStrikeToSourceScale{strikeToSourceScale}
        , fVertexBounds{vertexBounds}
        , fVertexData{vertexData}
        , fGlyphs{std::move(glyphs)}
        , fUseLCDText{useLCDText}
        , fAntiAliased{antiAliased} {}


GrAtlasSubRunOwner SDFTSubRunNoCache::Make(
        const SkZip<SkGlyphVariant, SkPoint>& accepted,
        const SkFont& runFont,
        sk_sp<SkStrike>&& strike,
        SkScalar strikeToSourceScale,
        GrSubRunAllocator* alloc) {

    SkRect bounds = SkRectPriv::MakeLargestInverted();
    auto initializer = [&](auto acceptedGlyph) {
        auto [variant, pos] = acceptedGlyph;
        const SkGlyph* skGlyph = variant;
        int16_t l = skGlyph->left(),
                t = skGlyph->top(),
                r = l + skGlyph->width(),
                b = t + skGlyph->height();
        SkPoint lt = SkPoint::Make(l, t) * strikeToSourceScale + pos,
                rb = SkPoint::Make(r, b) * strikeToSourceScale + pos;

        bounds.joinPossiblyEmptyRect(SkRect::MakeLTRB(lt.x(), lt.y(), rb.x(), rb.y()));
        return VertexData{pos, {l, t, r, b}};
    };

    SkSpan<VertexData> vertexData = alloc->makePODArray<VertexData>(accepted, initializer);

    return alloc->makeUnique<SDFTSubRunNoCache>(
            kA8_GrMaskFormat,
            strikeToSourceScale,
            bounds,
            vertexData,
            GrGlyphVector::Make(std::move(strike), accepted.get<0>(), alloc),
            runFont.getEdging() == SkFont::Edging::kSubpixelAntiAlias,
            has_some_antialiasing(runFont));
}

std::tuple<const GrClip*, GrOp::Owner>
SDFTSubRunNoCache::makeAtlasTextOp(const GrClip* clip,
                                   const SkMatrixProvider& viewMatrix,
                                   SkPoint drawOrigin,
                                   const SkPaint& paint,
                                   skgpu::v1::SurfaceDrawContext* sdc,
                                   GrAtlasSubRunOwner subRunOwner) const {
    SkASSERT(this->glyphCount() != 0);

    const SkMatrix& drawMatrix = viewMatrix.localToDevice();

    GrPaint grPaint;
    SkPMColor4f drawingColor = calculate_colors(sdc, paint, viewMatrix, fMaskFormat, &grPaint);

    auto [maskType, DFGPFlags, useGammaCorrectDistanceTable] =
    calculate_sdf_parameters(*sdc, drawMatrix, fUseLCDText, fAntiAliased);

    auto geometry = new ((void*)fGeom) AtlasTextOp::Geometry {
            *this,
            drawMatrix,
            drawOrigin,
            SkIRect::MakeEmpty(),
            nullptr,
            std::move(subRunOwner),
            drawingColor
    };

    GrRecordingContext* rContext = sdc->recordingContext();
    GrOp::Owner op = GrOp::Make<AtlasTextOp>(rContext,
                                             maskType,
                                             true,
                                             this->glyphCount(),
                                             this->deviceRect(drawMatrix, drawOrigin),
                                             SkPaintPriv::ComputeLuminanceColor(paint),
                                             useGammaCorrectDistanceTable,
                                             DFGPFlags,
                                             geometry,
                                             std::move(grPaint));

    return {clip, std::move(op)};
}

void SDFTSubRunNoCache::testingOnly_packedGlyphIDToGrGlyph(GrStrikeCache *cache) const {
    fGlyphs.packedGlyphIDToGrGlyph(cache);
}

std::tuple<bool, int> SDFTSubRunNoCache::regenerateAtlas(
        int begin, int end, GrMeshDrawTarget *target) const {

    return fGlyphs.regenerateAtlas(begin, end, fMaskFormat, SK_DistanceFieldInset, target);
}

size_t SDFTSubRunNoCache::vertexStride(const SkMatrix& drawMatrix) const {
    return sizeof(Mask2DVertex);
}

void SDFTSubRunNoCache::fillVertexData(
        void *vertexDst, int offset, int count,
        GrColor color,
        const SkMatrix& drawMatrix, SkPoint drawOrigin,
        SkIRect clip) const {
    using Quad = Mask2DVertex[4];

    const SkMatrix positionMatrix = position_matrix(drawMatrix, drawOrigin);

    SkASSERT(sizeof(Mask2DVertex) == this->vertexStride(positionMatrix));
    fill_transformed_vertices_2D(
            SkMakeZip((Quad*)vertexDst,
                      fGlyphs.glyphs().subspan(offset, count),
                      fVertexData.subspan(offset, count)),
            SK_DistanceFieldInset,
            fStrikeToSourceScale,
            color,
            positionMatrix);
}

int SDFTSubRunNoCache::glyphCount() const {
    return SkCount(fVertexData);
}

SkRect SDFTSubRunNoCache::deviceRect(const SkMatrix& drawMatrix, SkPoint drawOrigin) const {
    SkRect outBounds = fVertexBounds;
    outBounds.offset(drawOrigin);
    return drawMatrix.mapRect(outBounds);
}
}  // namespace

GrSubRunNoCachePainter::GrSubRunNoCachePainter(SkCanvas* canvas,
                                               skgpu::v1::SurfaceDrawContext* sdc,
                                               GrSubRunAllocator* alloc,
                                               const GrClip* clip,
                                               const SkMatrixProvider& viewMatrix,
                                               const SkGlyphRunList& glyphRunList,
                                               const SkPaint& paint)
            : fCanvas{canvas}
            , fSDC{sdc}
            , fAlloc{alloc}
            , fClip{clip}
            , fViewMatrix{viewMatrix}
            , fGlyphRunList{glyphRunList}
            , fPaint {paint} {}

void GrSubRunNoCachePainter::processDeviceMasks(
        const SkZip<SkGlyphVariant, SkPoint>& accepted, sk_sp<SkStrike>&& strike) {
    auto addGlyphsWithSameFormat = [&] (const SkZip<SkGlyphVariant, SkPoint>& accepted,
                                        GrMaskFormat format,
                                        sk_sp<SkStrike>&& runStrike) {
        const bool padAtlas =
                fSDC->recordingContext()->priv().options().fSupportBilerpFromGlyphAtlas;
        this->draw(DirectMaskSubRunNoCache::Make(
                accepted, std::move(runStrike), format, padAtlas, fAlloc));
    };

    add_multi_mask_format(addGlyphsWithSameFormat, accepted, std::move(strike));
}

void GrSubRunNoCachePainter::processSourceMasks(const SkZip<SkGlyphVariant, SkPoint>& accepted,
                                                sk_sp<SkStrike>&& strike,
                                                SkScalar strikeToSourceScale) {
    auto addGlyphsWithSameFormat = [&] (const SkZip<SkGlyphVariant, SkPoint>& accepted,
                                        GrMaskFormat format,
                                        sk_sp<SkStrike>&& runStrike) {
        this->draw(TransformedMaskSubRunNoCache::Make(
                accepted, std::move(runStrike), strikeToSourceScale, format, fAlloc));
    };
    add_multi_mask_format(addGlyphsWithSameFormat, accepted, std::move(strike));
}

void GrSubRunNoCachePainter::processSourcePaths(const SkZip<SkGlyphVariant, SkPoint>& accepted,
                                                const SkFont& runFont,
                                                SkScalar strikeToSourceScale) {
    PathOpSubmitter pathDrawing =
            PathOpSubmitter::Make(accepted,
                                  has_some_antialiasing(runFont),
                                  strikeToSourceScale,
                                  fAlloc);

    pathDrawing.submitOps(fCanvas, fClip, fViewMatrix, fGlyphRunList.origin(), fPaint, fSDC);
}

void GrSubRunNoCachePainter::processSourceDrawables(const SkZip<SkGlyphVariant, SkPoint>& accepted,
                                                    const SkFont& runFont,
                                                    SkScalar strikeToSourceScale) {
    DrawableOpSubmitter drawableDrawing =
            DrawableOpSubmitter::Make(accepted,
                                      has_some_antialiasing(runFont),
                                      strikeToSourceScale,
                                      fAlloc);

    drawableDrawing.submitOps(fCanvas, fClip, fViewMatrix, fGlyphRunList.origin(), fPaint, fSDC);
}

void GrSubRunNoCachePainter::processSourceSDFT(const SkZip<SkGlyphVariant, SkPoint>& accepted,
                                               sk_sp<SkStrike>&& strike,
                                               SkScalar strikeToSourceScale,
                                               const SkFont& runFont,
                                               const GrSDFTMatrixRange&) {
    if (accepted.empty()) {
        return;
    }
    this->draw(SDFTSubRunNoCache::Make(
            accepted, runFont, std::move(strike), strikeToSourceScale, fAlloc));
}

void GrSubRunNoCachePainter::draw(GrAtlasSubRunOwner subRun) {
    if (subRun == nullptr) {
        return;
    }
    GrAtlasSubRun* subRunPtr = subRun.get();
    auto [drawingClip, op] = subRunPtr->makeAtlasTextOp(
            fClip, fViewMatrix, fGlyphRunList.origin(), fPaint, fSDC, std::move(subRun));
    if (op != nullptr) {
        fSDC->addDrawOp(drawingClip, std::move(op));
    }
}

namespace {
// -- Slug -----------------------------------------------------------------------------------------
class Slug final : public GrSlug, public SkGlyphRunPainterInterface {
public:
    Slug(SkRect sourceBounds,
         const SkPaint& paint,
         const SkMatrix& positionMatrix,
         SkPoint origin,
         int allocSize);
    ~Slug() override = default;

    static sk_sp<Slug> Make(const SkMatrixProvider& viewMatrix,
                            const SkGlyphRunList& glyphRunList,
                            const SkPaint& paint,
                            const GrSDFTControl& control,
                            SkGlyphRunListPainter* painter);
    static sk_sp<GrSlug> MakeFromBuffer(SkReadBuffer& buffer,
                                        const SkStrikeClient* client);

    void surfaceDraw(SkCanvas*,
                     const GrClip* clip,
                     const SkMatrixProvider& viewMatrix,
                     skgpu::v1::SurfaceDrawContext* sdc);

    void flatten(SkWriteBuffer& buffer) const override;
    SkRect sourceBounds() const override { return fSourceBounds; }
    const SkPaint& paint() const override { return fPaint; }

    // SkGlyphRunPainterInterface
    void processDeviceMasks(
            const SkZip<SkGlyphVariant, SkPoint>& accepted, sk_sp<SkStrike>&& strike) override;
    void processSourceMasks(
            const SkZip<SkGlyphVariant, SkPoint>& accepted, sk_sp<SkStrike>&& strike,
            SkScalar strikeToSourceScale) override;
    void processSourcePaths(
            const SkZip<SkGlyphVariant, SkPoint>& accepted, const SkFont& runFont,
            SkScalar strikeToSourceScale) override;
    void processSourceDrawables(
            const SkZip<SkGlyphVariant, SkPoint>& drawables, const SkFont& runFont,
            SkScalar strikeToSourceScale) override;
    void processSourceSDFT(
            const SkZip<SkGlyphVariant, SkPoint>& accepted, sk_sp<SkStrike>&& strike,
            SkScalar strikeToSourceScale, const SkFont& runFont,
            const GrSDFTMatrixRange& matrixRange) override;

    const SkMatrix& initialPositionMatrix() const override { return fInitialPositionMatrix; }
    SkPoint origin() const { return fOrigin; }

    // Change memory management to handle the data after Slug, but in the same allocation
    // of memory. Only allow placement new.
    void operator delete(void* p) { ::operator delete(p); }
    void* operator new(size_t) { SK_ABORT("All slugs are created by placement new."); }
    void* operator new(size_t, void* p) { return p; }

    std::tuple<int, int> subRunCountAndUnflattenSizeHint() const {
        int unflattenSizeHint = 0;
        int subRunCount = 0;
        for (auto& subrun : fSubRuns) {
            subRunCount += 1;
            unflattenSizeHint += subrun.unflattenSize();
        }
        return {subRunCount, unflattenSizeHint};
    }

private:
    // The allocator must come first because it needs to be destroyed last. Other fields of this
    // structure may have pointers into it.
    GrSubRunAllocator fAlloc;
    const SkRect fSourceBounds;
    const SkPaint fPaint;
    const SkMatrix fInitialPositionMatrix;
    const SkPoint fOrigin;
    GrSubRunList fSubRuns;
};

Slug::Slug(SkRect sourceBounds,
           const SkPaint& paint,
           const SkMatrix& positionMatrix,
           SkPoint origin,
           int allocSize)
           : fAlloc {SkTAddOffset<char>(this, sizeof(Slug)), allocSize, allocSize/2}
           , fSourceBounds{sourceBounds}
           , fPaint{paint}
           , fInitialPositionMatrix{positionMatrix}
           , fOrigin{origin} { }

void Slug::surfaceDraw(SkCanvas* canvas, const GrClip* clip, const SkMatrixProvider& viewMatrix,
                       skgpu::v1::SurfaceDrawContext* sdc) {
    for (const GrSubRun& subRun : fSubRuns) {
        subRun.draw(canvas, clip, viewMatrix, fOrigin, fPaint, sdc);
    }
}

void Slug::flatten(SkWriteBuffer& buffer) const {
    buffer.writeRect(fSourceBounds);
    SkPaintPriv::Flatten(fPaint, buffer);
    buffer.writeMatrix(fInitialPositionMatrix);
    buffer.writePoint(fOrigin);
    auto [subRunCount, subRunsUnflattenSizeHint] = this->subRunCountAndUnflattenSizeHint();
    buffer.writeInt(subRunCount);
    buffer.writeInt(subRunsUnflattenSizeHint);
    for (auto& subRun : fSubRuns) {
        subRun.flatten(buffer);
    }
}

sk_sp<GrSlug> Slug::MakeFromBuffer(SkReadBuffer& buffer, const SkStrikeClient* client) {
    SkRect sourceBounds = buffer.readRect();
    if (!buffer.validate(!sourceBounds.isEmpty())) { return nullptr; }

    SkPaint paint = buffer.readPaint();
    SkMatrix positionMatrix;
    buffer.readMatrix(&positionMatrix);
    SkPoint origin = buffer.readPoint();
    int subRunCount = buffer.readInt();
    if (!buffer.validate(subRunCount != 0)) { return nullptr; }
    int subRunsUnflattenSizeHint = buffer.readInt();

    sk_sp<Slug> slug{new (::operator new (sizeof(Slug) + subRunsUnflattenSizeHint))
                             Slug(sourceBounds,
                                  paint,
                                  positionMatrix,
                                  origin,
                                  subRunsUnflattenSizeHint)};
    for (int i = 0; i < subRunCount; ++i) {
        auto subRun = GrSubRun::MakeFromBuffer(slug.get(), buffer, &slug->fAlloc, client);
        if (!buffer.validate(subRun != nullptr)) { return nullptr; }
        slug->fSubRuns.append(std::move(subRun));
    }

    // Something went wrong while reading.
    if (!buffer.isValid()) { return nullptr;}

    return std::move(slug);
}

// -- DirectMaskSubRunSlug -------------------------------------------------------------------------
class DirectMaskSubRunSlug final : public GrSubRun, public GrAtlasSubRun {
public:
    using DevicePosition = skvx::Vec<2, int16_t>;

    DirectMaskSubRunSlug(const GrTextReferenceFrame* referenceFrame,
                         GrMaskFormat format,
                         SkGlyphRect deviceBounds,
                         SkSpan<const DevicePosition> devicePositions,
                         GrGlyphVector&& glyphs);

    static GrSubRunOwner Make(const GrTextReferenceFrame* referenceFrame,
                              const SkZip<SkGlyphVariant, SkPoint>& accepted,
                              sk_sp<SkStrike>&& strike,
                              GrMaskFormat format,
                              GrSubRunAllocator* alloc);

    static GrSubRunOwner MakeFromBuffer(const GrTextReferenceFrame* referenceFrame,
                                        SkReadBuffer& buffer,
                                        GrSubRunAllocator* alloc,
                                        const SkStrikeClient* client);

    void draw(SkCanvas*,
              const GrClip* clip,
              const SkMatrixProvider& viewMatrix,
              SkPoint drawOrigin,
              const SkPaint& paint,
              skgpu::v1::SurfaceDrawContext* sdc) const override {
        auto [drawingClip, op] = this->makeAtlasTextOp(
                clip, viewMatrix, drawOrigin, paint, sdc, nullptr);
        if (op != nullptr) {
            sdc->addDrawOp(drawingClip, std::move(op));
        }
    }

    int unflattenSize() const override;

    size_t vertexStride(const SkMatrix& drawMatrix) const override;

    int glyphCount() const override;

    std::tuple<const GrClip*, GrOp::Owner>
    makeAtlasTextOp(const GrClip*,
                    const SkMatrixProvider& viewMatrix,
                    SkPoint,
                    const SkPaint&,
                    skgpu::v1::SurfaceDrawContext*,
                    GrAtlasSubRunOwner) const override;

    void testingOnly_packedGlyphIDToGrGlyph(GrStrikeCache *cache) const override;

    std::tuple<bool, int>
    regenerateAtlas(int begin, int end, GrMeshDrawTarget*) const override;

    void fillVertexData(void* vertexDst, int offset, int count,
                        GrColor color,
                        const SkMatrix& drawMatrix, SkPoint drawOrigin,
                        SkIRect clip) const override;

protected:
    SubRunType subRunType() const override { return kDirectMask; }
    void doFlatten(SkWriteBuffer& buffer) const override;

private:
    // Return true if the positionMatrix represents an integer translation. Return the device
    // bounding box of all the glyphs. If the bounding box is empty, then something went singular
    // and this operation should be dropped.
    std::tuple<bool, SkRect> deviceRectAndCheckTransform(const SkMatrix& positionMatrix) const;

    const GrTextReferenceFrame* const fReferenceFrame;
    const GrMaskFormat fMaskFormat;

    // The vertex bounds in device space. The bounds are the joined rectangles of all the glyphs.
    const SkGlyphRect fGlyphDeviceBounds;
    const SkSpan<const DevicePosition> fLeftTopDevicePos;

    // The regenerateAtlas method mutates fGlyphs. It should be called from onPrepare which must
    // be single threaded.
    mutable GrGlyphVector fGlyphs;
};

DirectMaskSubRunSlug::DirectMaskSubRunSlug(const GrTextReferenceFrame* referenceFrame,
                                           GrMaskFormat format,
                                           SkGlyphRect deviceBounds,
                                           SkSpan<const DevicePosition> devicePositions,
                                           GrGlyphVector&& glyphs)
        : fReferenceFrame{referenceFrame}
        , fMaskFormat{format}
        , fGlyphDeviceBounds{deviceBounds}
        , fLeftTopDevicePos{devicePositions}
        , fGlyphs{std::move(glyphs)} { }

GrSubRunOwner DirectMaskSubRunSlug::Make(const GrTextReferenceFrame* referenceFrame,
                                         const SkZip<SkGlyphVariant, SkPoint>& accepted,
                                         sk_sp<SkStrike>&& strike,
                                         GrMaskFormat format,
                                         GrSubRunAllocator* alloc) {
    auto glyphLeftTop = alloc->makePODArray<DevicePosition>(accepted.size());
    auto glyphIDs = alloc->makePODArray<GrGlyphVector::Variant>(accepted.size());

    // Because this is the direct case, the maximum width or height is the size that fits in the
    // atlas. This boundary is checked below to ensure that the call to SkGlyphRect below will
    // not overflow.
    constexpr SkScalar kMaxPos =
            std::numeric_limits<int16_t>::max() - SkStrikeCommon::kSkSideTooBigForAtlas;
    SkGlyphRect runBounds = skglyph::empty_rect();
    size_t goodPosCount = 0;
    for (auto [variant, pos] : accepted) {
        auto [x, y] = pos;
        // Ensure that the .offset() call below does not overflow. And, at this point none of the
        // rectangles are empty because they were culled before the run was created. Basically,
        // cull all the glyphs that can't appear on the screen.
        if (-kMaxPos < x && x < kMaxPos && -kMaxPos  < y && y < kMaxPos) {
            const SkGlyph* const skGlyph = variant;
            const SkGlyphRect deviceBounds =
                    skGlyph->glyphRect().offset(SkScalarRoundToInt(x), SkScalarRoundToInt(y));
            runBounds = skglyph::rect_union(runBounds, deviceBounds);
            glyphLeftTop[goodPosCount] = deviceBounds.topLeft();
            glyphIDs[goodPosCount].packedGlyphID = skGlyph->getPackedID();
            goodPosCount += 1;
        }
    }

    // Wow! no glyphs are in bounds and had non-empty bounds.
    if (goodPosCount == 0) {
        return nullptr;
    }

    SkSpan<const DevicePosition> leftTop{glyphLeftTop, goodPosCount};
    return alloc->makeUnique<DirectMaskSubRunSlug>(
            referenceFrame, format, runBounds, leftTop,
            GrGlyphVector{std::move(strike), {glyphIDs, goodPosCount}});
}

template <typename T>
static bool pun_read(SkReadBuffer& buffer, T* dst) {
    return buffer.readPad32(dst, sizeof(T));
}

GrSubRunOwner DirectMaskSubRunSlug::MakeFromBuffer(const GrTextReferenceFrame* referenceFrame,
                                                   SkReadBuffer& buffer,
                                                   GrSubRunAllocator* alloc,
                                                   const SkStrikeClient*) {

    GrMaskFormat format = (GrMaskFormat)buffer.readInt();
    SkGlyphRect runBounds;
    pun_read(buffer, &runBounds);

    int glyphCount = buffer.readInt();
    SkASSERT(0 < glyphCount);
    if (glyphCount <= 0) { return nullptr; }
    DevicePosition* positionsData = alloc->makePODArray<DevicePosition>(glyphCount);
    for (int i = 0; i < glyphCount; ++i) {
        pun_read(buffer, &positionsData[i]);
    }
    SkSpan<DevicePosition> positions(positionsData, glyphCount);

    auto glyphVector = GrGlyphVector::MakeFromBuffer(buffer, alloc);
    SkASSERT(glyphVector.has_value());
    if (!glyphVector) { return nullptr; }
    SkASSERT(SkTo<int>(glyphVector->glyphs().size()) == glyphCount);
    if (SkTo<int>(glyphVector->glyphs().size()) != glyphCount) { return nullptr; }
    return alloc->makeUnique<DirectMaskSubRunSlug>(
            referenceFrame, format, runBounds, positions, std::move(glyphVector.value()));
}

template <typename T>
static void pun_write(SkWriteBuffer& buffer, const T& src) {
    buffer.writePad32(&src, sizeof(T));
}

void DirectMaskSubRunSlug::doFlatten(SkWriteBuffer& buffer) const {
    buffer.writeInt(fMaskFormat);
    pun_write(buffer, fGlyphDeviceBounds);
    int glyphCount = SkTo<int>(fLeftTopDevicePos.size());
    buffer.writeInt(glyphCount);
    for (auto pos : fLeftTopDevicePos) {
        pun_write(buffer, pos);
    }
    fGlyphs.flatten(buffer);
}

int DirectMaskSubRunSlug::unflattenSize() const {
    return sizeof(DirectMaskSubRunSlug) +
           fGlyphs.unflattenSize() +
           sizeof(DevicePosition) * fGlyphs.glyphs().size();
}

size_t DirectMaskSubRunSlug::vertexStride(const SkMatrix& positionMatrix) const {
    if (!positionMatrix.hasPerspective()) {
        if (fMaskFormat != kARGB_GrMaskFormat) {
            return sizeof(Mask2DVertex);
        } else {
            return sizeof(ARGB2DVertex);
        }
    } else {
        if (fMaskFormat != kARGB_GrMaskFormat) {
            return sizeof(Mask3DVertex);
        } else {
            return sizeof(ARGB3DVertex);
        }
    }
}

int DirectMaskSubRunSlug::glyphCount() const {
    return SkCount(fGlyphs.glyphs());
}

std::tuple<const GrClip*, GrOp::Owner>
DirectMaskSubRunSlug::makeAtlasTextOp(const GrClip* clip,
                                      const SkMatrixProvider& viewMatrix,
                                      SkPoint drawOrigin,
                                      const SkPaint& paint,
                                      skgpu::v1::SurfaceDrawContext* sdc,
                                      GrAtlasSubRunOwner subRunOwner) const {
    SkASSERT(this->glyphCount() != 0);
    const SkMatrix& drawMatrix = viewMatrix.localToDevice();
    const SkMatrix& positionMatrix = position_matrix(drawMatrix, drawOrigin);

    auto [integerTranslate, subRunDeviceBounds] = this->deviceRectAndCheckTransform(positionMatrix);
    if (subRunDeviceBounds.isEmpty()) {
        return {nullptr, nullptr};
    }
    // Rect for optimized bounds clipping when doing an integer translate.
    SkIRect geometricClipRect = SkIRect::MakeEmpty();
    if (integerTranslate) {
        // We can clip geometrically using clipRect and ignore clip when an axis-aligned rectangular
        // non-AA clip is used. If clipRect is empty, and clip is nullptr, then there is no clipping
        // needed.
        const SkRect deviceBounds = SkRect::MakeWH(sdc->width(), sdc->height());
        auto [clipMethod, clipRect] = calculate_clip(clip, deviceBounds, subRunDeviceBounds);

        switch (clipMethod) {
            case kClippedOut:
                // Returning nullptr as op means skip this op.
                return {nullptr, nullptr};
            case kUnclipped:
            case kGeometryClipped:
                // GPU clip is not needed.
                clip = nullptr;
                break;
            case kGPUClipped:
                // Use th GPU clip; clipRect is ignored.
                break;
        }
        geometricClipRect = clipRect;

        if (!geometricClipRect.isEmpty()) { SkASSERT(clip == nullptr); }
    }

    GrPaint grPaint;
    const SkPMColor4f drawingColor =
            calculate_colors(sdc, paint, viewMatrix, fMaskFormat, &grPaint);

    auto geometry = AtlasTextOp::Geometry::MakeForBlob(*this,
                                                       drawMatrix,
                                                       drawOrigin,
                                                       geometricClipRect,
                                                       sk_ref_sp(fReferenceFrame),
                                                       drawingColor,
                                                       sdc->arenaAlloc());

    GrRecordingContext* const rContext = sdc->recordingContext();
    GrOp::Owner op = GrOp::Make<AtlasTextOp>(rContext,
                                             op_mask_type(fMaskFormat),
                                             !integerTranslate,
                                             this->glyphCount(),
                                             subRunDeviceBounds,
                                             geometry,
                                             std::move(grPaint));
    return {clip, std::move(op)};
}

void DirectMaskSubRunSlug::testingOnly_packedGlyphIDToGrGlyph(GrStrikeCache *cache) const {
    fGlyphs.packedGlyphIDToGrGlyph(cache);
}

std::tuple<bool, int>
DirectMaskSubRunSlug::regenerateAtlas(int begin, int end, GrMeshDrawTarget* target) const {
    return fGlyphs.regenerateAtlas(begin, end, fMaskFormat, 1, target, true);
}

// The 99% case. No clip. Non-color only.
void direct_2D3(SkZip<Mask2DVertex[4],
        const GrGlyph*,
        const DirectMaskSubRunNoCache::DevicePosition> quadData,
                GrColor color,
                SkPoint originOffset) {
    for (auto[quad, glyph, leftTop] : quadData) {
        auto[al, at, ar, ab] = glyph->fAtlasLocator.getUVs();
        SkScalar dl = leftTop[0] + originOffset.x(),
                 dt = leftTop[1] + originOffset.y(),
                 dr = dl + (ar - al),
                 db = dt + (ab - at);

        quad[0] = {{dl, dt}, color, {al, at}};  // L,T
        quad[1] = {{dl, db}, color, {al, ab}};  // L,B
        quad[2] = {{dr, dt}, color, {ar, at}};  // R,T
        quad[3] = {{dr, db}, color, {ar, ab}};  // R,B
    }
}

template<typename Quad, typename VertexData>
void transformed_direct_2D(SkZip<Quad, const GrGlyph*, const VertexData> quadData,
                           GrColor color,
                           const SkMatrix& matrix) {
    for (auto[quad, glyph, leftTop] : quadData) {
        auto[al, at, ar, ab] = glyph->fAtlasLocator.getUVs();
        SkScalar dl = leftTop[0],
                 dt = leftTop[1],
                 dr = dl + (ar - al),
                 db = dt + (ab - at);
        SkPoint lt = matrix.mapXY(dl, dt),
                lb = matrix.mapXY(dl, db),
                rt = matrix.mapXY(dr, dt),
                rb = matrix.mapXY(dr, db);
        quad[0] = {lt, color, {al, at}};  // L,T
        quad[1] = {lb, color, {al, ab}};  // L,B
        quad[2] = {rt, color, {ar, at}};  // R,T
        quad[3] = {rb, color, {ar, ab}};  // R,B
    }
}

template<typename Quad, typename VertexData>
void transformed_direct_3D(SkZip<Quad, const GrGlyph*, const VertexData> quadData,
                           GrColor color,
                           const SkMatrix& matrix) {
    auto mapXYZ = [&](SkScalar x, SkScalar y) {
        SkPoint pt{x, y};
        SkPoint3 result;
        matrix.mapHomogeneousPoints(&result, &pt, 1);
        return result;
    };
    for (auto[quad, glyph, leftTop] : quadData) {
        auto[al, at, ar, ab] = glyph->fAtlasLocator.getUVs();
        SkScalar dl = leftTop[0],
                 dt = leftTop[1],
                 dr = dl + (ar - al),
                 db = dt + (ab - at);
        SkPoint3 lt = mapXYZ(dl, dt),
                 lb = mapXYZ(dl, db),
                 rt = mapXYZ(dr, dt),
                 rb = mapXYZ(dr, db);
        quad[0] = {lt, color, {al, at}};  // L,T
        quad[1] = {lb, color, {al, ab}};  // L,B
        quad[2] = {rt, color, {ar, at}};  // R,T
        quad[3] = {rb, color, {ar, ab}};  // R,B
    }
}

void DirectMaskSubRunSlug::fillVertexData(void* vertexDst, int offset, int count,
                                          GrColor color,
                                          const SkMatrix& drawMatrix, SkPoint drawOrigin,
                                          SkIRect clip) const {
    auto quadData = [&](auto dst) {
        return SkMakeZip(dst,
                         fGlyphs.glyphs().subspan(offset, count),
                         fLeftTopDevicePos.subspan(offset, count));
    };

    const SkMatrix positionMatrix = position_matrix(drawMatrix, drawOrigin);
    auto [noTransformNeeded, originOffset] =
            can_use_direct(fReferenceFrame->initialPositionMatrix(), positionMatrix);

    if (noTransformNeeded) {
        if (clip.isEmpty()) {
            if (fMaskFormat != kARGB_GrMaskFormat) {
                using Quad = Mask2DVertex[4];
                SkASSERT(sizeof(Mask2DVertex) == this->vertexStride(SkMatrix::I()));
                direct_2D3(quadData((Quad*)vertexDst), color, originOffset);
            } else {
                using Quad = ARGB2DVertex[4];
                SkASSERT(sizeof(ARGB2DVertex) == this->vertexStride(SkMatrix::I()));
                generalized_direct_2D(quadData((Quad*)vertexDst), color, originOffset);
            }
        } else {
            if (fMaskFormat != kARGB_GrMaskFormat) {
                using Quad = Mask2DVertex[4];
                SkASSERT(sizeof(Mask2DVertex) == this->vertexStride(SkMatrix::I()));
                generalized_direct_2D(quadData((Quad*)vertexDst), color, originOffset, &clip);
            } else {
                using Quad = ARGB2DVertex[4];
                SkASSERT(sizeof(ARGB2DVertex) == this->vertexStride(SkMatrix::I()));
                generalized_direct_2D(quadData((Quad*)vertexDst), color, originOffset, &clip);
            }
        }
    } else if (SkMatrix inverse; fReferenceFrame->initialPositionMatrix().invert(&inverse)) {
        SkMatrix viewDifference = SkMatrix::Concat(positionMatrix, inverse);
        if (!viewDifference.hasPerspective()) {
            if (fMaskFormat != kARGB_GrMaskFormat) {
                using Quad = Mask2DVertex[4];
                SkASSERT(sizeof(Mask2DVertex) == this->vertexStride(positionMatrix));
                transformed_direct_2D(quadData((Quad*)vertexDst), color, viewDifference);
            } else {
                using Quad = ARGB2DVertex[4];
                SkASSERT(sizeof(ARGB2DVertex) == this->vertexStride(positionMatrix));
                transformed_direct_2D(quadData((Quad*)vertexDst), color, viewDifference);
            }
        } else {
            if (fMaskFormat != kARGB_GrMaskFormat) {
                using Quad = Mask3DVertex[4];
                SkASSERT(sizeof(Mask3DVertex) == this->vertexStride(positionMatrix));
                transformed_direct_3D(quadData((Quad*)vertexDst), color, viewDifference);
            } else {
                using Quad = ARGB3DVertex[4];
                SkASSERT(sizeof(ARGB3DVertex) == this->vertexStride(positionMatrix));
                transformed_direct_3D(quadData((Quad*)vertexDst), color, viewDifference);
            }
        }
    }
}

// true if only need to translate by integer amount, device rect.
std::tuple<bool, SkRect>
DirectMaskSubRunSlug::deviceRectAndCheckTransform(const SkMatrix& positionMatrix) const {
    SkPoint offset =
            positionMatrix.mapOrigin() - fReferenceFrame->initialPositionMatrix().mapOrigin();
    if (positionMatrix.isTranslate() && SkScalarIsInt(offset.x()) && SkScalarIsInt(offset.y())) {
        // Handle the integer offset case.
        // The offset should be integer, but make sure.
        SkIVector iOffset = {SkScalarRoundToInt(offset.x()), SkScalarRoundToInt(offset.y())};

        SkIRect outBounds = fGlyphDeviceBounds.iRect();
        return {true, SkRect::Make(outBounds.makeOffset(iOffset))};
    } else if (SkMatrix inverse; fReferenceFrame->initialPositionMatrix().invert(&inverse)) {
        SkMatrix viewDifference = SkMatrix::Concat(positionMatrix, inverse);
        return {false, viewDifference.mapRect(fGlyphDeviceBounds.rect())};
    }

    // initialPositionMatrix is singular. Do nothing.
    return {false, SkRect::MakeEmpty()};
}

void Slug::processDeviceMasks(
        const SkZip<SkGlyphVariant, SkPoint>& accepted, sk_sp<SkStrike>&& strike) {
    auto addGlyphsWithSameFormat = [&] (const SkZip<SkGlyphVariant, SkPoint>& accepted,
                                        GrMaskFormat format,
                                        sk_sp<SkStrike>&& runStrike) {
        GrSubRunOwner subRun = DirectMaskSubRunSlug::Make(
                this, accepted, std::move(runStrike), format, &fAlloc);
        if (subRun != nullptr) {
            fSubRuns.append(std::move(subRun));
        }
    };

    add_multi_mask_format(addGlyphsWithSameFormat, accepted, std::move(strike));
}

sk_sp<Slug> Slug::Make(const SkMatrixProvider& viewMatrix,
                       const SkGlyphRunList& glyphRunList,
                       const SkPaint& paint,
                       const GrSDFTControl& control,
                       SkGlyphRunListPainter* painter) {
    // The difference in alignment from the per-glyph data to the SubRun;
    constexpr size_t alignDiff =
            alignof(DirectMaskSubRun) - alignof(DirectMaskSubRun::DevicePosition);
    constexpr size_t vertexDataToSubRunPadding = alignDiff > 0 ? alignDiff : 0;
    size_t totalGlyphCount = glyphRunList.totalGlyphCount();
    // The bytesNeededForSubRun is optimized for DirectMaskSubRun which is by far the most
    // common case.
    size_t bytesNeededForSubRun = GrBagOfBytes::PlatformMinimumSizeWithOverhead(
            totalGlyphCount * sizeof(DirectMaskSubRunSlug::DevicePosition)
            + GrGlyphVector::GlyphVectorSize(totalGlyphCount)
            + glyphRunList.runCount() * (sizeof(DirectMaskSubRunSlug) + vertexDataToSubRunPadding),
            alignof(Slug));

    size_t allocationSize = sizeof(GrTextBlob) + bytesNeededForSubRun;

    const SkMatrix positionMatrix =
            position_matrix(viewMatrix.localToDevice(), glyphRunList.origin());

    sk_sp<Slug> slug{new (::operator new (allocationSize))
                             Slug(glyphRunList.sourceBounds(),
                                  paint,
                                  positionMatrix,
                                  glyphRunList.origin(),
                                  bytesNeededForSubRun)};

    const uint64_t uniqueID = glyphRunList.uniqueID();
    for (auto& glyphRun : glyphRunList) {
        painter->processGlyphRun(slug.get(),
                                 glyphRun,
                                 positionMatrix,
                                 paint,
                                 control,
                                 "Make Slug",
                                 uniqueID);
    }

    // There is nothing to draw here. This is particularly a problem with RSX form blobs where a
    // single space becomes a run with no glyphs.
    if (slug->fSubRuns.isEmpty()) { return nullptr; }

    return slug;
}

void Slug::processSourcePaths(const SkZip<SkGlyphVariant,
                              SkPoint>& accepted,
                              const SkFont& runFont,
                              SkScalar strikeToSourceScale) {
    fSubRuns.append(PathSubRun::Make(
            accepted, has_some_antialiasing(runFont), strikeToSourceScale, &fAlloc));
}

void Slug::processSourceDrawables(const SkZip<SkGlyphVariant, SkPoint>& accepted,
                                  const SkFont& runFont,
                                  SkScalar strikeToSourceScale) {
    fSubRuns.append(make_drawable_sub_run<DrawableSubRunSlug>(
            accepted, has_some_antialiasing(runFont), strikeToSourceScale, &fAlloc));
}

void Slug::processSourceSDFT(const SkZip<SkGlyphVariant, SkPoint>& accepted,
                                   sk_sp<SkStrike>&& strike,
                                   SkScalar strikeToSourceScale,
                                   const SkFont& runFont,
                                   const GrSDFTMatrixRange& matrixRange) {
    fSubRuns.append(SDFTSubRun::Make(
        this, accepted, runFont, std::move(strike), strikeToSourceScale, matrixRange, &fAlloc));
}

void Slug::processSourceMasks(const SkZip<SkGlyphVariant, SkPoint>& accepted,
                              sk_sp<SkStrike>&& strike,
                              SkScalar strikeToSourceScale) {

    auto addGlyphsWithSameFormat = [&] (const SkZip<SkGlyphVariant, SkPoint>& accepted,
                                        GrMaskFormat format,
                                        sk_sp<SkStrike>&& runStrike) {
        GrSubRunOwner subRun = TransformedMaskSubRun::Make(
                this, accepted, std::move(runStrike), strikeToSourceScale, format, &fAlloc);
        if (subRun != nullptr) {
            fSubRuns.append(std::move(subRun));
        }
    };

    add_multi_mask_format(addGlyphsWithSameFormat, accepted, std::move(strike));
}
}  // namespace

namespace skgpu::v1 {
sk_sp<GrSlug>
Device::convertGlyphRunListToSlug(const SkGlyphRunList& glyphRunList, const SkPaint& paint) {
    return fSurfaceDrawContext->convertGlyphRunListToSlug(
            this->asMatrixProvider(), glyphRunList, paint);
}

void Device::drawSlug(SkCanvas* canvas, GrSlug* slug) {
    fSurfaceDrawContext->drawSlug(canvas, this->clip(), this->asMatrixProvider(), slug);
}

sk_sp<GrSlug>
SurfaceDrawContext::convertGlyphRunListToSlug(const SkMatrixProvider& viewMatrix,
                                              const SkGlyphRunList& glyphRunList,
                                              const SkPaint& paint) {
    SkASSERT(fContext->priv().options().fSupportBilerpFromGlyphAtlas);

    GrSDFTControl control =
            this->recordingContext()->priv().getSDFTControl(
                    this->surfaceProps().isUseDeviceIndependentFonts());

    return Slug::Make(viewMatrix, glyphRunList, paint, control, &fGlyphPainter);
}

void SurfaceDrawContext::drawSlug(SkCanvas* canvas,
                                  const GrClip* clip,
                                  const SkMatrixProvider& viewMatrix,
                                  GrSlug* slugPtr) {
    Slug* slug = static_cast<Slug*>(slugPtr);

    slug->surfaceDraw(canvas, clip, viewMatrix, this);
}

sk_sp<GrSlug> MakeSlug(const SkMatrixProvider& drawMatrix,
                       const SkGlyphRunList& glyphRunList,
                       const SkPaint& paint,
                       const GrSDFTControl& control,
                       SkGlyphRunListPainter* painter) {
    return Slug::Make(drawMatrix, glyphRunList, paint, control, painter);
}
}  // namespace skgpu::v1

// -- GrSubRun -------------------------------------------------------------------------------------
void GrSubRun::flatten(SkWriteBuffer& buffer) const {
    buffer.writeInt(this->subRunType());
    this->doFlatten(buffer);
}

GrSubRunOwner GrSubRun::MakeFromBuffer(const GrTextReferenceFrame* referenceFrame,
                                       SkReadBuffer& buffer,
                                       GrSubRunAllocator* alloc,
                                       const SkStrikeClient* client) {
    using Maker = GrSubRunOwner (*)(const GrTextReferenceFrame*,
                                    SkReadBuffer&,
                                    GrSubRunAllocator*,
                                    const SkStrikeClient*);

    /* The makers will be populated in the next CL. */
    static Maker makers[kSubRunTypeCount] = {
            nullptr,                                             // 0 index is bad.
            DirectMaskSubRunSlug::MakeFromBuffer,
            SDFTSubRun::MakeFromBuffer,
            TransformedMaskSubRun::MakeFromBuffer,
            PathSubRun::MakeFromBuffer,
            DrawableSubRunSlug::MakeFromBuffer,
    };
    int subRunTypeInt = buffer.readInt();
    SkASSERT(kBad < subRunTypeInt && subRunTypeInt < kSubRunTypeCount);
    if (!buffer.validate(kBad < subRunTypeInt && subRunTypeInt < kSubRunTypeCount)) {
        return nullptr;
    }
    auto maker = makers[subRunTypeInt];
    if (!buffer.validate(maker != nullptr)) { return nullptr; }
    return maker(referenceFrame, buffer, alloc, client);
}

sk_sp<GrSlug> SkMakeSlugFromBuffer(SkReadBuffer& buffer, const SkStrikeClient* client) {
    return Slug::MakeFromBuffer(buffer, client);
}
