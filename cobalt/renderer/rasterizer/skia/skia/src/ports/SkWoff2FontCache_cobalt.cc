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

#include <sys/stat.h>

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/files/file.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/important_file_writer.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/timer/elapsed_timer.h"
#include "woff2/decode.h"
#include "woff2/output.h"

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

bool IsWoff2File(const base::FilePath& path) {
  return path.MatchesFinalExtension(FILE_PATH_LITERAL(".woff2"));
}

// Equivalent of base::GetFileInfo() without its base::ScopedBlockingCall,
// which DCHECKs on threads where blocking is disallowed (e.g. the browser UI
// thread when it creates the default typeface). Like the rest of the font
// loading path (sk_exists(), SkStream::MakeFromFile()), the cache lookup
// accesses local files directly; it only needs file metadata, so it calls
// stat(). File::Info::FromStat() keeps the result identical to
// base::GetFileInfo(), and therefore the cache key stable.
bool StatFile(const base::FilePath& path, base::File::Info* info) {
  base::stat_wrapper_t file_stat;
  if (stat(path.value().c_str(), &file_stat) != 0) {
    return false;
  }
  info->FromStat(file_stat);
  return true;
}

// Gets the font cache directory, <DIR_CACHE>/font_cache. This does not
// block: DIR_CACHE is resolved (and cached by PathService) early during
// browser startup, e.g. by GlobalFeatures before the FeatureList that gates
// this feature is created, so the PathService lookup is an in-memory read.
bool GetCacheDir(base::FilePath* cache_dir) {
  base::FilePath cache_root;
  if (!base::PathService::Get(base::DIR_CACHE, &cache_root)) {
    LOG(WARNING) << "CobaltMmapFontCache: no cache directory available.";
    return false;
  }
  *cache_dir = cache_root.Append(FILE_PATH_LITERAL("font_cache"));
  return true;
}

// Returns the path of the cache file of the WOFF2 font at |woff2_path|, or an
// empty path for non-WOFF2 files or if the font or the cache directory is
// unavailable. The file name is keyed on the font's basename, size and mtime,
// so a changed font gets a new cache file. The decompressed bytes may be a
// TTF or TTC; the extension is only cosmetic. Does not block; see StatFile()
// and GetCacheDir().
base::FilePath GetCacheFile(const base::FilePath& woff2_path) {
  base::File::Info info;
  base::FilePath cache_dir;
  if (!IsWoff2File(woff2_path) || !StatFile(woff2_path, &info) ||
      !GetCacheDir(&cache_dir)) {
    return base::FilePath();
  }
  return cache_dir.Append(
      woff2_path.BaseName().RemoveExtension().value() + "." +
      base::NumberToString(info.size) + "." +
      base::NumberToString(static_cast<int64_t>(info.last_modified.ToTimeT())) +
      ".ttf");
}

// Returns true if |cache_file| exists and is a non-empty file. Does not
// block; see StatFile().
bool IsCacheFilePresent(const base::FilePath& cache_file) {
  base::File::Info info;
  return StatFile(cache_file, &info) && !info.is_directory && info.size > 0;
}

// Decompresses the WOFF2 font |woff2| into a buffer of the SFNT size declared
// in its header (a font that exceeds it fails to decompress). Returns the SFNT
// bytes, or null on failure.
sk_sp<SkData> DecompressWoff2Data(const SkData& woff2) {
  const size_t sfnt_size =
      woff2::ComputeWOFF2FinalSize(woff2.bytes(), woff2.size());
  if (sfnt_size == 0 || sfnt_size > woff2::kDefaultMaxSize) {
    return nullptr;
  }
  sk_sp<SkData> sfnt = SkData::MakeUninitialized(sfnt_size);
  woff2::WOFF2MemoryOut out(static_cast<uint8_t*>(sfnt->writable_data()),
                            sfnt_size);
  if (!woff2::ConvertWOFF2ToTTF(woff2.bytes(), woff2.size(), &out)) {
    return nullptr;
  }
  return out.Size() == sfnt_size
             ? sfnt
             : SkData::MakeSubset(sfnt.get(), 0, out.Size());
}

// Writes |sfnt| to the cache file of the WOFF2 font at |woff2_path|.
// base::ImportantFileWriter writes it to a temporary file in the same
// directory, flushes that to disk and renames it to the cache file, so the
// cache file is never seen partially written.
void WriteCacheFile(const base::FilePath& woff2_path, sk_sp<SkData> sfnt) {
  const base::FilePath cache_file = GetCacheFile(woff2_path);
  if (cache_file.empty()) {
    return;
  }
  base::ElapsedTimer timer;
  if (!base::CreateDirectory(cache_file.DirName()) ||
      !base::ImportantFileWriter::WriteFileAtomically(
          cache_file, std::string_view(static_cast<const char*>(sfnt->data()),
                                       sfnt->size()))) {
    LOG(ERROR) << "CobaltMmapFontCache: failed to write " << cache_file.value();
    return;
  }
  LOG(INFO) << "CobaltMmapFontCache: wrote " << cache_file.value() << " in "
            << timer.Elapsed();
}

