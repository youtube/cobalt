// Copyright 2016 Google Inc. All Rights Reserved.
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

#ifndef COBALT_RENDERER_RASTERIZER_SKIA_SKIA_SRC_PORTS_SKFONTMGR_COBALT_H_
#define COBALT_RENDERER_RASTERIZER_SKIA_SKIA_SRC_PORTS_SKFONTMGR_COBALT_H_

#include <string>
#include <vector>

#include "base/containers/small_map.h"
#include "base/hash_tables.h"
#include "base/memory/scoped_vector.h"
#include "cobalt/renderer/rasterizer/skia/skia/src/ports/SkFontStyleSet_cobalt.h"
#include "cobalt/renderer/rasterizer/skia/skia/src/ports/SkFontUtil_cobalt.h"
#include "cobalt/renderer/rasterizer/skia/skia/src/ports/SkStream_cobalt.h"
#include "SkFontMgr.h"
#include "SkTArray.h"
#include "SkTypeface.h"

//  This class is essentially a collection of SkFontStyleSet_Cobalt, one
//  SkFontStyleSet_Cobalt for each family. This class may be modified to load
//  fonts from any source by changing the initialization.
//
//  In addition to containing a collection of SkFontStyleSet_Cobalt, the class
//  also contains a mapping of names to families, allowing for multiple aliases
//  to map to the same back-end family.
//
//  It also contains a list of fallback fonts, which are used when attempting
//  to match a character, rather than a family name. If a language is provided
//  then fallback fonts with that language are given priority. Otherwise, the
//  fallback fonts are checked in order.
class SkFontMgr_Cobalt : public SkFontMgr {
 public:
  typedef std::vector<SkFontStyleSet_Cobalt*> StyleSetArray;

  SkFontMgr_Cobalt(const char* directory,
                   const SkTArray<SkString, true>& default_fonts);

  // NOTE: This returns NULL if a match is not found.
  SkTypeface* matchFaceName(const std::string& font_face_name);

 protected:
  // From SkFontMgr
  virtual int onCountFamilies() const SK_OVERRIDE;

  virtual void onGetFamilyName(int index,
                               SkString* family_name) const SK_OVERRIDE;

  // NOTE: This returns NULL if there is no accessible style set at the index.
  virtual SkFontStyleSet_Cobalt* onCreateStyleSet(int index) const SK_OVERRIDE;

  // NOTE: This returns NULL if there is no family match.
  virtual SkFontStyleSet_Cobalt* onMatchFamily(const char family_name[]) const
      SK_OVERRIDE;

  // NOTE: This always returns a non-NULL value. If the family name cannot be
  // found, then the best match among the default family is returned.
  virtual SkTypeface* onMatchFamilyStyle(
      const char family_name[], const SkFontStyle& style) const SK_OVERRIDE;

// NOTE: This always returns a non-NULL value. If no match can be found, then
// the best match among the default family is returned.
  virtual SkTypeface* onMatchFamilyStyleCharacter(
      const char family_name[], const SkFontStyle& style, const char bcp47[],
      SkUnichar character) const SK_OVERRIDE;

  // NOTE: This returns NULL if a match is not found.
  virtual SkTypeface* onMatchFaceStyle(const SkTypeface* family_member,
                                       const SkFontStyle& font_style) const
      SK_OVERRIDE;

  // NOTE: This returns NULL if the typeface cannot be created.
  virtual SkTypeface* onCreateFromData(SkData* data,
                                       int face_index) const SK_OVERRIDE;

  // NOTE: This returns NULL if the typeface cannot be created.
  virtual SkTypeface* onCreateFromStream(SkStreamAsset* stream,
                                         int face_index) const SK_OVERRIDE;

  // NOTE: This returns NULL if the typeface cannot be created.
  virtual SkTypeface* onCreateFromFile(const char path[],
                                       int face_index) const SK_OVERRIDE;

  // NOTE: This always returns a non-NULL value. If no match can be found, then
  // the best match among the default family is returned.
  virtual SkTypeface* onLegacyCreateTypeface(
      const char family_name[], unsigned style_bits) const SK_OVERRIDE;

 private:
  typedef base::hash_map<std::string, SkFontStyleSet_Cobalt*> NameToStyleSetMap;
  typedef base::SmallMap<base::hash_map<std::string, StyleSetArray*> >
      NameToStyleSetArrayMap;

  void BuildNameToFamilyMap(const char* base_path,
                            SkTDArray<FontFamily*>* families);
  void FindDefaultFamily(const SkTArray<SkString, true>& default_families);

  // Returns the first encountered fallback family that matches the language tag
  // and supports the specified character.
  // NOTE: |style_sets_mutex_| should be locked prior to calling this function.
  SkTypeface* FindFamilyStyleCharacter(const SkFontStyle& style,
                                       const SkString& language_tag,
                                       SkUnichar character);

  // Returns every fallback family that matches the language tag. If the tag is
  // empty, then all fallback families are returned.
  // NOTE: |style_sets_mutex_| should be locked prior to calling this function.
  StyleSetArray* GetMatchingFallbackFamilies(const SkString& language_tag);

  SkFileMemoryChunkStreamManager system_typeface_stream_manager_;

  SkTArray<SkAutoTUnref<SkFontStyleSet_Cobalt>, true> font_style_sets_;

  SkTArray<SkString> family_names_;
  //  Map names to the back end so that all names for a given family refer to
  //  the same (non-replicated) set of typefaces.
  NameToStyleSetMap name_to_family_map_;
  NameToStyleSetMap full_font_name_to_style_set_map_;
  NameToStyleSetMap font_postscript_name_to_style_set_map_;

  // Fallback families that are used during character fallback.
  // All fallback families, regardless of language.
  StyleSetArray fallback_families_;
  // Language-specific fallback families. These are lazily populated from
  // |fallback_families_| when a new language tag is requested.
  ScopedVector<StyleSetArray> language_fallback_families_array_;
  NameToStyleSetArrayMap language_fallback_families_map_;

  // The default family that is used when no specific match is found during  a
  // request.
  SkFontStyleSet_Cobalt* default_family_;

  // Mutex shared by all style sets for accessing their modifiable data.
  mutable SkMutex style_sets_mutex_;
};

#endif  // COBALT_RENDERER_RASTERIZER_SKIA_SKIA_SRC_PORTS_SKFONTMGR_COBALT_H_
