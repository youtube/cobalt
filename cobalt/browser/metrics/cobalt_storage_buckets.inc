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

// Storage buckets: every file and directory in Cobalt's storage is attributed
// to exactly one bucket by its path. This table is shared by the browser's
// storage footprint sampler (cobalt_storage_footprint.cc) and the lab tool
// cobalt/tools/storage/storage_monitor.py, which parses this file, so lab and
// field numbers use the same buckets. Use only the two macros below.
//
// COBALT_STORAGE_BUCKET(Bucket)
//   Declares a bucket. The name is part of the histogram name
//   Cobalt.Storage.Footprint.{Phase}.Bucket.{Bucket}.SizeKB: do not rename
//   buckets, and add new ones to the "CobaltStorageBucket" variants in
//   tools/metrics/histograms/metadata/cobalt/histograms.xml.
//
// COBALT_STORAGE_PATH(Root, "path", Bucket)
//   An entry belongs to |Bucket| if its path relative to |Root| is "path", is
//   inside it ("path/..."), or is a companion file ("path-journal",
//   "path-wal", "path-shm"). Rules are tried in order and the first match
//   wins; an empty "path" matches everything. Entries are classified against
//   the innermost root that contains them. Roots:
//   - UserData: content::SHELL_DIR_USER_DATA.
//   - DirCache: base::DIR_CACHE.
//   - AppData: Android only, the app's private data directory
//     (Context.getDataDir()), which contains UserData and DirCache.

// Chromium storage in UserData.
COBALT_STORAGE_BUCKET(Cookies)
COBALT_STORAGE_BUCKET(LocalStorage)
COBALT_STORAGE_BUCKET(IndexedDB)
COBALT_STORAGE_BUCKET(ServiceWorker)
COBALT_STORAGE_BUCKET(QuotaManager)
COBALT_STORAGE_BUCKET(DIPS)
COBALT_STORAGE_BUCKET(NetworkState)
COBALT_STORAGE_BUCKET(BlobStorage)
// Everything that kCobaltStorageCleanup deletes: caches that older builds kept
// in UserData (the GPU program cache stays in UserData/ShaderCache while
// kCobaltStorageCleanup is disabled), and the storage of the subsystems that it
// disables or keeps in memory.
COBALT_STORAGE_BUCKET(CleanupTargets)
COBALT_STORAGE_BUCKET(OtherUserData)
// Caches in DirCache, which the OS may clear.
COBALT_STORAGE_BUCKET(HttpCache)
COBALT_STORAGE_BUCKET(CodeCacheJs)
COBALT_STORAGE_BUCKET(CodeCacheWasm)
COBALT_STORAGE_BUCKET(GrShaderCache)
COBALT_STORAGE_BUCKET(ShaderCache)
COBALT_STORAGE_BUCKET(FontCache)
COBALT_STORAGE_BUCKET(Crashpad)
COBALT_STORAGE_BUCKET(OtherCache)
// Cobalt's persistent histogram (stability metrics) files.
COBALT_STORAGE_BUCKET(StabilityPma)
// Android app directories written by the Java app, outside Chromium's roots.
COBALT_STORAGE_BUCKET(AndroidFiles)
COBALT_STORAGE_BUCKET(AndroidNoBackup)
COBALT_STORAGE_BUCKET(AndroidDbAndPrefs)
COBALT_STORAGE_BUCKET(OtherAppData)

COBALT_STORAGE_PATH(UserData, "Cookies", Cookies)
COBALT_STORAGE_PATH(UserData, "Network/Cookies", Cookies)
COBALT_STORAGE_PATH(UserData, "Local Storage", LocalStorage)
COBALT_STORAGE_PATH(UserData, "IndexedDB", IndexedDB)
COBALT_STORAGE_PATH(UserData, "Service Worker", ServiceWorker)
COBALT_STORAGE_PATH(UserData, "WebStorage", QuotaManager)
COBALT_STORAGE_PATH(UserData, "DIPS", DIPS)
COBALT_STORAGE_PATH(UserData, "Network Persistent State", NetworkState)
COBALT_STORAGE_PATH(UserData, "TransportSecurity", NetworkState)
COBALT_STORAGE_PATH(UserData, "Reporting and NEL", NetworkState)
COBALT_STORAGE_PATH(UserData, "Network", NetworkState)
COBALT_STORAGE_PATH(UserData, "blob_storage", BlobStorage)
COBALT_STORAGE_PATH(UserData, "BrowserStabilityMetrics", StabilityPma)
// Keep in sync with DeleteOrphanedUserDataCacheDirectories() and
// DeleteUnusedChromiumStoragePaths() in cobalt_browser_main_parts.cc.
COBALT_STORAGE_PATH(UserData, "Cache", CleanupTargets)
COBALT_STORAGE_PATH(UserData, "Code Cache", CleanupTargets)
COBALT_STORAGE_PATH(UserData, "ShaderCache", CleanupTargets)
COBALT_STORAGE_PATH(UserData, "GrShaderCache", CleanupTargets)
// disk_cache::CleanupDirectorySync() renames a cache directory to
// old_<name>_000 and deletes it on another task.
COBALT_STORAGE_PATH(UserData, "old_Cache_000", CleanupTargets)
COBALT_STORAGE_PATH(UserData, "old_Code Cache_000", CleanupTargets)
COBALT_STORAGE_PATH(UserData, "old_ShaderCache_000", CleanupTargets)
COBALT_STORAGE_PATH(UserData, "old_GrShaderCache_000", CleanupTargets)
COBALT_STORAGE_PATH(UserData, "SCT Auditing Pending Reports", CleanupTargets)
COBALT_STORAGE_PATH(UserData, "Trust Tokens", CleanupTargets)
COBALT_STORAGE_PATH(UserData, "SharedStorage", CleanupTargets)
COBALT_STORAGE_PATH(UserData, "Shared Dictionary", CleanupTargets)
COBALT_STORAGE_PATH(UserData, "Session Storage", CleanupTargets)
COBALT_STORAGE_PATH(UserData, "VideoDecodeStats", CleanupTargets)
COBALT_STORAGE_PATH(UserData, "shared_proto_db", CleanupTargets)
COBALT_STORAGE_PATH(UserData, "", OtherUserData)

COBALT_STORAGE_PATH(DirCache, "Cache", HttpCache)
COBALT_STORAGE_PATH(DirCache, "Code Cache/js", CodeCacheJs)
COBALT_STORAGE_PATH(DirCache, "Code Cache/wasm", CodeCacheWasm)
COBALT_STORAGE_PATH(DirCache, "GrShaderCache", GrShaderCache)
COBALT_STORAGE_PATH(DirCache, "ShaderCache", ShaderCache)
COBALT_STORAGE_PATH(DirCache, "font_cache", FontCache)
COBALT_STORAGE_PATH(DirCache, "crashpad", Crashpad)
COBALT_STORAGE_PATH(DirCache, "", OtherCache)

COBALT_STORAGE_PATH(AppData,
                    "app_content_shell/BrowserStabilityMetrics",
                    StabilityPma)
COBALT_STORAGE_PATH(AppData, "files", AndroidFiles)
COBALT_STORAGE_PATH(AppData, "no_backup", AndroidNoBackup)
COBALT_STORAGE_PATH(AppData, "databases", AndroidDbAndPrefs)
COBALT_STORAGE_PATH(AppData, "shared_prefs", AndroidDbAndPrefs)
COBALT_STORAGE_PATH(AppData, "", OtherAppData)
