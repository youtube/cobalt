/*
 * Copyright 2006 The Android Open Source Project
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "SkFontHost_Custom.h"

#include "SkData.h"
#include "SkFontHost.h"
#include "SkFontHost_FreeType_common.h"
#include "SkFontDescriptor.h"
#include "SkFontMgr.h"
#include "SkDescriptor.h"
#include "SkOSFile.h"
#include "SkPaint.h"
#include "SkString.h"
#include "SkStream.h"
#include "SkThread.h"
#include "SkTSearch.h"
#include "SkTypefaceCache.h"
#include "SkTArray.h"

#include <cmath>
#include <limits>

#ifndef SK_FONT_FILE_PREFIX
#    define SK_FONT_FILE_PREFIX "/usr/share/fonts/truetype/"
#endif

///////////////////////////////////////////////////////////////////////////////

/** The empty SkTypeface implementation for the custom font manager.
 *  Used as the last resort fallback typeface.
 */
class SkTypeface_Empty : public SkTypeface_Custom {
public:
    SkTypeface_Empty() : INHERITED(SkTypeface::kNormal, false, true, SkString()) {}

    virtual const char* getUniqueString() const SK_OVERRIDE { return NULL; }

protected:
    virtual SkStream* onOpenStream(int*) const SK_OVERRIDE { return NULL; }

private:
    typedef SkTypeface_Custom INHERITED;
};

/** The stream SkTypeface implementation for the custom font manager. */
class SkTypeface_Stream : public SkTypeface_Custom {
public:
    SkTypeface_Stream(Style style, bool isFixedPitch, bool sysFont, const SkString familyName,
                      SkStream* stream, int ttcIndex)
        : INHERITED(style, isFixedPitch, sysFont, familyName)
        , fStream(SkRef(stream)), fTtcIndex(ttcIndex)
    { }

    virtual const char* getUniqueString() const SK_OVERRIDE { return NULL; }

protected:
    virtual SkStream* onOpenStream(int* ttcIndex) const SK_OVERRIDE {
        *ttcIndex = 0;
        return fStream->duplicate();
    }

private:
    SkAutoTUnref<SkStream> fStream;
    int fTtcIndex;

    typedef SkTypeface_Custom INHERITED;
};

/** The file SkTypeface implementation for the custom font manager. */
class SkTypeface_File : public SkTypeface_Custom {
public:
    SkTypeface_File(Style style, bool isFixedPitch, bool sysFont, const SkString familyName,
                    const char path[])
        : INHERITED(style, isFixedPitch, sysFont, familyName)
        , fPath(path)
    { }

    virtual const char* getUniqueString() const SK_OVERRIDE {
        const char* str = strrchr(fPath.c_str(), '/');
        if (str) {
            str += 1;   // skip the '/'
        }
        return str;
    }

protected:
    virtual SkStream* onOpenStream(int* ttcIndex) const SK_OVERRIDE {
        *ttcIndex = 0;
        return SkStream::NewFromFile(fPath.c_str());
    }

private:
    SkString fPath;

    typedef SkTypeface_Custom INHERITED;
};

///////////////////////////////////////////////////////////////////////////////

/**
 *  SkFontStyleSet_Custom
 *
 *  This class is used by SkFontMgr_Custom to hold SkTypeface_Custom families.
 */
int SkFontStyleSet_Custom::count() {
    return fStyles.count();
}

void SkFontStyleSet_Custom::getStyle(int index, SkFontStyle* style, SkString* name) {
    SkASSERT(index < fStyles.count());
    bool bold = fStyles[index]->isBold();
    bool italic = fStyles[index]->isItalic();
    *style = SkFontStyle(bold ? SkFontStyle::kBold_Weight : SkFontStyle::kNormal_Weight,
                         SkFontStyle::kNormal_Width,
                         italic ? SkFontStyle::kItalic_Slant : SkFontStyle::kUpright_Slant);
    name->reset();
}

SkTypeface* SkFontStyleSet_Custom::createTypeface(int index) {
    SkASSERT(index < fStyles.count());
    return SkRef(fStyles[index].get());
}

int SkFontStyleSet_Custom::match_score(const SkFontStyle& pattern, const SkFontStyle& candidate) {
    int score = 0;
    score += (pattern.width() - candidate.width()) * 100;
    score += (pattern.isItalic() == candidate.isItalic()) ? 0 : 1000;
    score += pattern.weight() - candidate.weight();
    return score;
}

