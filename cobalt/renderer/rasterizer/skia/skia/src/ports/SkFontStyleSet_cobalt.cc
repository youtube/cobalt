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

#include "cobalt/renderer/rasterizer/skia/skia/src/ports/SkFontStyleSet_cobalt.h"

#include <cmath>
#include <limits>
#include <memory>

#include "SkOSFile.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/trace_event/trace_event.h"
#include "cobalt/renderer/rasterizer/skia/skia/src/ports/SkFreeType_cobalt.h"
#include "cobalt/renderer/rasterizer/skia/skia/src/ports/SkTypeface_cobalt.h"
#include "starboard/common/string.h"
#include "third_party/skia/src/utils/SkOSPath.h"

namespace {

int MatchScore(const SkFontStyle& pattern, const SkFontStyle& candidate) {
  // This logic is taken from Skia and is based upon the algorithm specified
  // within the spec:
  //   https://www.w3.org/TR/css-fonts-3/#font-matching-algorithm

  int score = 0;

  // CSS style (italic/oblique)
  // Being italic trumps all valid weights which are not italic.
  // Note that newer specs differentiate between italic and oblique.
  if ((pattern.slant() == SkFontStyle::kItalic_Slant) ==
      (candidate.slant() == SkFontStyle::kItalic_Slant)) {
    score += 1001;
  }

  // The 'closer' to the target weight, the higher the score.
  // 1000 is the 'heaviest' recognized weight
  if (pattern.weight() == candidate.weight()) {
    score += 1001;
  } else if (pattern.weight() <= 500) {
    if (400 <= pattern.weight() && pattern.weight() < 450) {
      if (450 <= candidate.weight() && candidate.weight() <= 500) {
        score += 500;
      }
    }
    if (candidate.weight() <= pattern.weight()) {
      score += 1000 - pattern.weight() + candidate.weight();
    } else {
      score += 1000 - candidate.weight();
    }
  } else if (pattern.weight() > 500) {
    if (candidate.weight() > pattern.weight()) {
      score += 1000 + pattern.weight() - candidate.weight();
    } else {
      score += candidate.weight();
    }
  }

  return score;
}


// Calculate the variation design coordinates for use with
// FT_Set_Var_Design_Coordinates or passed to SkFontData.
bool ComputeVariationPosition(
    const sk_freetype_cobalt::AxisDefinitions& axis_definitions,
    const FontFileInfo::VariationPosition& variation_position,
    SkFontStyleSet_Cobalt::ComputedVariationPosition* out_position) {
  if (variation_position.count() > axis_definitions.count()) {
    // More variation axes were specified than actually supported.
    return false;
  }

  int positions_used = 0;
  out_position->resize(axis_definitions.count());
  for (int axis = 0; axis < axis_definitions.count(); ++axis) {
    // See if this axis has a specified position. If not, then use the default
    // value from the axis definitions.
    Fixed16 value = axis_definitions[axis].def;
    for (int pos = 0; pos < variation_position.count(); ++pos) {
      if (variation_position[pos].tag == axis_definitions[axis].tag) {
        value = variation_position[pos].value;
        DCHECK(value >= axis_definitions[axis].minimum);
        DCHECK(value <= axis_definitions[axis].maximum);
        if (value < axis_definitions[axis].minimum ||
            value > axis_definitions[axis].maximum) {
          return false;
        }
        ++positions_used;
        break;
      }
    }
    (*out_position)[axis] = value;
  }

  if (positions_used != variation_position.count()) {
    // Some axes were specified which aren't supported.
    LOG(ERROR) << "Mismatched font variation tags specified";
    return false;
  }

  return true;
}

}  // namespace

