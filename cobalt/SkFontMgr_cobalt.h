// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#include <unordered_map>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "include/core/SkFontMgr.h"
#include "src/base/SkTSearch.h"
#include "include/core/SkTypeface.h"
#include "base/containers/small_map.h"
#include "base/synchronization/waitable_event.h"
#include "include/private/base/SkTArray.h"
#include "include/private/base/SkThreadAnnotations.h"
#include "include/private/base/SkTDArray.h"
#include "include/private/base/SkMutex.h"
#include "third_party/skia/include/core/SkTypeface.h"
#include "base/containers/small_map.h"
#include "base/memory/ref_counted.h"

class SkLanguage {
 public:
  SkLanguage() {}
  explicit SkLanguage(const SkString& tag) : tag_(tag) {}
  explicit SkLanguage(const char* tag) : tag_(tag) {}
  SkLanguage(const char* tag, size_t len) : tag_(tag, len) {}
  SkLanguage(const SkLanguage& b) : tag_(b.tag_) {}

  // Gets a BCP 47 language identifier for this SkLanguage.
  // @return a BCP 47 language identifier representing this language
  const SkString& GetTag() const { return tag_; }

  // Performs BCP 47 fallback to return an SkLanguage one step more general.
  // @return an SkLanguage one step more general
  SkLanguage GetParent() const;

  bool operator==(const SkLanguage& b) const { return tag_ == b.tag_; }
  bool operator!=(const SkLanguage& b) const { return tag_ != b.tag_; }
  SkLanguage& operator=(const SkLanguage& b) {
    tag_ = b.tag_;
    return *this;
  }

 private:
  //! BCP 47 language identifier
  SkString tag_;
};

typedef int32_t Fixed16;
typedef short int16;

namespace font_character_map {

static const int kMaxCharacterValue = 0x10ffff;
static const int kNumCharacterBitsPerPage = 8;
static const int kNumCharactersPerPage = 1 << kNumCharacterBitsPerPage;
static const int kMaxPageValue = kMaxCharacterValue >> kNumCharacterBitsPerPage;
static const int kPageCharacterIndexBitMask = kNumCharactersPerPage - 1;

// PageRange is a min-max pair of values that indicate that the font provides
// at least one character on all pages between the min and max value, inclusive.
typedef std::pair<int16, int16> PageRange;

// PageRanges is an array of PageRange objects that requires the objects to be
// in non-overlapping ascending order.
typedef skia_private::TArray<PageRange> PageRanges;

bool IsCharacterValid(SkUnichar character);
int16 GetPage(SkUnichar character);
unsigned char GetPageCharacterIndex(SkUnichar character);

struct Character {
  SkGlyphID id = 0;
  bool is_set = false;
};

using PageCharacters = Character[kNumCharactersPerPage];
class CharacterMap : public base::RefCountedThreadSafe<CharacterMap> {
 public:
  const Character Find(SkUnichar character) {
    SkAutoMutexExclusive scoped_mutex(mutex_);

    int16 page = GetPage(character);
    std::map<int16, PageCharacters>::iterator page_iterator = data_.find(page);

    if (page_iterator == data_.end()) return {};

    unsigned char character_index = GetPageCharacterIndex(character);
    return page_iterator->second[character_index];
  }

  void Insert(SkUnichar character, SkGlyphID glyph) {
    SkAutoMutexExclusive scoped_mutex(mutex_);

    int16 page = GetPage(character);
    unsigned char character_index = GetPageCharacterIndex(character);
    data_[page][character_index] = {/* id= */ glyph, /* is_set= */ true};
  }

 private:
  std::map<int16, PageCharacters> data_;
  mutable SkMutex mutex_;
};

}  // namespace font_character_map

struct FontFileInfo {
  enum FontStyles {
    kNormal_FontStyle = 0x01,
    kItalic_FontStyle = 0x02,
  };
  typedef uint32_t FontStyle;

