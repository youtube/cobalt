// Copyright 2017 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "cobalt/renderer/rasterizer/skia/skia/src/ports/SkFreeType_cobalt.h"

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_TRUETYPE_TABLES_H
#include FT_TYPE1_TABLES_H

#include "SkFontStyle.h"
#include "SkTSearch.h"
#include "base/logging.h"
#include "base/trace_event/trace_event.h"

namespace {

// This logic is taken from SkTypeface_FreeType::ScanFont() and should be kept
// in sync with it.
SkFontStyle GenerateSkFontStyleFromFace(FT_Face face) {
  int weight = SkFontStyle::kNormal_Weight;
  int width = SkFontStyle::kNormal_Width;
  SkFontStyle::Slant slant = SkFontStyle::kUpright_Slant;
  if (face->style_flags & FT_STYLE_FLAG_BOLD) {
    weight = SkFontStyle::kBold_Weight;
  }
  if (face->style_flags & FT_STYLE_FLAG_ITALIC) {
    slant = SkFontStyle::kItalic_Slant;
  }

  PS_FontInfoRec psFontInfo;
  TT_OS2* os2 = static_cast<TT_OS2*>(FT_Get_Sfnt_Table(face, ft_sfnt_os2));
  if (os2 && os2->version != 0xffff) {
    weight = os2->usWeightClass;
// We modify the logic here because Cobalt only supports the default font width,
// and also only supports upright and italic font slants (no oblique).
#if 0
    width = os2->usWidthClass;

    // OS/2::fsSelection bit 9 indicates oblique.
    if (SkToBool(os2->fsSelection & (1u << 9))) {
      slant = SkFontStyle::kOblique_Slant;
    }
#endif
  } else if (0 == FT_Get_PS_Font_Info(face, &psFontInfo) && psFontInfo.weight) {
    static const struct {
      char const* const name;
      int const weight;
    } commonWeights[] = {
        // There are probably more common names, but these are known to exist.
        {"all", SkFontStyle::kNormal_Weight},  // Multiple Masters usually
                                               // default to normal.
        {"black", SkFontStyle::kBlack_Weight},
        {"bold", SkFontStyle::kBold_Weight},
        {"book",
         (SkFontStyle::kNormal_Weight + SkFontStyle::kLight_Weight) / 2},
        {"demi", SkFontStyle::kSemiBold_Weight},
        {"demibold", SkFontStyle::kSemiBold_Weight},
        {"extra", SkFontStyle::kExtraBold_Weight},
        {"extrabold", SkFontStyle::kExtraBold_Weight},
        {"extralight", SkFontStyle::kExtraLight_Weight},
        {"hairline", SkFontStyle::kThin_Weight},
        {"heavy", SkFontStyle::kBlack_Weight},
        {"light", SkFontStyle::kLight_Weight},
        {"medium", SkFontStyle::kMedium_Weight},
        {"normal", SkFontStyle::kNormal_Weight},
        {"plain", SkFontStyle::kNormal_Weight},
        {"regular", SkFontStyle::kNormal_Weight},
        {"roman", SkFontStyle::kNormal_Weight},
        {"semibold", SkFontStyle::kSemiBold_Weight},
        {"standard", SkFontStyle::kNormal_Weight},
        {"thin", SkFontStyle::kThin_Weight},
        {"ultra", SkFontStyle::kExtraBold_Weight},
        {"ultrablack", SkFontStyle::kExtraBlack_Weight},
        {"ultrabold", SkFontStyle::kExtraBold_Weight},
        {"ultraheavy", SkFontStyle::kExtraBlack_Weight},
        {"ultralight", SkFontStyle::kExtraLight_Weight},
    };
    int const index =
        SkStrLCSearch(&commonWeights[0].name, SK_ARRAY_COUNT(commonWeights),
                      psFontInfo.weight, sizeof(commonWeights[0]));
    if (index >= 0) {
      weight = commonWeights[index].weight;
    } else {
      DLOG(ERROR) << "Do not know weight for: %s (%s) \n"
                  << face->family_name << psFontInfo.weight;
    }
  }

  return SkFontStyle(weight, width, slant);
}

void GenerateCharacterMapFromFace(
    FT_Face face, font_character_map::CharacterMap* character_map) {
  TRACE_EVENT0("cobalt::renderer", "GenerateCharacterMapFromFace");

  FT_UInt glyph_index;
  SkUnichar code_point = FT_Get_First_Char(face, &glyph_index);
  while (glyph_index) {
    character_map->Insert(code_point, SkToU16(glyph_index));
    code_point = FT_Get_Next_Char(face, code_point, &glyph_index);
  }
}

}  // namespace

// These functions are used by FreeType during FT_Open_Face.
extern "C" {

static unsigned long sk_freetype_cobalt_stream_io(FT_Stream ftStream,
                                                  unsigned long offset,
                                                  unsigned char* buffer,
                                                  unsigned long count) {
  SkStreamAsset* stream =
      static_cast<SkStreamAsset*>(ftStream->descriptor.pointer);
  stream->seek(offset);
  return stream->read(buffer, count);
}

static void sk_freetype_cobalt_stream_close(FT_Stream) {}
}

namespace sk_freetype_cobalt {

bool ScanFont(SkStreamAsset* stream, int face_index, SkString* name,
              SkFontStyle* style, bool* is_fixed_pitch,
              font_character_map::CharacterMap* maybe_character_map /*=NULL*/) {
  TRACE_EVENT0("cobalt::renderer", "SkFreeTypeUtil::ScanFont()");

  FT_Library freetype_lib;
  if (FT_Init_FreeType(&freetype_lib) != 0) {
    return false;
  }

  FT_StreamRec streamRec;
  memset(&streamRec, 0, sizeof(streamRec));
  streamRec.size = stream->getLength();
  streamRec.descriptor.pointer = stream;
  streamRec.read = sk_freetype_cobalt_stream_io;
  streamRec.close = sk_freetype_cobalt_stream_close;

  FT_Open_Args args;
  memset(&args, 0, sizeof(args));
  args.flags = FT_OPEN_STREAM;
  args.stream = &streamRec;

  FT_Face face;
  FT_Error err = FT_Open_Face(freetype_lib, &args, face_index, &face);
  if (err) {
    FT_Done_FreeType(freetype_lib);
    return false;
  }

  name->set(face->family_name);
  *style = GenerateSkFontStyleFromFace(face);
  *is_fixed_pitch = FT_IS_FIXED_WIDTH(face);

  if (maybe_character_map) {
    GenerateCharacterMapFromFace(face, maybe_character_map);
  }

  // release this font.
  FT_Done_Face(face);

  // shut down FreeType.
  FT_Done_FreeType(freetype_lib);
  return true;
}

}  // namespace sk_freetype_cobalt