SkFontStyleSet_Cobalt::SkFontStyleSet_Cobalt(
    const FontFamilyInfo& family_info, const char* base_path,
    SkFileMemoryChunkStreamManager* const local_typeface_stream_manager,
    SkMutex* const manager_owned_mutex, FontFormatSetting font_format_setting)
    : local_typeface_stream_manager_(local_typeface_stream_manager),
      manager_owned_mutex_(manager_owned_mutex),
      is_fallback_family_(family_info.is_fallback_family),
      language_(family_info.language),
      page_ranges_(family_info.page_ranges) {
  TRACE_EVENT0("cobalt::renderer",
               "SkFontStyleSet_Cobalt::SkFontStyleSet_Cobalt()");
  DCHECK(manager_owned_mutex_);

  if (family_info.names.count() == 0) {
    return;
  }

  disable_character_map_ = family_info.disable_caching;

  family_name_ = family_info.names[0];
  SkTHashMap<SkString, int> styles_index_map;

  // These structures support validation of entries for font variations.
  SkTHashMap<SkString, sk_freetype_cobalt::AxisDefinitions>
      variation_definitions;
  ComputedVariationPosition computed_variation_position;
  sk_freetype_cobalt::AxisDefinitions axis_definitions;

  // Avoid expensive alloc / dealloc calls with SkTArray by reserving size and
  // using resize(0) instead of reset(). Adobe multiple master fonts are limited
  // to 4 axes, and although OpenType font variations may have more, they tend
  // not to -- it's usually just weight, width, and slant. But just in case,
  // use a relatively high reservation.
  computed_variation_position.reserve(16);
  axis_definitions.reserve(16);

  for (int i = 0; i < family_info.fonts.count(); ++i) {
    const FontFileInfo& font_file = family_info.fonts[i];

    SkString file_path(SkOSPath::Join(base_path, font_file.file_name.c_str()));

    // Validate the font file entry.
    if (font_file.variation_position.empty()) {
      // Static font files only need to check for file existence.
      if (!sk_exists(file_path.c_str(), kRead_SkFILE_Flag)) {
        DLOG(INFO) << "Failed to find static font file: " << file_path.c_str();
        continue;
      }
      computed_variation_position.resize(0);
    } else {
      // Need to scan the font file to verify variation parameters. To improve
      // performance, cache the scan results.

      // In case axis definition changes based on font index, include font
      // index in the key.
      SkString cache_key(font_file.file_name);
      char cache_key_suffix[3] = {':', static_cast<char>(font_file.index + 'A'),
                                  0};
      cache_key.append(cache_key_suffix);

      sk_freetype_cobalt::AxisDefinitions* cached_axis_definitions =
          variation_definitions.find(cache_key);

      if (cached_axis_definitions == nullptr) {
        // Scan the font file to get the variation information (if any).
        if (!sk_exists(file_path.c_str(), kRead_SkFILE_Flag)) {
          // Add an empty axis definition list as placeholder. This will
          // be detected as an incompatible font file.
          axis_definitions.resize(0);
        } else {
          DLOG(INFO) << "Scanning variable font file: " << file_path.c_str();

          // Create a stream to scan the font. Since these fonts may not ever
          // be used at runtime, purge stream's chunks after scanning to avoid
          // memory bloat.
          SkFileMemoryChunkStreamProvider* stream_provider =
              local_typeface_stream_manager_->GetStreamProvider(
                  file_path.c_str());
          std::unique_ptr<const SkFileMemoryChunks> memory_chunks_snapshot(
              stream_provider->CreateMemoryChunksSnapshot());
          std::unique_ptr<SkFileMemoryChunkStream> stream(
              stream_provider->OpenStream());

          SkString unused_face_name;
          SkFontStyle unused_font_style;
          bool unused_is_fixed_pitch;
          if (!sk_freetype_cobalt::ScanFont(
                  stream.get(), font_file.index, &unused_face_name,
                  &unused_font_style, &unused_is_fixed_pitch, &axis_definitions,
                  nullptr)) {
            axis_definitions.resize(0);
          }

          stream.reset(nullptr);
          stream_provider->PurgeUnusedMemoryChunks();
        }

        cached_axis_definitions =
            variation_definitions.set(cache_key, axis_definitions);
      }

      if (!ComputeVariationPosition(*cached_axis_definitions,
                                    font_file.variation_position,
                                    &computed_variation_position)) {
        DLOG(INFO) << "Incompatible variable font file: " << file_path.c_str();
        continue;
      }
    }

    auto file_name = font_file.file_name.c_str();
    auto extension = strrchr(file_name, '.');
    if (!extension) {
      continue;
    }
    ++extension;

    // Only add font formats that match the format setting.
    if (font_format_setting == kTtf) {
      if (strcasecmp("ttf", extension) != 0 &&
          strcasecmp(extension, "ttc") != 0) {
        continue;
      }
    } else if (font_format_setting == kWoff2 &&
               strcasecmp("woff2", extension) != 0) {
      continue;
    }

    SkFontStyle style(font_file.weight, SkFontStyle::kNormal_Width,
                      font_file.style == FontFileInfo::kItalic_FontStyle
                          ? SkFontStyle::kItalic_Slant
                          : SkFontStyle::kUpright_Slant);

    std::string full_font_name;
    if (!font_file.full_font_name.isEmpty()) {
      full_font_name = std::string(font_file.full_font_name.c_str(),
                                   font_file.full_font_name.size());
    }
    std::string postscript_name;
    if (!font_file.postscript_name.isEmpty()) {
      postscript_name = std::string(font_file.postscript_name.c_str(),
                                    font_file.postscript_name.size());
    }

    SkString font_name;
    if (full_font_name.empty()) {
      // If full font name is missing, use file name.
      size_t name_len = font_file.file_name.size() - strlen(extension) - 1;
      font_name = SkString(file_name, name_len);
    } else {
      font_name = SkString(full_font_name.c_str());
    }
    auto font = new SkFontStyleSetEntry_Cobalt(
        file_path, font_file.index, computed_variation_position, style,
        full_font_name, postscript_name, font_file.disable_synthetic_bolding);
    int* index = styles_index_map.find(font_name);
    if (index != nullptr) {
      // If style with name already exists in family, replace it.
      if (font_format_setting == kTtfPreferred &&
          strcasecmp("ttf", extension) == 0) {
        styles_[*index].reset(font);
      } else if (font_format_setting == kWoff2Preferred &&
                 strcasecmp("woff2", extension) == 0) {
        styles_[*index].reset(font);
      }
    } else {
      int count = styles_.count();
      styles_index_map.set(font_name, count);
      styles_.push_back().reset(font);
    }
  }
}

