/*
 * Copyright 2010 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef GrGlyph_DEFINED
#define GrGlyph_DEFINED

#include "include/private/GrTypesPriv.h"
#include "src/core/SkGlyph.h"
#include "src/core/SkMask.h"
#include "src/gpu/GrDrawOpAtlas.h"

class GrGlyph {
public:
    static GrMaskFormat FormatFromSkGlyph(SkMask::Format format) {
        switch (format) {
            case SkMask::kBW_Format:
            case SkMask::kSDF_Format:
                // fall through to kA8 -- we store BW and SDF glyphs in our 8-bit cache
            case SkMask::kA8_Format:
                return kA8_GrMaskFormat;
            case SkMask::k3D_Format:
                return kA8_GrMaskFormat; // ignore the mul and add planes, just use the mask
            case SkMask::kLCD16_Format:
                return kA565_GrMaskFormat;
            case SkMask::kARGB32_Format:
                return kARGB_GrMaskFormat;
        }

        SkUNREACHABLE;
    }

<<<<<<< HEAD:third_party/skia/src/gpu/GrGlyph.h
    GrGlyph(SkPackedGlyphID packedGlyphID) : fPackedID(packedGlyphID) {}

    const SkPackedGlyphID       fPackedID;
    GrDrawOpAtlas::AtlasLocator fAtlasLocator;
=======
    Glyph(SkPackedGlyphID packedGlyphID, skgpu::MaskFormat format)
            : fPackedID(packedGlyphID)
            , fMaskFormat(format) {}

    const SkPackedGlyphID       fPackedID;
    const skgpu::MaskFormat     fMaskFormat;
    skgpu::AtlasLocator         fAtlasLocator;
>>>>>>> 2ce82b47d8 (BACKPORT: Make sure we are getting the correct atlas for glyph mask f… (#9520)):third_party/skia/src/text/gpu/Glyph.h
};

#endif
