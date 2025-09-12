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

#ifndef COBALT_RENDERER_RASTERIZER_SKIA_SKIA_SRC_PORTS_SKFONTSTYLESET_COBALT_H_
#define COBALT_RENDERER_RASTERIZER_SKIA_SKIA_SRC_PORTS_SKFONTSTYLESET_COBALT_H_

#include <string>
#include <unordered_map>

#include "SkFontMgr.h"
#include "SkStream.h"
#include "SkString.h"
#include "SkTHash.h"
#include "SkTypeface.h"
#include "cobalt/renderer/rasterizer/skia/skia/src/ports/SkFontUtil_cobalt.h"
#include "cobalt/renderer/rasterizer/skia/skia/src/ports/SkStream_cobalt.h"

// This class, which is thread-safe, is Cobalt's implementation of
// SkFontStyleSet. It represents a collection of local typefaces that support
// the same set of characters but with different styles. When a specific style
// is requested, SkFontStyleSet returns the typeface that best matches that
// style.
//
// SkFontStyleSet contains a map of the characters that it supports. It uses
// this map to quickly check for whether or not its typefaces can provide glyphs
// for specific characters during fallback.
//
// Both the character map of the style set and the typeface of each individual
// entry are lazily loaded the first time that they are needed.
class SkFontStyleSet_Cobalt : public SkFontStyleSet {
 public:
  typedef SkTArray<Fixed16, true> ComputedVariationPosition;

  struct SkFontStyleSetEntry_Cobalt : public SkRefCnt {
    // NOTE: SkFontStyleSetEntry_Cobalt objects are not guaranteed to last for
    // the lifetime of SkFontMgr_Cobalt and can be removed by their owning
    // SkFontStyleSet_Cobalt if their typeface fails to load properly. As a
    // result, it is not safe to store their pointers outside of
    // SkFontStyleSet_Cobalt.
    SkFontStyleSetEntry_Cobalt(
        const SkString& file_path,
        const int face_index,
        const ComputedVariationPosition& computed_variation_position,
        const SkFontStyle& style,
        const std::string& full_name,
        const std::string& postscript_name,
        const bool disable_synthetic_bolding)
        : font_file_path(file_path),
          face_index(face_index),
          computed_variation_position(computed_variation_position),
          font_style(style),
          full_font_name(full_name),
          font_postscript_name(postscript_name),
          disable_synthetic_bolding(disable_synthetic_bolding),
          is_face_info_generated(false),
          face_is_fixed_pitch(false) {}

    const SkString font_file_path;
    const int face_index;
    const ComputedVariationPosition computed_variation_position;
    SkFontStyle font_style;
    const std::string full_font_name;
    const std::string font_postscript_name;
    const bool disable_synthetic_bolding;

    // Face info is generated from the font file.
    bool is_face_info_generated;
    SkString face_name;
    bool face_is_fixed_pitch;

    sk_sp<SkTypeface> typeface;
  };

  enum FontFormatSetting {
    kTtf,
    kWoff2,
    kTtfPreferred,
    kWoff2Preferred,
  };

  SkFontStyleSet_Cobalt(
      const FontFamilyInfo& family_info,
      const char* base_path,
      SkFileMemoryChunkStreamManager* const local_typeface_stream_manager,
      SkMutex* const manager_owned_mutex,
      FontFormatSetting font_format_setting);

  // From SkFontStyleSet
  int count() override;
  // NOTE: SkFontStyleSet_Cobalt does not support getStyle(), as publicly
  // accessing styles by index is unsafe.
  void getStyle(int index, SkFontStyle* style, SkString* name) override;
  // NOTE: SkFontStyleSet_Cobalt does not support createTypeface(), as
  // publicly accessing styles by index is unsafe.
  SkTypeface* createTypeface(int index) override;

  SkTypeface* matchStyle(const SkFontStyle& pattern) override;

  const SkString& get_family_name() const { return family_name_; }

  const SkLanguage& get_language() const { return language_; }

 private:
  // NOTE: It is the responsibility of the caller to lock the mutex before
  // calling any of the non-const private functions.

  SkTypeface* MatchStyleWithoutLocking(const SkFontStyle& pattern);
  SkTypeface* MatchFullFontName(const std::string& name);
  SkTypeface* MatchFontPostScriptName(const std::string& name);
  SkTypeface* TryRetrieveTypefaceAndRemoveStyleOnFailure(int style_index);
  bool ContainsTypeface(const SkTypeface* typeface);

  bool ContainsCharacter(const SkFontStyle& style, SkUnichar character);
  bool CharacterMapContainsCharacter(
      SkUnichar character,
      const scoped_refptr<font_character_map::CharacterMap>& map);

  bool GenerateStyleFaceInfo(SkFontStyleSetEntry_Cobalt* style,
                             SkStreamAsset* stream,
                             int style_index);

  int GetClosestStyleIndex(const SkFontStyle& pattern);
  void CreateStreamProviderTypeface(
      SkFontStyleSetEntry_Cobalt* style,
      int style_index,
      SkFileMemoryChunkStreamProvider* stream_provider = NULL);

  // Purge typefaces that are only referenced internally.
  void PurgeUnreferencedTypefaces();

  // NOTE: The following variables can safely be accessed outside of the mutex.
  SkFileMemoryChunkStreamManager* const local_typeface_stream_manager_;
  SkMutex* const manager_owned_mutex_;

  SkString family_name_;
  bool is_fallback_family_;
  SkLanguage language_;
  font_character_map::PageRanges page_ranges_;

  // Used when the styles in the styleset have different character mappings.
  bool disable_character_map_;

  // NOTE: The following characters require locking when being accessed.
  // This map will store one map per style, and can be indexed with the
  // style_index of each style.
  std::unordered_map<int, scoped_refptr<font_character_map::CharacterMap>>
      character_maps_;

  SkTArray<sk_sp<SkFontStyleSetEntry_Cobalt>, true> styles_;

  friend class SkFontMgr_Cobalt;
};

#endif  // COBALT_RENDERER_RASTERIZER_SKIA_SKIA_SRC_PORTS_SKFONTSTYLESET_COBALT_H_
