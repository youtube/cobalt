/*
 * Copyright 2006 The Android Open Source Project
 *
 * Use of this source code is governed by a BSD-style license that can be found in the LICENSE file.
 */

#ifndef SkStrike_DEFINED
#define SkStrike_DEFINED

#include "include/core/SkFontMetrics.h"
#include "include/core/SkFontTypes.h"
#include "include/private/SkMutex.h"
#include "include/private/SkTHash.h"
#include "include/private/SkTemplates.h"
#include "src/core/SkArenaAlloc.h"
#include "src/core/SkDescriptor.h"
#include "src/core/SkGlyph.h"
#include "src/core/SkGlyphRunPainter.h"
#include "src/core/SkStrikeForGPU.h"
#include <memory>

class SkScalerContext;

// This class represents a strike: a specific combination of typeface, size, matrix, etc., and
// holds the glyphs for that strike.
class SkScalerCache {
public:
    SkScalerCache(std::unique_ptr<SkScalerContext> scaler,
                  const SkFontMetrics* metrics = nullptr);

    // Lookup (or create if needed) the toGlyph using toID. If that glyph is not initialized with
    // an image, then use the information in from to initialize the width, height top, left,
    // format and image of the toGlyph. This is mainly used preserving the glyph if it was
    // created by a search of desperation.
    std::tuple<SkGlyph*, size_t> mergeGlyphAndImage(
            SkPackedGlyphID toID, const SkGlyph& from) SK_EXCLUDES(fMu);

    // If the path has never been set, then add a path to glyph.
    std::tuple<const SkPath*, size_t> mergePath(
            SkGlyph* glyph, const SkPath* path, bool hairline) SK_EXCLUDES(fMu);

    // If the drawable has never been set, then add a drawble to glyph.
    std::tuple<SkDrawable*, size_t> mergeDrawable(
            SkGlyph* glyph, sk_sp<SkDrawable> drawable) SK_EXCLUDES(fMu);

    /** Return the number of glyphs currently cached. */
    int countCachedGlyphs() const SK_EXCLUDES(fMu);

    /** If the advance axis intersects the glyph's path, append the positions scaled and offset
        to the array (if non-null), and set the count to the updated array length.
    */
    void findIntercepts(const SkScalar bounds[2], SkScalar scale, SkScalar xPos,
                        SkGlyph* , SkScalar* array, int* count) SK_EXCLUDES(fMu);

    const SkFontMetrics& getFontMetrics() const {
        return fFontMetrics;
    }

    std::tuple<SkSpan<const SkGlyph*>, size_t> metrics(
            SkSpan<const SkGlyphID> glyphIDs, const SkGlyph* results[]) SK_EXCLUDES(fMu);

    std::tuple<SkSpan<const SkGlyph*>, size_t> preparePaths(
            SkSpan<const SkGlyphID> glyphIDs, const SkGlyph* results[]) SK_EXCLUDES(fMu);

    std::tuple<SkSpan<const SkGlyph*>, size_t> prepareImages(
            SkSpan<const SkPackedGlyphID> glyphIDs, const SkGlyph* results[]) SK_EXCLUDES(fMu);

    size_t prepareForDrawingMasksCPU(SkDrawableGlyphBuffer* accepted) SK_EXCLUDES(fMu);

    // SkStrikeForGPU APIs
    const SkGlyphPositionRoundingSpec& roundingSpec() const {
        return fRoundingSpec;
    }

    size_t prepareForMaskDrawing(
            SkDrawableGlyphBuffer* accepted, SkSourceGlyphBuffer* rejected) SK_EXCLUDES(fMu);

    size_t prepareForSDFTDrawing(
            SkDrawableGlyphBuffer* accepted, SkSourceGlyphBuffer* rejected) SK_EXCLUDES(fMu);

    size_t prepareForPathDrawing(
            SkDrawableGlyphBuffer* accepted, SkSourceGlyphBuffer* rejected) SK_EXCLUDES(fMu);

    size_t prepareForDrawableDrawing(
            SkDrawableGlyphBuffer* accepted, SkSourceGlyphBuffer* rejected) SK_EXCLUDES(fMu);

    void dump() const SK_EXCLUDES(fMu);

    SkScalerContext* getScalerContext() const { return fScalerContext.get(); }

private:
    template <typename Fn>
    size_t commonFilterLoop(SkDrawableGlyphBuffer* accepted, Fn&& fn) SK_REQUIRES(fMu);

    // Return a glyph. Create it if it doesn't exist, and initialize the glyph with metrics and
    // advances using a scaler.
    std::tuple<SkGlyph*, size_t> glyph(SkPackedGlyphID) SK_REQUIRES(fMu);

    std::tuple<SkGlyphDigest, size_t> digest(SkPackedGlyphID) SK_REQUIRES(fMu);

    // Generate the glyph digest information and update structures to add the glyph.
    SkGlyphDigest addGlyph(SkGlyph* glyph) SK_REQUIRES(fMu);

    std::tuple<const void*, size_t> prepareImage(SkGlyph* glyph) SK_REQUIRES(fMu);

    // If the path has never been set, then use the scaler context to add the glyph.
    std::tuple<const SkPath*, size_t> preparePath(SkGlyph*) SK_REQUIRES(fMu);

    // If the drawable has never been set, then use the scaler context to add the glyph.
    std::tuple<SkDrawable*, size_t> prepareDrawable(SkGlyph*) SK_REQUIRES(fMu);

    enum PathDetail {
        kMetricsOnly,
        kMetricsAndPath
    };

    // internalPrepare will only be called with a mutex already held.
    std::tuple<SkSpan<const SkGlyph*>, size_t> internalPrepare(
            SkSpan<const SkGlyphID> glyphIDs,
            PathDetail pathDetail,
            const SkGlyph** results) SK_REQUIRES(fMu);

    const std::unique_ptr<SkScalerContext> fScalerContext;
    const SkFontMetrics                    fFontMetrics;
    const SkGlyphPositionRoundingSpec      fRoundingSpec;

    mutable SkMutex fMu;

    // Map from a combined GlyphID and sub-pixel position to a SkGlyphDigest. The actual glyph is
    // stored in the fAlloc. The pointer to the glyph is stored fGlyphForIndex. The
    // SkGlyphDigest's fIndex field stores the index. This pointer provides an unchanging
    // reference to the SkGlyph as long as the strike is alive, and fGlyphForIndex
    // provides a dense index for glyphs.
    SkTHashTable<SkGlyphDigest, uint32_t, SkGlyphDigest> fDigestForPackedGlyphID SK_GUARDED_BY(fMu);
    std::vector<SkGlyph*> fGlyphForIndex SK_GUARDED_BY(fMu);

    // so we don't grow our arrays a lot
    inline static constexpr size_t kMinGlyphCount = 8;
    inline static constexpr size_t kMinGlyphImageSize = 16 /* height */ * 8 /* width */;
    inline static constexpr size_t kMinAllocAmount = kMinGlyphImageSize * kMinGlyphCount;

    SkArenaAlloc            fAlloc SK_GUARDED_BY(fMu) {kMinAllocAmount};
};

#endif  // SkStrike_DEFINED
