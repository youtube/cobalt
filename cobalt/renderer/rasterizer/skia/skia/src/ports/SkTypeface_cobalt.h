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

#ifndef COBALT_RENDERER_RASTERIZER_SKIA_SKIA_SRC_PORTS_SKTYPEFACE_COBALT_H_
#define COBALT_RENDERER_RASTERIZER_SKIA_SKIA_SRC_PORTS_SKTYPEFACE_COBALT_H_

#include <memory>

#include "SkStream.h"
#include "SkString.h"
#include "base/memory/ref_counted.h"
#include "cobalt/renderer/rasterizer/skia/skia/src/ports/SkFontStyleSet_cobalt.h"
#include "cobalt/renderer/rasterizer/skia/skia/src/ports/SkStream_cobalt.h"
#include "third_party/skia/src/ports/SkFontHost_FreeType_common.h"

class SkFontMgr_Cobalt;

class SkTypeface_Cobalt : public SkTypeface_FreeType {
 public:
  SkTypeface_Cobalt(
      int face_index, SkFontStyle style, bool is_fixed_pitch,
      const SkString& family_name,
      scoped_refptr<font_character_map::CharacterMap> character_map);

  virtual size_t GetStreamLength() const = 0;

  bool synthesizes_bold() const { return synthesizes_bold_; }

 protected:
  sk_sp<SkTypeface> onMakeClone(const SkFontArguments& args) const override;

  void onCharsToGlyphs(const SkUnichar uni[], int count,
                       SkGlyphID glyphs[]) const override;

  void onGetFamilyName(SkString* family_name) const override;

  int face_index_;
  SkString family_name_;
  bool synthesizes_bold_;

 private:
  typedef SkTypeface_FreeType INHERITED;
  SkGlyphID characterMapGetGlyphIdForCharacter(SkUnichar character) const;
  scoped_refptr<font_character_map::CharacterMap> character_map_;
};

class SkTypeface_CobaltStream : public SkTypeface_Cobalt {
 public:
  SkTypeface_CobaltStream(
      std::unique_ptr<SkStreamAsset> stream, int face_index, SkFontStyle style,
      bool is_fixed_pitch, const SkString& family_name,
      scoped_refptr<font_character_map::CharacterMap> character_map);

  void onGetFontDescriptor(SkFontDescriptor* descriptor,
                           bool* serialize) const override;

  std::unique_ptr<SkStreamAsset> onOpenStream(int* face_index) const override;

  size_t GetStreamLength() const override;

 private:
  typedef SkTypeface_Cobalt INHERITED;

  std::unique_ptr<SkStreamAsset> stream_;
};

class SkTypeface_CobaltStreamProvider : public SkTypeface_Cobalt {
 public:
  SkTypeface_CobaltStreamProvider(
      SkFileMemoryChunkStreamProvider* stream_provider, int face_index,
      SkFontStyle style, bool is_fixed_pitch, const SkString& family_name,
      bool disable_synthetic_bolding,
      scoped_refptr<font_character_map::CharacterMap> character_map);

  void onGetFontDescriptor(SkFontDescriptor* descriptor,
                           bool* serialize) const override;

  std::unique_ptr<SkStreamAsset> onOpenStream(int* face_index) const override;

  size_t GetStreamLength() const override;

 private:
  typedef SkTypeface_Cobalt INHERITED;

  SkFileMemoryChunkStreamProvider* const stream_provider_;
};

#endif  // COBALT_RENDERER_RASTERIZER_SKIA_SKIA_SRC_PORTS_SKTYPEFACE_COBALT_H_