SkTypeface* SkFontStyleSet_Custom::matchStyle(const SkFontStyle& pattern) {
    if (0 == fStyles.count()) {
        return NULL;
    }

    SkTypeface_Custom* closest = fStyles[0];
    int minScore = std::numeric_limits<int>::max();
    for (int i = 0; i < fStyles.count(); ++i) {
        bool bold = fStyles[i]->isBold();
        bool italic = fStyles[i]->isItalic();
        SkFontStyle style = SkFontStyle(bold ? SkFontStyle::kBold_Weight
                                             : SkFontStyle::kNormal_Weight,
                                        SkFontStyle::kNormal_Width,
                                        italic ? SkFontStyle::kItalic_Slant
                                               : SkFontStyle::kUpright_Slant);

        int score = match_score(pattern, style);
        if (std::abs(score) < std::abs(minScore)) {
            closest = fStyles[i];
            minScore = score;
        }
    }
    return SkRef(closest);
}

void SkFontStyleSet_Custom::appendTypeface(SkTypeface_Custom* typeface) {
    fStyles.push_back().reset(typeface);
}

SkFontMgr_Custom::SkFontMgr_Custom(const char* dir, const SkTArray<SkString, true>& default_fonts) {
    this->load_system_fonts(dir, default_fonts);
}

int SkFontMgr_Custom::onCountFamilies() const {
    return fFamilies.count();
}

void SkFontMgr_Custom::onGetFamilyName(int index, SkString* familyName) const {
    SkASSERT(index < fFamilies.count());
    familyName->set(fFamilies[index]->fFamilyName);
}

SkFontStyleSet_Custom* SkFontMgr_Custom::onCreateStyleSet(int index) const {
    SkASSERT(index < fFamilies.count());
    return SkRef(fFamilies[index].get());
}

SkFontStyleSet_Custom* SkFontMgr_Custom::onMatchFamily(const char familyName[]) const {
    for (int i = 0; i < fFamilies.count(); ++i) {
        if (fFamilies[i]->fFamilyName.equals(familyName)) {
            return SkRef(fFamilies[i].get());
        }
    }
    return NULL;
}

SkTypeface* SkFontMgr_Custom::onMatchFamilyStyle(const char familyName[],
                                                 const SkFontStyle& fontStyle) const
{
    SkAutoTUnref<SkFontStyleSet> sset(this->matchFamily(familyName));
    return sset->matchStyle(fontStyle);
}

SkTypeface* SkFontMgr_Custom::onMatchFaceStyle(const SkTypeface* familyMember,
                                               const SkFontStyle& fontStyle) const
{
    for (int i = 0; i < fFamilies.count(); ++i) {
        for (int j = 0; j < fFamilies[i]->fStyles.count(); ++j) {
            if (fFamilies[i]->fStyles[j] == familyMember) {
                return fFamilies[i]->matchStyle(fontStyle);
            }
        }
    }
    return NULL;
}

SkTypeface* SkFontMgr_Custom::onCreateFromData(SkData* data, int ttcIndex) const {
    SkAutoTUnref<SkStream> stream(new SkMemoryStream(data));
    return this->createFromStream(stream, ttcIndex);
}

SkTypeface* SkFontMgr_Custom::onCreateFromStream(SkStream* stream, int ttcIndex) const {
    if (NULL == stream || stream->getLength() <= 0) {
        SkDELETE(stream);
        return NULL;
    }

    bool isFixedPitch;
    SkTypeface::Style style;
    SkString name;
    if (SkTypeface_FreeType::ScanFont(stream, ttcIndex, &name, &style, &isFixedPitch)) {
        return SkNEW_ARGS(SkTypeface_Stream, (style, isFixedPitch, false, name,
                                              stream, ttcIndex));
    } else {
        return NULL;
    }
}

SkTypeface* SkFontMgr_Custom::onCreateFromFile(const char path[], int ttcIndex) const {
    SkAutoTUnref<SkStream> stream(SkStream::NewFromFile(path));
    return stream.get() ? this->createFromStream(stream, ttcIndex) : NULL;
}