  struct VariationCoordinate {
    uint32_t tag;
    Fixed16 value;
  };
  typedef skia_private::TArray<VariationCoordinate, true> VariationPosition;

  FontFileInfo()
      : index(0),
        weight(400),
        style(kNormal_FontStyle),
        disable_synthetic_bolding(false) {}

  SkString file_name;
  int index;
  int weight;
  FontStyle style;
  bool disable_synthetic_bolding;
  VariationPosition variation_position;

  SkString full_font_name;
  SkString postscript_name;
};

struct FontFamilyInfo {
  FontFamilyInfo()
      : is_fallback_family(true), fallback_priority(0), disable_caching(0) {}

  skia_private::TArray<SkString> names;
  skia_private::TArray<FontFileInfo> fonts;
  SkLanguage language;
  bool is_fallback_family;
  int fallback_priority;
  font_character_map::PageRanges page_ranges;
  bool disable_caching;
};

class SkFileMemoryChunkStreamProvider;
class SkFileMemoryChunk;
class SkFileMemoryChunkStream;

class SkFileMemoryChunkStreamManager {
 public:
  SkFileMemoryChunkStreamManager(const std::string& name,
                                 int cache_capacity_in_bytes);

  // Returns the stream provider associated with the file path. If it does not
  // exist then it is created.
  SkFileMemoryChunkStreamProvider* GetStreamProvider(
      const std::string& file_path);

  // Purges unused memory chunks from all existing stream providers.
  void PurgeUnusedMemoryChunks();

 private:
  friend SkFileMemoryChunkStreamProvider;

  // Attempts to reserve an available memory chunk and returns true if
  // successful. On success, |available_chunk_count_| is decremented by one.
  bool TryReserveMemoryChunk();
  // Adds the specified count to |available_chunk_count_|.
  void ReleaseReservedMemoryChunks(size_t count);

  SkMutex stream_provider_mutex_;
  std::vector<std::unique_ptr<SkFileMemoryChunkStreamProvider>>
      stream_provider_array_;
  SkFileMemoryChunkStreamProviderMap stream_provider_map_;

  base::subtle::Atomic32 available_chunk_count_;
};

class SkFontStyleSet_Cobalt : public SkFontStyleSet {
 public:
  typedef skia_private::TArray<Fixed16, true> ComputedVariationPosition;

