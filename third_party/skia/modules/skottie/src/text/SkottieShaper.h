/*
 * Copyright 2019 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef SkottieShaper_DEFINED
#define SkottieShaper_DEFINED

#include "include/core/SkPoint.h"
#include "include/utils/SkTextUtils.h"

#include <vector>

class SkFontMgr;
class SkTextBlob;

namespace skottie {

// Helper implementing After Effects text shaping semantics on top of SkShaper.

class Shaper final {
public:
    struct Fragment {
        sk_sp<SkTextBlob> fBlob;
        SkPoint           fPos;

        // Only valid for kFragmentGlyphs
        float             fAdvance,
                          fAscent;
        uint32_t          fLineIndex;    // 0-based index for the line this fragment belongs to.
        bool              fIsWhitespace; // True if the first code point in the corresponding
                                         // cluster is whitespace.
    };

    struct Result {
        std::vector<Fragment> fFragments;
        size_t                fMissingGlyphCount = 0;

        SkRect computeVisualBounds() const;
    };

    enum class VAlign : uint8_t {
        // Align the first line typographical top with the text box top (AE box text).
        kTop,
        // Align the first line typographical baseline with the text box top (AE point text).
        kTopBaseline,

        // Skottie vertical alignment extensions: these are based on an extent box defined (in Y) as
        //
        //   ------------------------------------------------------
        //   MIN(visual_top_extent   , typographical_top_extent   )
        //
        //                         ...
        //
        //   MAX(visual_bottom_extent, typographical_bottom_extent)
        //   ------------------------------------------------------

        // extent box top -> text box top
        kVisualTop,
        // extent box center -> text box center
        kVisualCenter,
        // extent box bottom -> text box bottom
        kVisualBottom,
    };

    enum class ResizePolicy : uint8_t {
        // Use the specified text size.
        kNone,
        // Resize the text such that the extent box fits (snuggly) in the text box,
        // both horizontally and vertically.
        kScaleToFit,
        // Same kScaleToFit if the text doesn't fit at the specified font size.
        // Otherwise, same as kNone.
        kDownscaleToFit,
    };

    enum class LinebreakPolicy : uint8_t {
        // Break lines such that they fit in a non-empty paragraph box, horizontally.
        kParagraph,
        // Only break lines when requested explicitly (\r), regardless of paragraph box dimensions.
        kExplicit,
    };

    // Initial text direction.
    enum class Direction : uint8_t { kLTR, kRTL };

    enum class Capitalization {
        kNone,
        kUpperCase,
    };

    enum Flags : uint32_t {
        kNone                       = 0x00,

        // Split out individual glyphs into separate Fragments
        // (useful when the caller intends to manipulate glyphs independently).
        kFragmentGlyphs             = 0x01,

        // Compute the advance and ascent for each fragment.
        kTrackFragmentAdvanceAscent = 0x02,
    };

    struct TextDesc {
        const sk_sp<SkTypeface>&  fTypeface;
        SkScalar                  fTextSize,
                                  fMinTextSize,
                                  fMaxTextSize,
                                  fLineHeight,
                                  fLineShift,
                                  fAscent;
        SkTextUtils::Align        fHAlign;
        VAlign                    fVAlign;
        ResizePolicy              fResize;
        LinebreakPolicy           fLinebreak;
        Direction                 fDirection;
        Capitalization            fCapitalization;
        uint32_t                  fFlags;
    };

    // Performs text layout along an infinite horizontal line, starting at |textPoint|.
    // Only explicit line breaks (\r) are observed.
    static Result Shape(const SkString& text, const TextDesc& desc, const SkPoint& textPoint,
                        const sk_sp<SkFontMgr>&);

    // Performs text layout within |textBox|, injecting line breaks as needed to ensure
    // horizontal fitting.  The result is *not* guaranteed to fit vertically (it may extend
    // below the box bottom).
    static Result Shape(const SkString& text, const TextDesc& desc, const SkRect& textBox,
                        const sk_sp<SkFontMgr>&);

private:
    Shaper() = delete;
};

} // namespace skottie

#endif // SkottieShaper_DEFINED
