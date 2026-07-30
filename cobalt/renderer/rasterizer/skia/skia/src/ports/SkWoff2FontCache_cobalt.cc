// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/renderer/rasterizer/skia/skia/src/ports/SkWoff2FontCache_cobalt.h"

#include <string.h>
#include <strings.h>

#include <string>
#include <string_view>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/timer/elapsed_timer.h"
#include "woff2/decode.h"

namespace sk_woff2_cache_cobalt {

BASE_FEATURE(kCobaltMmapFontCache,
             "CobaltMmapFontCache",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsMmapFontCacheEnabled() {
  // The font manager can be created lazily on first font use, which in some
  // processes may precede FeatureList initialization. Treat that as disabled
  // (today's in-heap path) rather than failing.
  if (!base::FeatureList::GetInstance()) {
    return false;
  }
  return base::FeatureList::IsEnabled(kCobaltMmapFontCache);
}

namespace {

// Decompresses the WOFF2 file at |woff2_path| and atomically writes the raw
// SFNT bytes to |cache_file| (tmp file in |cache_dir| + rename).
bool DecompressWoff2ToFile(const base::FilePath& woff2_path,
                           const base::FilePath& cache_dir,
                           const base::FilePath& cache_file) {
  std::string woff2_data;
  if (!base::ReadFileToString(woff2_path, &woff2_data)) {
    LOG(ERROR) << "CobaltMmapFontCache: failed to read " << woff2_path.value();
    return false;
  }

  const uint8_t* woff2_bytes =
      reinterpret_cast<const uint8_t*>(woff2_data.data());
  const size_t final_size =
      woff2::ComputeWOFF2FinalSize(woff2_bytes, woff2_data.size());
  if (final_size == 0 || final_size > woff2::kDefaultMaxSize) {
    LOG(ERROR) << "CobaltMmapFontCache: bad WOFF2 final size " << final_size
               << " for " << woff2_path.value();
    return false;
  }

  std::string sfnt_data;
  sfnt_data.reserve(final_size);
  woff2::WOFF2StringOut sfnt_out(&sfnt_data);
  if (!woff2::ConvertWOFF2ToTTF(woff2_bytes, woff2_data.size(), &sfnt_out)) {
    LOG(ERROR) << "CobaltMmapFontCache: WOFF2 decompression failed for "
               << woff2_path.value();
    return false;
  }

  base::FilePath temp_file;
  if (!base::CreateTemporaryFileInDir(cache_dir, &temp_file)) {
    return false;
  }
  // |sfnt_data| may have grown beyond the actual output; write Size() bytes.
  if (!base::WriteFile(temp_file,
                       std::string_view(sfnt_data.data(), sfnt_out.Size())) ||
      !base::ReplaceFile(temp_file, cache_file, nullptr)) {
    LOG(ERROR) << "CobaltMmapFontCache: failed to write " << cache_file.value();
    base::DeleteFile(temp_file);
    return false;
  }
  return true;
}

}  // namespace

SkString GetOrCreateCachedSfntPath(const SkString& font_file_path) {
  const char* extension = strrchr(font_file_path.c_str(), '.');
  if (!extension || strcasecmp(extension, ".woff2") != 0) {
    return SkString();
  }

  base::FilePath woff2_path(font_file_path.c_str());
  base::File::Info info;
  if (!base::GetFileInfo(woff2_path, &info)) {
    return SkString();
  }

  base::FilePath cache_dir;
  if (!base::PathService::Get(base::DIR_CACHE, &cache_dir)) {
    LOG(WARNING) << "CobaltMmapFontCache: no cache directory available.";
    return SkString();
  }
  cache_dir = cache_dir.Append(FILE_PATH_LITERAL("font_cache"));

  // Key the cache file on basename, source size and source mtime so a changed
  // source font produces a new cache entry. The decompressed bytes may be a
  // TTF or TTC; the extension is only cosmetic.
  const std::string cache_name =
      woff2_path.BaseName().RemoveExtension().value() + "." +
      base::NumberToString(info.size) + "." +
      base::NumberToString(static_cast<int64_t>(info.last_modified.ToTimeT())) +
      ".ttf";
  const base::FilePath cache_file = cache_dir.Append(cache_name);

  if (base::GetFileSize(cache_file).value_or(0) > 0) {
    return SkString(cache_file.value().c_str());
  }

  if (!base::CreateDirectory(cache_dir)) {
    LOG(ERROR) << "CobaltMmapFontCache: failed to create " << cache_dir.value();
    return SkString();
  }

  base::ElapsedTimer timer;
  if (!DecompressWoff2ToFile(woff2_path, cache_dir, cache_file)) {
    return SkString();
  }
  LOG(INFO) << "CobaltMmapFontCache: decompressed " << woff2_path.value()
            << " -> " << cache_file.value() << " in " << timer.Elapsed();
  return SkString(cache_file.value().c_str());
}

}  // namespace sk_woff2_cache_cobalt
