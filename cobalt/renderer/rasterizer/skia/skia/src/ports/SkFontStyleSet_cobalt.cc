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

#include "cobalt/renderer/rasterizer/skia/skia/src/ports/SkFontStyleSet_cobalt.h"

#include <cmath>
#include <limits>

#include "SkOSFile.h"
#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "cobalt/renderer/rasterizer/skia/skia/src/ports/SkFreeType_cobalt.h"
#include "cobalt/renderer/rasterizer/skia/skia/src/ports/SkTypeface_cobalt.h"

namespace {

int MatchScore(const SkFontStyle& pattern, const SkFontStyle& candidate) {
  // This logic is taken from Skia and is based upon the algorithm specified
  // within the spec:
  //   https://www.w3.org/TR/css-fonts-3/#font-matching-algorithm

  int score = 0;

  // CSS style (italic/oblique)
  // Being italic trumps all valid weights which are not italic.
  // Note that newer specs differentiate between italic and oblique.
  if (pattern.isItalic() == candidate.isItalic()) {
    score += 1001;
  }

  // The 'closer' to the target weight, the higher the score.
  // 1000 is the 'heaviest' recognized weight
  if (pattern.weight() == candidate.weight()) {
    score += 1000;
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

}  // namespace

SkFontStyleSet_Cobalt::SkFontStyleSet_Cobalt(
    const FontFamilyInfo& family_info, const char* base_path,
    SkFileMemoryChunkStreamManager* const local_typeface_stream_manager,
    SkMutex* const manager_owned_mutex)
    : local_typeface_stream_manager_(local_typeface_stream_manager),
      manager_owned_mutex_(manager_owned_mutex),
      is_fallback_family_(family_info.is_fallback_family),
      language_(family_info.language),
      page_ranges_(family_info.page_ranges),
      is_character_map_generated_(!is_fallback_family_) {
  TRACE_EVENT0("cobalt::renderer",
               "SkFontStyleSet_Cobalt::SkFontStyleSet_Cobalt()");
  DCHECK(manager_owned_mutex_);

  if (family_info.names.count() == 0) {
    return;
  }

  family_name_ = family_info.names[0];

  for (int i = 0; i < family_info.fonts.count(); ++i) {
    const FontFileInfo& font_file = family_info.fonts[i];

    SkString file_path(SkOSPath::Join(base_path, font_file.file_name.c_str()));

    // Validate that the file exists at this location. If it does not, then skip
    // over it; it isn't being added to the set.
    if (!sk_exists(file_path.c_str(), kRead_SkFILE_Flag)) {
      DLOG(INFO) << "Failed to find font file: " << file_path.c_str();
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

    styles_.push_back().reset(
        SkNEW_ARGS(SkFontStyleSetEntry_Cobalt,
                   (file_path, font_file.index, style, full_font_name,
                    postscript_name, font_file.disable_synthetic_bolding)));
  }
}

int SkFontStyleSet_Cobalt::count() {
  SkAutoMutexAcquire scoped_mutex(*manager_owned_mutex_);
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
  SkAutoMutexAcquire scoped_mutex(*manager_owned_mutex_);
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
  SkFontStyleSetEntry_Cobalt* style = styles_[style_index];
  // If the typeface doesn't already exist, then attempt to create it.
  if (style->typeface == NULL) {
    CreateStreamProviderTypeface(style);
    // If the creation attempt failed and the typeface is still NULL, then
    // remove the entry from the set's styles.
    if (style->typeface == NULL) {
      styles_[style_index].swap(&styles_.back());
      styles_.pop_back();
      return NULL;
    }
  }
  return SkRef(style->typeface.get());
}

bool SkFontStyleSet_Cobalt::ContainsTypeface(const SkTypeface* typeface) {
  for (int i = 0; i < styles_.count(); ++i) {
    if (styles_[i]->typeface == typeface) {
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
  if (!is_character_map_generated_) {
    TRACE_EVENT0("cobalt::renderer",
                 "SkFontStyleSet_Cobalt::ContainsCharacter() and "
                 "!is_character_map_generated_");
    // Attempt to load the closest font style from the set. If it fails to load,
    // it will be removed from the set and, as long as font styles remain in the
    // set, the logic will be attempted again.
    while (styles_.count() > 0) {
      int style_index = GetClosestStyleIndex(style);
      SkFontStyleSetEntry_Cobalt* closest_style = styles_[style_index];

      SkFileMemoryChunkStreamProvider* stream_provider =
          local_typeface_stream_manager_->GetStreamProvider(
              closest_style->font_file_path.c_str());

      // Create a snapshot prior to loading any additional memory chunks. In the
      // case where the typeface does not end up being created, this enables the
      // stream provider to purge newly created memory chunks, while retaining
      // any pre-existing ones.
      scoped_ptr<const SkFileMemoryChunks> memory_chunks_snapshot(
          stream_provider->CreateMemoryChunksSnapshot());

      SkAutoTUnref<SkFileMemoryChunkStream> stream(
          stream_provider->OpenStream());
      if (GenerateStyleFaceInfo(closest_style, stream)) {
        if (CharacterMapContainsCharacter(character)) {
          CreateStreamProviderTypeface(closest_style, stream_provider);
          return true;
        } else {
          // If a typeface was not created, destroy the stream and purge any
          // newly created memory chunks. The stream must be destroyed first or
          // it will retain references to memory chunks, preventing them from
          // being purged.
          stream.reset(NULL);
          stream_provider->PurgeUnusedMemoryChunks();
          return false;
        }
      }

      styles_[style_index].swap(&styles_.back());
      styles_.pop_back();
    }

    is_character_map_generated_ = true;
  }

  return CharacterMapContainsCharacter(character);
}

bool SkFontStyleSet_Cobalt::CharacterMapContainsCharacter(SkUnichar character) {
  font_character_map::CharacterMap::iterator page_iterator =
      character_map_.find(font_character_map::GetPage(character));
  return page_iterator != character_map_.end() &&
         page_iterator->second.test(
             font_character_map::GetPageCharacterIndex(character));
}

bool SkFontStyleSet_Cobalt::GenerateStyleFaceInfo(
    SkFontStyleSetEntry_Cobalt* style, SkStreamAsset* stream) {
  if (style->is_face_info_generated) {
    return true;
  }

  // Providing a pointer to the character map will cause it to be generated
  // during ScanFont. Only provide it if it hasn't already been generated.
  font_character_map::CharacterMap* character_map =
      !is_character_map_generated_ ? &character_map_ : NULL;

  if (!sk_freetype_cobalt::ScanFont(
          stream, style->face_index, &style->face_name, &style->face_style,
          &style->face_is_fixed_pitch, character_map)) {
    return false;
  }

  style->is_face_info_generated = true;
  is_character_map_generated_ = true;
  return true;
}

int SkFontStyleSet_Cobalt::GetClosestStyleIndex(const SkFontStyle& pattern) {
  int closest_index = 0;
  int max_score = std::numeric_limits<int>::min();
  for (int i = 0; i < styles_.count(); ++i) {
    int score = MatchScore(pattern, styles_[i]->font_style);
    if (score > max_score) {
      closest_index = i;
      max_score = score;
    }
  }
  return closest_index;
}

void SkFontStyleSet_Cobalt::CreateStreamProviderTypeface(
    SkFontStyleSetEntry_Cobalt* style_entry,
    SkFileMemoryChunkStreamProvider* stream_provider /*=NULL*/) {
  TRACE_EVENT0("cobalt::renderer",
               "SkFontStyleSet_Cobalt::CreateStreamProviderTypeface()");

  if (!stream_provider) {
    stream_provider = local_typeface_stream_manager_->GetStreamProvider(
        style_entry->font_file_path.c_str());
  }

  SkAutoTUnref<SkFileMemoryChunkStream> stream(stream_provider->OpenStream());
  if (GenerateStyleFaceInfo(style_entry, stream)) {
    LOG(INFO) << "Scanned font from file: " << style_entry->face_name.c_str()
              << "(" << style_entry->face_style << ")";
    style_entry->typeface.reset(
        SkNEW_ARGS(SkTypeface_CobaltStreamProvider,
                   (stream_provider, style_entry->face_index,
                    style_entry->face_style, style_entry->face_is_fixed_pitch,
                    family_name_, style_entry->disable_synthetic_bolding)));
  } else {
    LOG(ERROR) << "Failed to scan font: "
               << style_entry->font_file_path.c_str();
  }
}

void SkFontStyleSet_Cobalt::PurgeUnreferencedTypefaces() {
  // Walk each of the styles looking for any that have a non-NULL typeface.
  // These are purged if they are unreferenced outside of the style set.
  for (int i = 0; i < styles_.count(); ++i) {
    SkAutoTUnref<SkTypeface>& typeface = styles_[i]->typeface;
    if (typeface.get() != NULL && typeface->unique()) {
      typeface.reset(NULL);
    }
  }
}