  struct SkFontStyleSetEntry_Cobalt : public SkRefCnt {
    // NOTE: SkFontStyleSetEntry_Cobalt objects are not guaranteed to last for
    // the lifetime of SkFontMgr_Cobalt and can be removed by their owning
    // SkFontStyleSet_Cobalt if their typeface fails to load properly. As a
    // result, it is not safe to store their pointers outside of
    // SkFontStyleSet_Cobalt.
    SkFontStyleSetEntry_Cobalt(
        const SkString& file_path, const int face_index,
        const ComputedVariationPosition& computed_variation_position,
        const SkFontStyle& style, const std::string& full_name,
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
      const FontFamilyInfo& family_info, const char* base_path,
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
  sk_sp<SkTypeface> createTypeface(int index) override;

  sk_sp<SkTypeface> matchStyle(const SkFontStyle& pattern) override;

  const SkString& get_family_name() const { return family_name_; }

  const SkLanguage& get_language() const { return language_; }

 private:
  // NOTE: It is the responsibility of the caller to lock the mutex before
  // calling any of the non-const private functions.

  sk_sp<SkTypeface> MatchStyleWithoutLocking(const SkFontStyle& pattern);
  sk_sp<SkTypeface> MatchFullFontName(const std::string& name);
  sk_sp<SkTypeface> MatchFontPostScriptName(const std::string& name);
  SkTypeface* TryRetrieveTypefaceAndRemoveStyleOnFailure(int style_index);
  bool ContainsTypeface(const SkTypeface* typeface);

  bool ContainsCharacter(const SkFontStyle& style, SkUnichar character);
  bool CharacterMapContainsCharacter(
      SkUnichar character,
      const scoped_refptr<font_character_map::CharacterMap>& map);

  bool GenerateStyleFaceInfo(SkFontStyleSetEntry_Cobalt* style,
                             SkStreamAsset* stream, int style_index);

  int GetClosestStyleIndex(const SkFontStyle& pattern);
  void CreateStreamProviderTypeface(
      SkFontStyleSetEntry_Cobalt* style, int style_index,
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

  skia_private::TArray<sk_sp<SkFontStyleSetEntry_Cobalt>, true> styles_;

  friend class SkFontMgr_Cobalt;
};

typedef base::small_map<
    std::unordered_map<size_t, scoped_refptr<const SkFileMemoryChunk>>, 8>
    SkFileMemoryChunks;
typedef std::unordered_map<std::string, SkFileMemoryChunkStreamProvider*>
    SkFileMemoryChunkStreamProviderMap;

class SkFileMemoryChunk : public base::RefCountedThreadSafe<SkFileMemoryChunk> {
 public:
  static const size_t kSizeInBytes = 32 * 1024;
  uint8_t memory[kSizeInBytes];
};

// This class, which is thread-safe, is Cobalt's implementation of SkFontMgr. It
// is responsible for the creation of remote typefaces and for, given a set of
// constraints, providing Cobalt with its best matching local typeface.
//
// When a remote typeface's raw data is downloaded from the web, it is provided
// to the font manager, which returns a typeface created from that data.
//
// For local typefaces, the font manager uses Cobalt-specific and
// system-specific configuration files to generate named font families and
// fallback font families. Cobalt uses named families when it needs a typeface
// for a family with a specific name. It utilizes fallback families when none
// of the listed named families supported a given character. In that case,
// the manager finds the best match among the fallback families that can provide
// a glyph for that character.
//
// For both named families and fallback families, after the manager locates the
// best matching family, the determination of the specific typeface to use is
// left to the family. The manager provides the family with the requested style
// and the family returns the typeface that best fits that style.
class SkFontMgr_Cobalt : public SkFontMgr {
 public:
  typedef std::vector<SkFontStyleSet_Cobalt*> StyleSetArray;
  typedef std::map<int, StyleSetArray> PriorityStyleSetArrayMap;

  SkFontMgr_Cobalt(const char* cobalt_font_config_directory,
                   const char* cobalt_font_files_directory,
                   const char* system_font_config_directory,
                   const char* system_font_files_directory,
                   const skia_private::TArray<SkString, true>& default_fonts);

  // Purges all font caching in Skia and the local stream manager.
  void PurgeCaches();

  // NOTE: This returns NULL if a match is not found.
  sk_sp<SkTypeface> MatchFaceName(const char face_name[]);

  // Loads the font that matches the suggested script for the device's locale.
  void LoadLocaleDefault();
  // Clears the font that matches the suggested script for the device's locale.
  void ClearLocaleDefault();

 protected:
  // From SkFontMgr
  int onCountFamilies() const override;

  void onGetFamilyName(int index, SkString* family_name) const override;

  // NOTE: This returns NULL if there is no accessible style set at the index.
  sk_sp<SkFontStyleSet> onCreateStyleSet(int index) const override;

  // NOTE: This returns NULL if there is no family match.
  sk_sp<SkFontStyleSet> onMatchFamily(const char family_name[]) const override;

  // NOTE: This always returns a non-NULL value. If the family name cannot be
  // found, then the best match among the default family is returned.
  sk_sp<SkTypeface> onMatchFamilyStyle(const char family_name[],
                                 const SkFontStyle& style) const override;

  // NOTE: This always returns a non-NULL value. If no match can be found, then
  // the best match among the default family is returned.
  sk_sp<SkTypeface> onMatchFamilyStyleCharacter(const char family_name[],
                                          const SkFontStyle& style,
                                          const char* bcp47[], int bcp47_count,
                                          SkUnichar character) const override;

  // NOTE: This returns NULL if the typeface cannot be created.
  sk_sp<SkTypeface> onMakeFromData(sk_sp<SkData> data,
                                   int face_index) const override;

  // NOTE: This returns NULL if the typeface cannot be created.
  sk_sp<SkTypeface> onMakeFromStreamArgs(std::unique_ptr<SkStreamAsset>,
                                         const SkFontArguments&) const override;

  // NOTE: This returns NULL if the typeface cannot be created.
  sk_sp<SkTypeface> onMakeFromStreamIndex(std::unique_ptr<SkStreamAsset> stream,
                                          int face_index) const override;

  // NOTE: This returns NULL if the typeface cannot be created.
  sk_sp<SkTypeface> onMakeFromFile(const char path[],
                                   int face_index) const override;

  // NOTE: This always returns a non-NULL value. If no match can be found, then
  // the best match among the default family is returned.
  sk_sp<SkTypeface> onLegacyMakeTypeface(const char family_name[],
                                         SkFontStyle style) const override;

 private:
  typedef std::unordered_map<std::string, SkFontStyleSet_Cobalt*> NameToStyleSetMap;
  typedef base::small_map<std::unordered_map<std::string, StyleSetArray*>>
      NameToStyleSetArrayMap;

  void ParseConfigAndBuildFamilies(
      const char* font_config_directory, const char* font_files_directory,
      PriorityStyleSetArrayMap* priority_fallback_families);
  void BuildNameToFamilyMap(
      const char* font_files_directory,
      SkTDArray<FontFamilyInfo*>* config_font_families,
      PriorityStyleSetArrayMap* priority_fallback_families);
  void GeneratePriorityOrderedFallbackFamilies(
      const PriorityStyleSetArrayMap& priority_fallback_families);
  void FindDefaultFamily(const skia_private::TArray<SkString, true>& default_families);
  bool CheckIfFamilyMatchesLocaleScript(sk_sp<SkFontStyleSet_Cobalt> new_family,
                                        const char* script);

  // Returns the first encountered fallback family that matches the language tag
  // and supports the specified character.
  // NOTE: |style_sets_mutex_| should be locked prior to calling this function.
  sk_sp<SkTypeface> FindFamilyStyleCharacter(const SkFontStyle& style,
                                       const SkString& language_tag,
                                       SkUnichar character);

  // Returns every fallback family that matches the language tag. If the tag is
  // empty, then all fallback families are returned.
  // NOTE: |style_sets_mutex_| should be locked prior to calling this function.
  StyleSetArray* GetMatchingFallbackFamilies(const SkString& language_tag);

  SkFileMemoryChunkStreamManager local_typeface_stream_manager_;

  skia_private::TArray<sk_sp<SkFontStyleSet_Cobalt>, true> families_;

  skia_private::TArray<SkString> family_names_;
  // Map names to the back end so that all names for a given family refer to
  // the same (non-replicated) set of typefaces.
  NameToStyleSetMap name_to_family_map_;
  NameToStyleSetMap full_font_name_to_family_map_;
  NameToStyleSetMap font_postscript_name_to_family_map_;

  // Fallback families that are used during character fallback.
  // All fallback families, regardless of language.
  StyleSetArray fallback_families_;
  // Language-specific fallback families. These are lazily populated from
  // |fallback_families_| when a new language tag is requested.
  std::vector<std::unique_ptr<StyleSetArray>> language_fallback_families_array_;
  NameToStyleSetArrayMap language_fallback_families_map_;

  // List of default families that are used when no specific match is found
  // during a request.
  std::vector<sk_sp<SkFontStyleSet_Cobalt>> default_families_;

  // List of initial families used for all locales.
  std::vector<sk_sp<SkFontStyleSet_Cobalt>> initial_families_;

  // Used to delay font loading until default fonts are fully loaded.
  base::WaitableEvent default_fonts_loaded_event_;

  // Mutex shared by all families for accessing their modifiable data.
  mutable SkMutex family_mutex_;
};

#endif  // COBALT_RENDERER_RASTERIZER_SKIA_SKIA_SRC_PORTS_SKFONTMGR_COBALT_H_
