/*
 * Copyright 2015 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef GrTextBlob_DEFINED
#define GrTextBlob_DEFINED

#include <algorithm>
#include <limits>

#include "include/core/SkPoint3.h"
#include "include/core/SkRefCnt.h"
#include "include/private/chromium/GrSlug.h"
#include "src/core/SkGlyphRunPainter.h"
#include "src/core/SkIPoint16.h"
#include "src/core/SkMaskFilterBase.h"
#include "src/core/SkOpts.h"
#include "src/core/SkRectPriv.h"
#include "src/core/SkStrikeSpec.h"
#include "src/core/SkTInternalLList.h"
#include "src/core/SkTLazy.h"
#include "src/gpu/GrColor.h"
#include "src/gpu/GrSubRunAllocator.h"
#include "src/gpu/ops/GrOp.h"

class GrAtlasManager;
class GrDeferredUploadTarget;
class GrGlyph;
class GrMeshDrawTarget;
class GrStrikeCache;
class GrSubRun;

class SkMatrixProvider;
class SkStrikeClient;
class SkSurfaceProps;
class SkTextBlob;
class SkTextBlobRunIterator;

namespace skgpu::v1 { class SurfaceDrawContext; }

// -- GrAtlasSubRun --------------------------------------------------------------------------------
// GrAtlasSubRun is the API that AtlasTextOp uses to generate vertex data for drawing.
//     There are three different ways GrAtlasSubRun is specialized.
//      * DirectMaskSubRun* - this is by far the most common type of SubRun. The mask pixels are
//        in 1:1 correspondence with the pixels on the device. The destination rectangles in this
//        SubRun are in device space. This SubRun handles color glyphs.
//      * TransformedMaskSubRun* - handles glyph where the image in the atlas needs to be
//        transformed to the screen. It is usually used for large color glyph which can't be
//        drawn with paths or scaled distance fields, but will be used to draw bitmap glyphs to
//        the screen, if the matrix does not map 1:1 to the screen. The destination rectangles
//        are in source space.
//      * SDFTSubRun* - scaled distance field text handles largish single color glyphs that still
//        can fit in the atlas; the sizes between direct SubRun, and path SubRun. The destination
//        rectangles are in source space.
class GrAtlasSubRun;
using GrAtlasSubRunOwner = std::unique_ptr<GrAtlasSubRun, GrSubRunAllocator::Destroyer>;
class GrAtlasSubRun  {
public:
    virtual ~GrAtlasSubRun() = default;

    virtual size_t vertexStride(const SkMatrix& drawMatrix) const = 0;
    virtual int glyphCount() const = 0;

    virtual std::tuple<const GrClip*, GrOp::Owner>
    makeAtlasTextOp(
            const GrClip*,
            const SkMatrixProvider& viewMatrix,
            SkPoint drawOrigin,
            const SkPaint&,
            skgpu::v1::SurfaceDrawContext*,
            GrAtlasSubRunOwner subRun) const = 0;

    virtual void fillVertexData(
            void* vertexDst, int offset, int count,
            GrColor color,
            const SkMatrix& drawMatrix,
            SkPoint drawOrigin,
            SkIRect clip) const = 0;

    virtual void testingOnly_packedGlyphIDToGrGlyph(GrStrikeCache* cache) const = 0;

    // This call is not thread safe. It should only be called from GrDrawOp::onPrepare which
    // is single threaded.
    virtual std::tuple<bool, int> regenerateAtlas(
            int begin, int end, GrMeshDrawTarget* target) const = 0;
};

// -- GrSubRun -------------------------------------------------------------------------------------
// GrSubRun defines the most basic functionality of a SubRun; the ability to draw, and the
// ability to be in a list.
class GrSubRun;
using GrSubRunOwner = std::unique_ptr<GrSubRun, GrSubRunAllocator::Destroyer>;
class GrBlobSubRun;
class GrSubRun {
public:
    virtual ~GrSubRun();
    // Produce GPU ops for this subRun or just draw them.
    virtual void draw(SkCanvas*,
                      const GrClip*,
                      const SkMatrixProvider& viewMatrix,
                      SkPoint drawOrigin,
                      const SkPaint&,
                      skgpu::v1::SurfaceDrawContext*) const = 0;

    virtual const GrBlobSubRun* blobCast() const;
    void flatten(SkWriteBuffer& buffer) const;
    static GrSubRunOwner MakeFromBuffer(const GrTextReferenceFrame* referenceFrame,
                                        SkReadBuffer& buffer,
                                        GrSubRunAllocator* alloc,
                                        const SkStrikeClient* client);

    // Size hint for unflattening this run. If this is accurate, it will help with the allocation
    // of the slug. If it's off then there may be more allocations needed to unflatten.
    virtual int unflattenSize() const = 0;

protected:
    enum SubRunType : int;
    virtual SubRunType subRunType() const = 0;
    virtual void doFlatten(SkWriteBuffer& buffer) const = 0;

private:
    friend class GrSubRunList;
    GrSubRunOwner fNext;
};

// -- GrSubRunList ---------------------------------------------------------------------------------
class GrSubRunList {
public:
    class Iterator {
    public:
        using value_type = GrSubRun;
        using difference_type = ptrdiff_t;
        using pointer = value_type*;
        using reference = value_type&;
        using iterator_category = std::input_iterator_tag;
        Iterator(GrSubRun* subRun) : fPtr{subRun} { }
        Iterator& operator++() { fPtr = fPtr->fNext.get(); return *this; }
        Iterator operator++(int) { Iterator tmp(*this); operator++(); return tmp; }
        bool operator==(const Iterator& rhs) const { return fPtr == rhs.fPtr; }
        bool operator!=(const Iterator& rhs) const { return fPtr != rhs.fPtr; }
        reference operator*() { return *fPtr; }

    private:
        GrSubRun* fPtr;
    };

    void append(GrSubRunOwner subRun) {
        GrSubRunOwner* newTail = &subRun->fNext;
        *fTail = std::move(subRun);
        fTail = newTail;
    }
    bool isEmpty() const { return fHead == nullptr; }
    Iterator begin() { return Iterator{ fHead.get()}; }
    Iterator end() { return Iterator{nullptr}; }
    Iterator begin() const { return Iterator{ fHead.get()}; }
    Iterator end() const { return Iterator{nullptr}; }
    GrSubRun& front() const {return *fHead; }

private:
    GrSubRunOwner fHead{nullptr};
    GrSubRunOwner* fTail{&fHead};
};

// -- GrTextBlob -----------------------------------------------------------------------------------
// A GrTextBlob contains a fully processed SkTextBlob, suitable for nearly immediate drawing
// on the GPU.  These are initially created with valid positions and colors, but with invalid
// texture coordinates.
//
// A GrTextBlob contains a number of SubRuns that are created in the blob's arena. Each SubRun
// tracks its own GrGlyph* and vertex data. The memory is organized in the arena in the following
// way so that the pointers for the GrGlyph* and vertex data are known before creating the SubRun.
//
//  GrGlyph*... | vertexData... | SubRun | GrGlyph*... | vertexData... | SubRun  etc.
//
// In these classes, I'm trying to follow the convention about matrices and origins.
// * drawMatrix and drawOrigin    - describes transformations for the current draw command.
// * initial Matrix - describes the combined initial matrix and origin the GrTextBlob was created
//                    with.
//
//
class GrTextBlob final : public GrTextReferenceFrame, public SkGlyphRunPainterInterface {
public:
    // Key is not used as part of a hash map, so the hash is never taken. It's only used in a
    // list search using operator =().
    struct Key {
        static std::tuple<bool, Key> Make(const SkGlyphRunList& glyphRunList,
                                          const SkPaint& paint,
                                          const SkSurfaceProps& surfaceProps,
                                          const GrColorInfo& colorInfo,
                                          const SkMatrix& drawMatrix,
                                          const GrSDFTControl& control);
        uint32_t fUniqueID;
        // Color may affect the gamma of the mask we generate, but in a fairly limited way.
        // Each color is assigned to on of a fixed number of buckets based on its
        // luminance. For each luminance bucket there is a "canonical color" that
        // represents the bucket.  This functionality is currently only supported for A8
        SkColor fCanonicalColor;
        SkScalar fFrameWidth;
        SkScalar fMiterLimit;
        SkPixelGeometry fPixelGeometry;
        SkMaskFilterBase::BlurRec fBlurRec;
        uint32_t fScalerContextFlags;
        SkMatrix fPositionMatrix;
        // Below here fields are of size 1 byte.
        bool fHasSomeDirectSubRuns;
        bool fHasBlur;
        SkPaint::Style fStyle;
        SkPaint::Join fJoin;

        bool operator==(const Key& other) const;
    };

    SK_DECLARE_INTERNAL_LLIST_INTERFACE(GrTextBlob);

    // Make a GrTextBlob and its sub runs.
    static sk_sp<GrTextBlob> Make(const SkGlyphRunList& glyphRunList,
                                  const SkPaint& paint,
                                  const SkMatrix& positionMatrix,
                                  bool supportBilerpAtlas,
                                  const GrSDFTControl& control,
                                  SkGlyphRunListPainter* painter);

    ~GrTextBlob() override;

    // Change memory management to handle the data after GrTextBlob, but in the same allocation
    // of memory. Only allow placement new.
    void operator delete(void* p);
    void* operator new(size_t);
    void* operator new(size_t, void* p);

    const Key& key() { return fKey; }

    void addKey(const Key& key);
    bool hasPerspective() const;
    const SkMatrix& initialPositionMatrix() const override { return fInitialPositionMatrix; }
    bool supportBilerpAtlas() const { return fSupportBilerpAtlas; }

    bool canReuse(const SkPaint& paint, const SkMatrix& positionMatrix) const;

    const Key& key() const;
    size_t size() const;

    void draw(SkCanvas*,
              const GrClip* clip,
              const SkMatrixProvider& viewMatrix,
              SkPoint drawOrigin,
              const SkPaint& paint,
              skgpu::v1::SurfaceDrawContext* sdc);
    const GrAtlasSubRun* testingOnlyFirstSubRun() const;

private:
    GrTextBlob(int allocSize,
               bool supportBilerpAtlas,
               const SkMatrix& positionMatrix,
               SkColor initialLuminance);

    // Methods to satisfy SkGlyphRunPainterInterface
    void processDeviceMasks(const SkZip<SkGlyphVariant, SkPoint>& accepted,
                            sk_sp<SkStrike>&& strike) override;
    void processSourcePaths(const SkZip<SkGlyphVariant, SkPoint>& accepted,
                            const SkFont& runFont,
                            SkScalar strikeToSourceScale) override;
    void processSourceDrawables(const SkZip<SkGlyphVariant, SkPoint>& accepted,
                                const SkFont& runFont,
                                SkScalar strikeToSourceScale) override;
    void processSourceSDFT(const SkZip<SkGlyphVariant, SkPoint>& accepted,
                           sk_sp<SkStrike>&& strike,
                           SkScalar strikeToSourceScale,
                           const SkFont& runFont,
                           const GrSDFTMatrixRange& matrixRange) override;
    void processSourceMasks(const SkZip<SkGlyphVariant, SkPoint>& accepted,
                            sk_sp<SkStrike>&& strike,
                            SkScalar strikeToSourceScale) override;

    // The allocator must come first because it needs to be destroyed last. Other fields of this
    // structure may have pointers into it.
    GrSubRunAllocator fAlloc;

    // Owner and list of the SubRun.
    GrSubRunList fSubRunList;

    // Overall size of this struct plus vertices and glyphs at the end.
    const int fSize;

    // Support using bilerp for directly mapped sub runs.
    const bool fSupportBilerpAtlas;

    // The initial view matrix combined with the initial origin. Used to determine if a cached
    // subRun can be used in this draw situation.
    const SkMatrix fInitialPositionMatrix;

    const SkColor fInitialLuminance;

    Key fKey;

    bool fSomeGlyphsExcluded{false};
};

class GrSubRunNoCachePainter : public SkGlyphRunPainterInterface {
public:
    GrSubRunNoCachePainter(SkCanvas*,
                           skgpu::v1::SurfaceDrawContext*,
                           GrSubRunAllocator*,
                           const GrClip*,
                           const SkMatrixProvider& viewMatrix,
                           const SkGlyphRunList&,
                           const SkPaint&);
    void processDeviceMasks(const SkZip<SkGlyphVariant, SkPoint>& accepted,
                            sk_sp<SkStrike>&& strike) override;
    void processSourceMasks(const SkZip<SkGlyphVariant, SkPoint>& accepted,
                            sk_sp<SkStrike>&& strike,
                            SkScalar strikeToSourceScale) override;
    void processSourcePaths(const SkZip<SkGlyphVariant, SkPoint>& accepted,
                            const SkFont& runFont,
                            SkScalar strikeToSourceScale) override;
    void processSourceDrawables(const SkZip<SkGlyphVariant, SkPoint>& accepted,
                                const SkFont& runFont,
                                SkScalar strikeToSourceScale) override;
    void processSourceSDFT(const SkZip<SkGlyphVariant, SkPoint>& accepted,
                           sk_sp<SkStrike>&& strike,
                           SkScalar strikeToSourceScale,
                           const SkFont& runFont,
                           const GrSDFTMatrixRange& matrixRange) override;

private:

    // Draw passes ownership of the sub run to the op.
    void draw(GrAtlasSubRunOwner subRun);

    SkCanvas* fCanvas;
    skgpu::v1::SurfaceDrawContext* const fSDC;
    GrSubRunAllocator* const fAlloc;
    const GrClip* const fClip;
    const SkMatrixProvider& fViewMatrix;
    const SkGlyphRunList& fGlyphRunList;
    const SkPaint& fPaint;
};

namespace skgpu::v1 {
sk_sp<GrSlug> MakeSlug(const SkMatrixProvider& drawMatrix,
                       const SkGlyphRunList& glyphRunList,
                       const SkPaint& paint,
                       const GrSDFTControl& control,
                       SkGlyphRunListPainter* painter);
}  // namespace skgpu::v1
#endif  // GrTextBlob_DEFINED
