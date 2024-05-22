/*
 * Copyright 2017 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef GrColorInfo_DEFINED
#define GrColorInfo_DEFINED

#include "include/core/SkColorSpace.h"
#include "include/core/SkRefCnt.h"
#include "include/gpu/GrTypes.h"
#include "src/gpu/GrColorSpaceXform.h"

/**
 * All the info needed to interpret a color: Color type + alpha type + color space. Also caches
 * the GrColorSpaceXform from sRGB. */
class GrColorInfo {
public:
    GrColorInfo() = default;
    GrColorInfo(const GrColorInfo&);
    GrColorInfo& operator=(const GrColorInfo&);
    GrColorInfo(GrColorType, SkAlphaType, sk_sp<SkColorSpace>);
    /* implicit */ GrColorInfo(const SkColorInfo&);

    bool operator==(const GrColorInfo& that) const {
        return  fColorType == that.fColorType &&
                fAlphaType == that.fAlphaType &&
                SkColorSpace::Equals(fColorSpace.get(), that.fColorSpace.get());
    }
    bool operator!=(const GrColorInfo& that) const { return !(*this == that); }

    GrColorInfo makeColorType(GrColorType ct) const {
        return GrColorInfo(ct, fAlphaType, this->refColorSpace());
    }

    bool isLinearlyBlended() const { return fColorSpace && fColorSpace->gammaIsLinear(); }

    SkColorSpace* colorSpace() const { return fColorSpace.get(); }
    sk_sp<SkColorSpace> refColorSpace() const { return fColorSpace; }

    GrColorSpaceXform* colorSpaceXformFromSRGB() const { return fColorXformFromSRGB.get(); }
    sk_sp<GrColorSpaceXform> refColorSpaceXformFromSRGB() const { return fColorXformFromSRGB; }

    GrColorType colorType() const { return fColorType; }
    SkAlphaType alphaType() const { return fAlphaType; }

    bool isAlphaOnly() const { return GrColorTypeIsAlphaOnly(fColorType); }

    bool isValid() const {
        return fColorType != GrColorType::kUnknown && fAlphaType != kUnknown_SkAlphaType;
    }

private:
    sk_sp<SkColorSpace> fColorSpace;
    sk_sp<GrColorSpaceXform> fColorXformFromSRGB;
    GrColorType fColorType = GrColorType::kUnknown;
    SkAlphaType fAlphaType = kUnknown_SkAlphaType;
};

#endif
