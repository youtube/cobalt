/*
 * Copyright 2006 The Android Open Source Project
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef SKFONTHOST_CUSTOM_H_
#define SKFONTHOST_CUSTOM_H_

#include "SkFontHost.h"
#include "SkFontHost_FreeType_common.h"
#include "SkFontDescriptor.h"
#include "SkFontMgr.h"
#include "SkTypefaceCache.h"

#include <limits>

/** The base SkTypeface implementation for the custom font manager. */
class SkTypeface_Custom : public SkTypeface_FreeType {
public:
    SkTypeface_Custom(Style style, bool isFixedPitch, bool sysFont, const SkString familyName)
        : INHERITED(style, SkTypefaceCache::NewFontID(), isFixedPitch)
        , fIsSysFont(sysFont), fFamilyName(familyName)
    { }

    bool isSysFont() const { return fIsSysFont; }

    virtual const char* getUniqueString() const = 0;

protected:
    virtual void onGetFamilyName(SkString* familyName) const SK_OVERRIDE {
        *familyName = fFamilyName;
    }

    virtual void onGetFontDescriptor(SkFontDescriptor* desc, bool* isLocal) const SK_OVERRIDE {
        desc->setFamilyName(fFamilyName.c_str());
        desc->setFontFileName(this->getUniqueString());
        *isLocal = !this->isSysFont();
    }

private:
    bool fIsSysFont;
    SkString fFamilyName;

    typedef SkTypeface_FreeType INHERITED;
};

/**
 *  SkFontStyleSet_Custom
 *
 *  This class is used by SkFontMgr_Custom to hold SkTypeface_Custom families.
 */
class SkFontStyleSet_Custom : public SkFontStyleSet {
public:
    explicit SkFontStyleSet_Custom(const SkString familyName) : fFamilyName(familyName) { }

    virtual int count() SK_OVERRIDE;

    virtual void getStyle(int index, SkFontStyle* style, SkString* name) SK_OVERRIDE;

    virtual SkTypeface* createTypeface(int index) SK_OVERRIDE;

    static int match_score(const SkFontStyle& pattern, const SkFontStyle& candidate);

    virtual SkTypeface* matchStyle(const SkFontStyle& pattern) SK_OVERRIDE;

private:
    SkTArray<SkAutoTUnref<SkTypeface_Custom>, true> fStyles;
    SkString fFamilyName;

    void appendTypeface(SkTypeface_Custom* typeface);

    friend class SkFontMgr_Custom;
};

/**
 *  SkFontMgr_Custom
 *
 *  This class is essentially a collection of SkFontStyleSet_Custom,
 *  one SkFontStyleSet_Custom for each family. This class may be modified
 *  to load fonts from any source by changing the initialization.
 */
class SkFontMgr_Custom : public SkFontMgr {
public:
    explicit SkFontMgr_Custom(const char* dir, const SkTArray<SkString, true>& default_fonts);

protected:
    virtual int onCountFamilies() const SK_OVERRIDE;

    virtual void onGetFamilyName(int index, SkString* familyName) const SK_OVERRIDE;

    virtual SkFontStyleSet_Custom* onCreateStyleSet(int index) const SK_OVERRIDE;

    virtual SkFontStyleSet_Custom* onMatchFamily(const char familyName[]) const SK_OVERRIDE;

    virtual SkTypeface* onMatchFamilyStyle(const char familyName[],
                                           const SkFontStyle& fontStyle) const SK_OVERRIDE;

    virtual SkTypeface* onMatchFaceStyle(const SkTypeface* familyMember,
                                         const SkFontStyle& fontStyle) const SK_OVERRIDE;

    virtual SkTypeface* onCreateFromData(SkData* data, int ttcIndex) const SK_OVERRIDE;

    virtual SkTypeface* onCreateFromStream(SkStream* stream, int ttcIndex) const SK_OVERRIDE;

    virtual SkTypeface* onCreateFromFile(const char path[], int ttcIndex) const SK_OVERRIDE;

    virtual SkTypeface* onLegacyCreateTypeface(const char familyName[],
                                               unsigned styleBits) const SK_OVERRIDE;
private:

    static bool get_name_and_style(const char path[], SkString* name,
                                   SkTypeface::Style* style, bool* isFixedPitch);

    void load_directory_fonts(const SkString& directory);

    void load_system_fonts(const char* dir, const SkTArray<SkString, true>& default_fonts);

    SkTArray<SkAutoTUnref<SkFontStyleSet_Custom>, true> fFamilies;
    SkAutoTUnref<SkFontStyleSet_Custom> fDefaultFamily;
    SkAutoTUnref<SkTypeface> fDefaultNormal;
};

#endif // SKFONTHOST_CUSTOM_H_