int SkFontStyleSet_Cobalt::count() {
  SkAutoMutexExclusive scoped_mutex(*manager_owned_mutex_);
  return styles_.count();
}

void SkFontStyleSet_Cobalt::getStyle(int index, SkFontStyle* style,
                                     SkString* name) {
  // SkFontStyleSet_Cobalt does not support publicly interacting with entries
  // via index, as entries can potentially be removed, thereby invalidating the
  // indices.
  NOTREACHED();
}

SkTypeface* SkFontStyleSet_Cobalt::createTypeface(int index) {
  // SkFontStyleSet_Cobalt does not support publicly interacting with entries
  // via index, as entries can potentially be removed, thereby invalidating the
  // indices.
  NOTREACHED();
  return NULL;
}

SkTypeface* SkFontStyleSet_Cobalt::matchStyle(const SkFontStyle& pattern) {
  SkAutoMutexExclusive scoped_mutex(*manager_owned_mutex_);
  return MatchStyleWithoutLocking(pattern);
}

SkTypeface* SkFontStyleSet_Cobalt::MatchStyleWithoutLocking(
    const SkFontStyle& pattern) {
  SkTypeface* typeface = NULL;
  while (typeface == NULL && styles_.count() > 0) {
    typeface = TryRetrieveTypefaceAndRemoveStyleOnFailure(
        GetClosestStyleIndex(pattern));
  }
  return typeface;
}

SkTypeface* SkFontStyleSet_Cobalt::MatchFullFontName(const std::string& name) {
  for (int i = 0; i < styles_.count(); ++i) {
    if (styles_[i]->full_font_name == name) {
      return TryRetrieveTypefaceAndRemoveStyleOnFailure(i);
    }
  }
  return NULL;
}

SkTypeface* SkFontStyleSet_Cobalt::MatchFontPostScriptName(
    const std::string& name) {
  for (int i = 0; i < styles_.count(); ++i) {
    if (styles_[i]->font_postscript_name == name) {
      return TryRetrieveTypefaceAndRemoveStyleOnFailure(i);
    }
  }
  return NULL;
}

