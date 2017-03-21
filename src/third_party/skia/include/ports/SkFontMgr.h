/*
 * Copyright 2013 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef SkFontMgr_DEFINED
#define SkFontMgr_DEFINED

#include "SkRefCnt.h"
#include "SkFontStyle.h"

class SkData;
class SkStreamAsset;
class SkString;
class SkTypeface;

class SK_API SkFontStyleSet : public SkRefCnt {
public:
    SK_DECLARE_INST_COUNT(SkFontStyleSet)

    virtual int count() = 0;
    virtual void getStyle(int index, SkFontStyle*, SkString* style) = 0;
    virtual SkTypeface* createTypeface(int index) = 0;
    virtual SkTypeface* matchStyle(const SkFontStyle& pattern) = 0;

    static SkFontStyleSet* CreateEmpty();

private:
    typedef SkRefCnt INHERITED;
};

class SkTypeface;

class SK_API SkFontMgr : public SkRefCnt {
public:
    SK_DECLARE_INST_COUNT(SkFontMgr)

    int countFamilies() const;
    void getFamilyName(int index, SkString* familyName) const;
    SkFontStyleSet* createStyleSet(int index) const;

    /**
     *  The caller must call unref() on the returned object.
     *  Never returns NULL; will return an empty set if the name is not found.
     *
     *  It is possible that this will return a style set not accessible from
     *  createStyleSet(int) due to hidden or auto-activated fonts.
     */
    SkFontStyleSet* matchFamily(const char familyName[]) const;

    /**
     *  Find the closest matching typeface to the specified familyName and style
     *  and return a ref to it. The caller must call unref() on the returned
     *  object. Will never return NULL, as it will return the default font if
     *  no matching font is found.
     *
     *  It is possible that this will return a style set not accessible from
     *  createStyleSet(int) or matchFamily(const char[]) due to hidden or
     *  auto-activated fonts.
     */
    SkTypeface* matchFamilyStyle(const char familyName[], const SkFontStyle&) const;

    /**
     *  Use the system fallback to find a typeface for the given character.
     *  Note that bcp47 is a combination of ISO 639, 15924, and 3166-1 codes,
     *  so it is fine to just pass a ISO 639 here.
     *
     *  Will return NULL if no family can be found for the character
     *  in the system fallback.
     *
     *  bcp47[0] is the least significant fallback, bcp47[bcp47Count-1] is the
     *  most significant. If no specified bcp47 codes match, any font with the
     *  requested character will be matched.
     */
#ifdef SK_FM_NEW_MATCH_FAMILY_STYLE_CHARACTER
    SkTypeface* matchFamilyStyleCharacter(const char familyName[], const SkFontStyle&,
                                          const char* bcp47[], int bcp47Count,
                                          SkUnichar character) const;
#else
    SkTypeface* matchFamilyStyleCharacter(const char familyName[], const SkFontStyle&,
                                          const char bcp47[], SkUnichar character) const;
#endif

    SkTypeface* matchFaceStyle(const SkTypeface*, const SkFontStyle&) const;

    /**
     *  Create a typeface for the specified data and TTC index (pass 0 for none)
     *  or NULL if the data is not recognized. The caller must call unref() on
     *  the returned object if it is not null.
     */
    SkTypeface* createFromData(SkData*, int ttcIndex = 0) const;

    /**
     *  Create a typeface for the specified stream and TTC index
     *  (pass 0 for none) or NULL if the stream is not recognized. The caller
     *  must call unref() on the returned object if it is not null.
     */
    SkTypeface* createFromStream(SkStreamAsset*, int ttcIndex = 0) const;

    /**
     *  Create a typeface for the specified fileName and TTC index
     *  (pass 0 for none) or NULL if the file is not found, or its contents are
     *  not recognized. The caller must call unref() on the returned object
     *  if it is not null.
     */
    SkTypeface* createFromFile(const char path[], int ttcIndex = 0) const;

    SkTypeface* legacyCreateTypeface(const char familyName[],
                                     unsigned typefaceStyleBits) const;

    /**
     *  Return a ref to the default fontmgr. The caller must call unref() on
     *  the returned object.
     */
    static SkFontMgr* RefDefault();

protected:
    virtual int onCountFamilies() const = 0;
    virtual void onGetFamilyName(int index, SkString* familyName) const = 0;
    virtual SkFontStyleSet* onCreateStyleSet(int index)const  = 0;

    /** May return NULL if the name is not found. */
    virtual SkFontStyleSet* onMatchFamily(const char familyName[]) const = 0;

    virtual SkTypeface* onMatchFamilyStyle(const char familyName[],
                                           const SkFontStyle&) const = 0;
    // TODO: pure virtual, implement on all impls.
#ifdef SK_FM_NEW_MATCH_FAMILY_STYLE_CHARACTER
    virtual SkTypeface* onMatchFamilyStyleCharacter(const char familyName[], const SkFontStyle&,
                                                    const char* bcp47[], int bcp47Count,
                                                    SkUnichar character) const
#else
    virtual SkTypeface* onMatchFamilyStyleCharacter(const char familyName[], const SkFontStyle&,
                                                    const char bcp47[], SkUnichar character) const
#endif
    { return NULL; }
    virtual SkTypeface* onMatchFaceStyle(const SkTypeface*,
                                         const SkFontStyle&) const = 0;

    virtual SkTypeface* onCreateFromData(SkData*, int ttcIndex) const = 0;
    virtual SkTypeface* onCreateFromStream(SkStreamAsset*, int ttcIndex) const = 0;
    virtual SkTypeface* onCreateFromFile(const char path[], int ttcIndex) const = 0;

    virtual SkTypeface* onLegacyCreateTypeface(const char familyName[],
                                               unsigned styleBits) const = 0;
private:
    static SkFontMgr* Factory();    // implemented by porting layer
    static SkFontMgr* CreateDefault();

    typedef SkRefCnt INHERITED;
};

#endif
