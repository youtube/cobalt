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

// Rollback PoC (option C of "C25->C27->C25 Upgrade and Rollback Storage
// Handling"): after a rollback from Cobalt 27, Cobalt 25 deletes the storage
// that Cobalt 27 left behind, so that it neither fills the persistent
// partition nor makes a later re-upgrade skip the storage migration.
// Throwaway prototype code; DO NOT SUBMIT.

#ifndef COBALT_STORAGE_C27_STORAGE_CLEANUP_H_
#define COBALT_STORAGE_C27_STORAGE_CLEANUP_H_

#include <string>

#include "base/files/file_path.h"

namespace cobalt {
namespace storage {

// Cobalt 27 keeps its profile (cookies, localStorage, ...) in
// kSbSystemPathFilesDirectory, which does not exist in the Starboard versions
// Cobalt 25 runs on (it was added in Starboard 17). The location is therefore
// derived from the default layout of the Linux and RDK reference ports, where
// it is the ".cobalt_files" sibling of kSbSystemPathStorageDirectory
// ("$HOME/.cobalt_storage"). Other ports, and RDK with COBALT_FILES_DIR set,
// put it elsewhere.
extern const char kC27FilesDirectoryName[];

// Written by Cobalt 27 on its first launch, after it has imported the Cobalt 25
// storage record or found nothing to import. While it exists, Cobalt 27 skips
// the import.
extern const char kC27MigrationMarkerFileName[];

// Written by the option B prototype next to the migration marker.
extern const char kC27LegacyRecordDigestFileName[];

// Port of Cobalt 27's starboard::GetAppKey(), which names its per-app
// directories. |url| is the raw --url value, or empty if the switch is absent
// (the default URL is used). User info, query and fragment are stripped as
// text, the rest is web-safe base64 encoded, and only the last
// (kSbFileMaxName - 24) characters are kept. This differs from
// base::GetApplicationKey() for long URLs and for URLs that GURL rewrites.
std::string GetC27AppKey(const std::string& url);

// Deletes what Cobalt 27 left behind for the app with key |c27_app_key|:
//   <dirname(storage_dir)>/.cobalt_files/<key>/   the Cobalt 27 profile;
//       ".cobalt_files" itself is removed too if that leaves it empty.
//   <cache_dir>/<key>/   Cobalt 27's cache directory on Starboard device
//       builds. It also holds the migration marker.
//   <dirname(cache_dir)>/{migration_completed.txt,legacy_record_sha256.txt}
//       where linux-x64x11 developer builds of Cobalt 27 put the marker when
//       XDG_CACHE_HOME is unset. Only regular files or links are removed here.
// |storage_dir| is kSbSystemPathStorageDirectory and |cache_dir| is
// kSbSystemPathCacheDirectory. A path that is empty or not absolute skips the
// locations derived from it. Nothing that Cobalt 25 itself writes is touched,
// and symbolic links are removed, never followed.
//
// Returns the size of the files deleted (0 if there was nothing to delete; a
// link to a file counts with that file's size), or -1 if |c27_app_key| is not
// usable as a directory name.
int64_t DeleteOrphanedC27Storage(const base::FilePath& storage_dir,
                                 const base::FilePath& cache_dir,
                                 const std::string& c27_app_key);

// Looks up the two directories through Starboard and calls
// DeleteOrphanedC27Storage() for GetC27AppKey(|url|). Blocks on file I/O. Call
// it once at startup, before the storage manager is created.
void DeleteOrphanedC27StorageAtStartup(const std::string& url);

}  // namespace storage
}  // namespace cobalt

#endif  // COBALT_STORAGE_C27_STORAGE_CLEANUP_H_