SkTypeface* SkFontStyleSet_Cobalt::TryRetrieveTypefaceAndRemoveStyleOnFailure(
    int style_index) {
  DCHECK(style_index >= 0 && style_index < styles_.count());
  SkFontStyleSetEntry_Cobalt* style = styles_[style_index].get();
  // If the typeface doesn't already exist, then attempt to create it.
  if (style->typeface == NULL) {
    CreateStreamProviderTypeface(style, style_index);
    // If the creation attempt failed and the typeface is still NULL, then
    // remove the entry from the set's styles.
    if (style->typeface == NULL) {
      styles_[style_index].swap(styles_.back());
      styles_.pop_back();
      return NULL;
    }
  }
  return SkRef(style->typeface.get());
}

bool SkFontStyleSet_Cobalt::ContainsTypeface(const SkTypeface* typeface) {
  for (int i = 0; i < styles_.count(); ++i) {
    if (styles_[i]->typeface.get() == typeface) {
      return true;
    }
  }
  return false;
}

bool SkFontStyleSet_Cobalt::ContainsCharacter(const SkFontStyle& style,
                                              SkUnichar character) {
  // If page ranges exist for this style set, then verify that this character
  // falls within the ranges. Otherwise, the style set is treated as having a
  // page range containing all characters.
  size_t num_ranges = page_ranges_.count();
  if (num_ranges > 0) {
    int16 page = font_character_map::GetPage(character);

    // Verify that the last page range is not less than the page containing the
    // character. If it's less, then this character can't be contained
    // within the pages. Otherwise, this last range acts as a sentinel for the
    // search.
    if (page_ranges_[num_ranges - 1].second < page) {
      return false;
    }

    int range_index = 0;
    while (page_ranges_[range_index].second < page) {
      ++range_index;
    }

    if (page_ranges_[range_index].first > page) {
      return false;
    }
  }

  // If this point is reached, then the character is contained within one of
  // the font style set's page ranges. Now, verify that the specific character
  // is supported by the font style set.

  // The character map is lazily generated. Generate it now if it isn't already
  // generated.
  int style_index = GetClosestStyleIndex(style);
  if (!character_maps_[style_index]) {
    TRACE_EVENT0("cobalt::renderer",
                 "SkFontStyleSet_Cobalt::ContainsCharacter() and "
                 "!is_character_map_generated_");
    // Attempt to load the closest font style from the set. If it fails to load,
    // it will be removed from the set and, as long as font styles remain in the
    // set, the logic will be attempted again.
    while (styles_.count() > 0) {
      SkFontStyleSetEntry_Cobalt* closest_style = styles_[style_index].get();

      SkFileMemoryChunkStreamProvider* stream_provider =
          local_typeface_stream_manager_->GetStreamProvider(
              closest_style->font_file_path.c_str());

      // Create a snapshot prior to loading any additional memory chunks. In the
      // case where the typeface does not end up being created, this enables the
      // stream provider to purge newly created memory chunks, while retaining
      // any pre-existing ones.
      std::unique_ptr<const SkFileMemoryChunks> memory_chunks_snapshot(
          stream_provider->CreateMemoryChunksSnapshot());

      std::unique_ptr<SkFileMemoryChunkStream> stream(
          stream_provider->OpenStream());
      if (GenerateStyleFaceInfo(closest_style, stream.get(), style_index)) {
        if (CharacterMapContainsCharacter(character,
                                          character_maps_[style_index])) {
          CreateStreamProviderTypeface(closest_style, style_index,
                                       stream_provider);
          return true;
        } else {
          // If a typeface was not created, destroy the stream and purge any
          // newly created memory chunks. The stream must be destroyed first or
          // it will retain references to memory chunks, preventing them from
          // being purged.
          stream.reset(nullptr);
          stream_provider->PurgeUnusedMemoryChunks();
          return false;
        }
      }

      styles_[style_index].swap(styles_.back());
      styles_.pop_back();
    }
  }

  DCHECK(character_maps_[style_index]);
  return CharacterMapContainsCharacter(character, character_maps_[style_index]);
}