// Deletes every file in the cache directory other than the cache files of the
// current version of the WOFF2 fonts in |font_file_paths|. This covers the
// cache files of fonts that were updated or removed (e.g. by a firmware
// update) and the temporary files of interrupted cache file writes.
void DeleteStaleCacheFiles(const std::vector<std::string>& font_file_paths) {
  base::FilePath cache_dir;
  if (!GetCacheDir(&cache_dir)) {
    return;
  }

  base::flat_set<base::FilePath> current_cache_files;
  for (const std::string& font_file_path : font_file_paths) {
    base::FilePath cache_file = GetCacheFile(base::FilePath(font_file_path));
    if (!cache_file.empty()) {
      current_cache_files.insert(std::move(cache_file));
    }
  }

  // This also lists hidden files, such as the temporary files.
  base::FileEnumerator enumerator(cache_dir, /*recursive=*/false,
                                  base::FileEnumerator::FILES);
  for (base::FilePath path = enumerator.Next(); !path.empty();
       path = enumerator.Next()) {
    if (!current_cache_files.contains(path)) {
      LOG(INFO) << "CobaltMmapFontCache: deleting stale " << path.value();
      base::DeleteFile(path);
    }
  }
}

// Returns the sequence that runs the background work of the cache (the cache
// file writes and the cleanup), or null if there is no ThreadPool. A sequence
// runs one task at a time, in posting order, which guarantees that the
// cleanup never deletes the temporary file of a cache file that is being
// written.
scoped_refptr<base::SequencedTaskRunner> GetTaskRunner() {
  base::ThreadPoolInstance* const thread_pool = base::ThreadPoolInstance::Get();
  if (!thread_pool) {
    return nullptr;
  }

  static base::NoDestructor<base::Lock> lock;
  static base::NoDestructor<scoped_refptr<base::SequencedTaskRunner>>
      task_runner;
  // The ThreadPool that |task_runner| posts to. It is only compared, never
  // dereferenced: tests replace the ThreadPool, and the task runners of an
  // earlier one no longer run tasks.
  static base::ThreadPoolInstance* task_runner_thread_pool = nullptr;

  base::AutoLock auto_lock(*lock);
  if (thread_pool != task_runner_thread_pool) {
    // BEST_EFFORT: the cache only benefits later app launches, so maintaining
    // it must not compete with user-visible work. CONTINUE_ON_SHUTDOWN: cache
    // files are written atomically (tmp + rename) and a temporary file left
    // behind is deleted by the next cleanup, so abandoning the work at
    // shutdown is safe and avoids delaying shutdown by a cache file write.
    *task_runner = base::ThreadPool::CreateSequencedTaskRunner(
        {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
         base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN});
    task_runner_thread_pool = thread_pool;
  }
  return *task_runner;
}

}  // namespace

SkString GetCachedSfntPath(const SkString& font_file_path) {
  const base::FilePath cache_file =
      GetCacheFile(base::FilePath(font_file_path.c_str()));
  if (cache_file.empty() || !IsCacheFilePresent(cache_file)) {
    return SkString();
  }
  return SkString(cache_file.value().c_str());
}

sk_sp<SkData> DecompressWoff2(const SkString& font_file_path) {
  if (!IsWoff2File(base::FilePath(font_file_path.c_str()))) {
    return nullptr;
  }
  // Maps the font like SkStream::MakeFromFile() maps cache files: Skia's file
  // API, unlike base's, does not assert that blocking is allowed.
  sk_sp<SkData> woff2 = SkData::MakeFromFileName(font_file_path.c_str());
  if (!woff2) {
    return nullptr;
  }

  base::ElapsedTimer timer;
  sk_sp<SkData> sfnt = DecompressWoff2Data(*woff2);
  if (!sfnt) {
    LOG(ERROR) << "CobaltMmapFontCache: failed to decompress "
               << font_file_path.c_str();
    return nullptr;
  }
  LOG(INFO) << "CobaltMmapFontCache: decompressed " << font_file_path.c_str()
            << " in " << timer.Elapsed();
  return sfnt;
}

void ScheduleCacheFileWrite(const SkString& font_file_path,
                            sk_sp<SkData> sfnt) {
  // Without a ThreadPool (e.g. unit tests without a TaskEnvironment), the
  // cache simply stays cold.
  base::FilePath woff2_path(font_file_path.c_str());
  scoped_refptr<base::SequencedTaskRunner> task_runner = GetTaskRunner();
  if (task_runner && sfnt && IsWoff2File(woff2_path)) {
    task_runner->PostTask(FROM_HERE,
                          base::BindOnce(&WriteCacheFile, std::move(woff2_path),
                                         std::move(sfnt)));
  }
}

void ScheduleCacheCleanup(std::vector<std::string> font_file_paths) {
  if (scoped_refptr<base::SequencedTaskRunner> task_runner = GetTaskRunner()) {
    task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(&DeleteStaleCacheFiles, std::move(font_file_paths)));
  }
}

}  // namespace sk_woff2_cache_cobalt