SkTypeface* SkFontMgr_Custom::onLegacyCreateTypeface(const char familyName[],
                                                     unsigned styleBits) const
{
    SkTypeface::Style oldStyle = (SkTypeface::Style)styleBits;
    SkFontStyle style = SkFontStyle(oldStyle & SkTypeface::kBold
                                             ? SkFontStyle::kBold_Weight
                                             : SkFontStyle::kNormal_Weight,
                                    SkFontStyle::kNormal_Width,
                                    oldStyle & SkTypeface::kItalic
                                             ? SkFontStyle::kItalic_Slant
                                             : SkFontStyle::kUpright_Slant);
    SkTypeface* tf = NULL;

    if (familyName) {
        tf = this->onMatchFamilyStyle(familyName, style);
    }

    if (NULL == tf) {
        tf = fDefaultFamily->matchStyle(style);
    }

    return tf;
}

bool SkFontMgr_Custom::get_name_and_style(const char path[], SkString* name,
                                          SkTypeface::Style* style, bool* isFixedPitch) {
    SkAutoTUnref<SkStream> stream(SkStream::NewFromFile(path));
    if (stream.get()) {
        return SkTypeface_FreeType::ScanFont(stream, 0, name, style, isFixedPitch);
    } else {
        SkDebugf("---- failed to open <%s> as a font\n", path);
        return false;
    }
}

void SkFontMgr_Custom::load_directory_fonts(const SkString& directory) {
    SkOSFile::Iter iter(directory.c_str(), ".ttf");
    SkString name;

    while (iter.next(&name, false)) {
        SkString filename(
            SkOSPath::Join(directory.c_str(), name.c_str()));

        bool isFixedPitch;
        SkString realname;
        SkTypeface::Style style = SkTypeface::kNormal; // avoid uninitialized warning

        if (!get_name_and_style(filename.c_str(), &realname, &style, &isFixedPitch)) {
            SkDebugf("------ can't load <%s> as a font\n", filename.c_str());
            continue;
        }

        // Open a file stream pointed to the current font "ttf" file.
        SkAutoTUnref<SkStream> file_stream(
            SkStream::NewFromFile(filename.c_str()));
        // Load the entire font file into memory and store it as a SkData.
        SkAutoTUnref<SkData> font_data(
            SkData::NewFromStream(file_stream, file_stream->getLength()));
        // Pass the SkData into the SkMemoryStream.
        SkAutoTUnref<SkMemoryStream> stream(new SkMemoryStream(font_data));

        // Create a font based off of our memory stream referencing the font
        // data.
        SkTypeface_Custom* tf = SkNEW_ARGS(SkTypeface_Stream, (
                                            style,
                                            isFixedPitch,
                                            true,  // system-font (cannot delete)
                                            realname,
                                            stream,
                                            0));

        SkAutoTUnref<SkFontStyleSet_Custom> addTo(
            this->onMatchFamily(realname.c_str()));

        if (NULL == addTo) {
            addTo.reset(new SkFontStyleSet_Custom(realname));
            fFamilies.push_back().reset(SkRef(addTo.get()));
        }
        addTo->appendTypeface(tf);
    }

    SkOSFile::Iter dirIter(directory.c_str());
    while (dirIter.next(&name, true)) {
        if (name.startsWith(".")) {
            continue;
        }
        SkString dirname(SkOSPath::Join(directory.c_str(), name.c_str()));
        load_directory_fonts(dirname);
    }
}

void SkFontMgr_Custom::load_system_fonts(const char* dir, const SkTArray<SkString, true>& default_fonts) {
    SkString baseDirectory(dir);
    load_directory_fonts(baseDirectory);

    if (fFamilies.empty()) {
        SkFontStyleSet_Custom* family = new SkFontStyleSet_Custom(SkString());
        fFamilies.push_back().reset(family);
        family->appendTypeface(SkNEW(SkTypeface_Empty));
    }

    // Try to pick a default font.
    for (size_t i = 0; i < default_fonts.count(); ++i) {
        SkAutoTUnref<SkFontStyleSet_Custom> set(
            this->onMatchFamily(default_fonts[i].c_str()));
        if (NULL == set) {
            continue;
        }

        SkAutoTUnref<SkTypeface> tf(
            set->matchStyle(SkFontStyle(SkFontStyle::kNormal_Weight,
                                        SkFontStyle::kNormal_Width,
                                        SkFontStyle::kUpright_Slant)));
        if (NULL == tf) {
            continue;
        }

        fDefaultFamily.swap(&set);
        fDefaultNormal.swap(&tf);
        break;
    }
    if (NULL == fDefaultNormal) {
        fDefaultFamily.reset(SkRef(fFamilies[0].get()));
        fDefaultNormal.reset(SkRef(fDefaultFamily->fStyles[0].get()));
    }
}
