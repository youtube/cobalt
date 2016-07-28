/*
 * Copyright 2006 The Android Open Source Project
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "SkFontHost_Custom.h"

#ifndef SK_FONT_FILE_PREFIX
#    define SK_FONT_FILE_PREFIX "/usr/share/fonts/truetype/"
#endif

///////////////////////////////////////////////////////////////////////////////

SkFontMgr* SkFontMgr::Factory() {
    SkTArray<SkString, true> default_fonts;
    default_fonts.push_back(SkString("Arial"));
    default_fonts.push_back(SkString("Verdana"));
    default_fonts.push_back(SkString("Times New Roman"));
    default_fonts.push_back(SkString("Droid Sans"));

    return new SkFontMgr_Custom(SK_FONT_FILE_PREFIX, default_fonts);
}