bool SkFontStyleSet_Cobalt::CharacterMapContainsCharacter(
    SkUnichar character,
    const scoped_refptr<font_character_map::CharacterMap>& map) {
  font_character_map::Character c = map->Find(character);
  return c.is_set && c.id > 0;
}

bool SkFontStyleSet_Cobalt::GenerateStyleFaceInfo(
    SkFontStyleSetEntry_Cobalt* style, SkStreamAsset* stream, int style_index) {
  if (style->is_face_info_generated) {
    return true;
  }

  // Providing a pointer to the character map will cause it to be generated
  // during ScanFont. Only provide it if it hasn't already been generated.

  font_character_map::CharacterMap* character_map = NULL;
  if (!character_maps_[style_index]) {
    character_maps_[style_index] =
        base::MakeRefCounted<font_character_map::CharacterMap>();
    character_map = character_maps_[style_index].get();
  }

  SkFontStyle old_style = style->font_style;
  if (!sk_freetype_cobalt::ScanFont(
          stream, style->face_index, &style->face_name, &style->font_style,
          &style->face_is_fixed_pitch, nullptr, character_map)) {
    return false;
  }

  if (style->computed_variation_position.count() > 0) {
    // For font variations, use the font style parsed from fonts.xml rather than
    // the style returned by ScanFont since that's just the default.
    style->font_style = old_style;
  }

  style->is_face_info_generated = true;
  return true;
}

int SkFontStyleSet_Cobalt::GetClosestStyleIndex(const SkFontStyle& pattern) {
  int closest_index = 0;
  int max_score = std::numeric_limits<int>::min();
  for (int i = 0; i < styles_.count(); ++i) {
    int score = MatchScore(pattern, styles_[i]->font_style);
    if (styles_[i]->computed_variation_position.count() > 0) {
      // Slightly prefer static fonts over font variations to maintain old look.
      score -= 1;
    }
    if (score > max_score) {
      closest_index = i;
      max_score = score;
    }
  }
  return closest_index;
}

void SkFontStyleSet_Cobalt::CreateStreamProviderTypeface(
    SkFontStyleSetEntry_Cobalt* style_entry, int style_index,
    SkFileMemoryChunkStreamProvider* stream_provider /*=NULL*/) {
  TRACE_EVENT0("cobalt::renderer",
               "SkFontStyleSet_Cobalt::CreateStreamProviderTypeface()");

  if (!stream_provider) {
    stream_provider = local_typeface_stream_manager_->GetStreamProvider(
        style_entry->font_file_path.c_str());
  }

  std::unique_ptr<SkFileMemoryChunkStream> stream(
      stream_provider->OpenStream());
  if (GenerateStyleFaceInfo(style_entry, stream.get(), style_index)) {
    LOG(INFO) << "Scanned font from file: " << style_entry->face_name.c_str()
              << "(" << style_entry->font_style.weight() << ", "
              << style_entry->font_style.width() << ", "
              << style_entry->font_style.slant() << ")";
    scoped_refptr<font_character_map::CharacterMap> map =
        disable_character_map_ ? NULL : character_maps_[style_index];
    style_entry->typeface.reset(new SkTypeface_CobaltStreamProvider(
        stream_provider, style_entry->face_index, style_entry->font_style,
        style_entry->face_is_fixed_pitch, family_name_,
        style_entry->disable_synthetic_bolding,
        style_entry->computed_variation_position, map));
  } else {
    LOG(ERROR) << "Failed to scan font: "
               << style_entry->font_file_path.c_str();
  }
}

void SkFontStyleSet_Cobalt::PurgeUnreferencedTypefaces() {
  // Walk each of the styles looking for any that have a non-NULL typeface.
  // These are purged if they are unreferenced outside of the style set.
  for (int i = 0; i < styles_.count(); ++i) {
    sk_sp<SkTypeface>& typeface = styles_[i]->typeface;
    if (typeface.get() != NULL && typeface->unique()) {
      typeface.reset(NULL);
    }
  }
}
