/*
 * Copyright 2013 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "SkFontConfigInterface.h"
#include "SkFontHost_FreeType_common.h"
#include "SkStream.h"
#include "SkTypefaceCache.h"

class SkFontDescriptor;

class FontConfigTypeface : public SkTypeface_FreeType {
    SkFontConfigInterface::FontIdentity fIdentity;
    SkString fFamilyName;
    SkStreamAsset* fLocalStream;

public:
    static FontConfigTypeface* Create(Style style,
                                      const SkFontConfigInterface::FontIdentity& fi,
                                      const SkString& familyName) {
        return SkNEW_ARGS(FontConfigTypeface, (style, fi, familyName));
    }

    static FontConfigTypeface* Create(Style style, bool fixedWidth, SkStreamAsset* localStream) {
        return SkNEW_ARGS(FontConfigTypeface, (style, fixedWidth, localStream));
    }

    virtual ~FontConfigTypeface() {
        SkSafeUnref(fLocalStream);
    }

    const SkFontConfigInterface::FontIdentity& getIdentity() const {
        return fIdentity;
    }

    const char* getFamilyName() const { return fFamilyName.c_str(); }
    SkStreamAsset*   getLocalStream() const { return fLocalStream; }

    bool isFamilyName(const char* name) const {
        return fFamilyName.equals(name);
    }

    static SkTypeface* LegacyCreateTypeface(const SkTypeface* family,
                                            const char familyName[],
                                            SkTypeface::Style);

protected:
    friend class SkFontHost;    // hack until we can make public versions

    FontConfigTypeface(Style style,
                       const SkFontConfigInterface::FontIdentity& fi,
                       const SkString& familyName)
            : INHERITED(style, SkTypefaceCache::NewFontID(), false)
            , fIdentity(fi)
            , fFamilyName(familyName)
            , fLocalStream(NULL) {}

    FontConfigTypeface(Style style, bool fixedWidth, SkStreamAsset* localStream)
            : INHERITED(style, SkTypefaceCache::NewFontID(), fixedWidth) {
        // we default to empty fFamilyName and fIdentity
        fLocalStream = localStream;
        SkSafeRef(localStream);
    }

    virtual void onGetFamilyName(SkString* familyName) const SK_OVERRIDE;
    virtual void onGetFontDescriptor(SkFontDescriptor*, bool*) const SK_OVERRIDE;
    virtual SkStreamAsset* onOpenStream(int* ttcIndex) const SK_OVERRIDE;

private:
    typedef SkTypeface_FreeType INHERITED;
};
