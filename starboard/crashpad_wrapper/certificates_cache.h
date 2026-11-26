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

#ifndef STARBOARD_CRASHPAD_WRAPPER_CERTIFICATES_CACHE_H_
#define STARBOARD_CRASHPAD_WRAPPER_CERTIFICATES_CACHE_H_

#include <optional>
#include <string>

#include "base/files/file_path.h"

namespace crashpad {

// Keeps a copy of the CA certificates in `ca_certificates_path` in
// `cache_directory_path`/certs, and returns the path of the copy. The copy is
// made again only when the certificates may have changed, since copying them
// out of an APK takes over 100 ms: when `install_stamp`, the manifest.json of
// the Evergreen package they ship in, or the certificate names change.
// `install_stamp` identifies the installed app and changes when the app is
// installed or updated. With neither an install stamp nor a manifest.json, the
// certificates are copied every time. Returns std::nullopt if the copy fails.
std::optional<base::FilePath> SyncCACertificatesCache(
    const std::string& ca_certificates_path,
    const std::string& cache_directory_path,
    const std::string& install_stamp);

}  // namespace crashpad

#endif  // STARBOARD_CRASHPAD_WRAPPER_CERTIFICATES_CACHE_H_
