// Copyright 2017 Google Inc. All Rights Reserved.
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

#include "cobalt/renderer/rasterizer/skia/skia/src/ports/SkFontUtil_cobalt.h"
#include "cobalt/renderer/rasterizer/skia/skia/src/ports/SkStream_cobalt.h"
#include "SkFontMgr.h"
#include "SkStream.h"
#include "SkString.h"
#include "SkTArray.h"
#include "SkTypeface.h"

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
  struct SkFontStyleSetEntry_Cobalt : public SkRefCnt {
    // NOTE: SkFontStyleSetEntry_Cobalt objects are not guaranteed to last for
    // the lifetime of SkFontMgr_Cobalt and can be removed by their owning
    // SkFontStyleSet_Cobalt if their typeface fails to load properly. As a
    // result, it is not safe to store their pointers outside of
    // SkFontStyleSet_Cobalt.
    SkFontStyleSetEntry_Cobalt(const SkString& file_path, const int face_index,
                               const SkFontStyle& style,
                               const std::string& full_name,
                               const std::string& postscript_name,
                               const bool disable_synthetic_bolding)
        : font_file_path(file_path),
          face_index(face_index),
          font_style(style),
          full_font_name(full_name),
          font_postscript_name(postscript_name),
          disable_synthetic_bolding(disable_synthetic_bolding),
          is_face_info_generated(false),
          face_style(SkTypeface::kNormal),
          face_is_fixed_pitch(false) {}

    const SkString font_file_path;
    const int face_index;
    const SkFontStyle font_style;
    const std::string full_font_name;
    const std::string font_postscript_name;
    const bool disable_synthetic_bolding;

    // Face info is generated from the font file.
    bool is_face_info_generated;
    SkString face_name;
    SkTypeface::Style face_style;
    bool face_is_fixed_pitch;

    sk_sp<SkTypeface> typeface;
  };

  SkFontStyleSet_Cobalt(
      const FontFamilyInfo& family_info, const char* base_path,
      SkFileMemoryChunkStreamManager* const local_typeface_stream_manager,
      SkMutex* const manager_owned_mutex);

  // From SkFontStyleSet
  int count() override;
  // NOTE: SkFontStyleSet_Cobalt does not support getStyle(), as publicly
  // accessing styles by index is unsafe.
  void getStyle(int index, SkFontStyle* style,
                        SkString* name) override;
  // NOTE: SkFontStyleSet_Cobalt does not support createTypeface(), as
  // publicly accessing styles by index is unsafe.
  SkTypeface* createTypeface(int index) override;

  SkTypeface* matchStyle(const SkFontStyle& pattern) override;

  const SkString& get_family_name() const { return family_name_; }

 private:
  // NOTE: It is the responsibility of the caller to lock the mutex before
  // calling any of the non-const private functions.

  SkTypeface* MatchStyleWithoutLocking(const SkFontStyle& pattern);
  SkTypeface* MatchFullFontName(const std::string& name);
  SkTypeface* MatchFontPostScriptName(const std::string& name);
  SkTypeface* TryRetrieveTypefaceAndRemoveStyleOnFailure(int style_index);
  bool ContainsTypeface(const SkTypeface* typeface);

  bool ContainsCharacter(const SkFontStyle& style, SkUnichar character);
  bool CharacterMapContainsCharacter(SkUnichar character);

  bool GenerateStyleFaceInfo(SkFontStyleSetEntry_Cobalt* style,
                             SkStreamAsset* stream);

  int GetClosestStyleIndex(const SkFontStyle& pattern);
  void CreateStreamProviderTypeface(
      SkFontStyleSetEntry_Cobalt* style,
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

  // NOTE: The following characters require locking when being accessed.
  bool is_character_map_generated_;
  font_character_map::CharacterMap character_map_;

  SkTArray<sk_sp<SkFontStyleSetEntry_Cobalt>, true> styles_;

  friend class SkFontMgr_Cobalt;
};

#endif  // COBALT_RENDERER_RASTERIZER_SKIA_SKIA_SRC_PORTS_SKFONTSTYLESET_COBALT_H_
