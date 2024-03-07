// Copyright 2023 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/common/paths.h"

#include <string>
#include <vector>

#include "starboard/common/log.h"
#include "starboard/configuration_constants.h"
#include "starboard/file.h"
#include "starboard/system.h"

namespace starboard {
namespace common {

namespace {

std::string GetCACertificatesPathFromSubdir(const std::string& content_subdir) {
  std::vector<char> buffer(kSbFileMaxPath);
  if (!SbSystemGetPath(kSbSystemPathContentDirectory, buffer.data(),
                       buffer.size())) {
    SB_LOG(ERROR) << "Failed to get system path content directory";
    return "";
  }

  std::string content_base_path(buffer.data());
  if (!content_subdir.empty()) {
    content_base_path += std::string(kSbFileSepString) + content_subdir;
  }
  return content_base_path + kSbFileSepString + "ssl" + kSbFileSepString +
         "certs";
}

}  // namespace

std::string PrependContentPath(const std::string& path) {
  std::vector<char> content_path(kSbFileMaxPath);

  if (!SbSystemGetPath(kSbSystemPathContentDirectory, content_path.data(),
                       kSbFileMaxPath)) {
    SB_LOG(ERROR) << "Failed to get system path content directory";
    return "";
  }

  std::string relative_path(content_path.data());

  relative_path.push_back(kSbFileSepChar);
  relative_path.append(path);

  return relative_path;
}

// TODO(b/36360851): add unit tests when we have a preferred way to mock
// Starboard APIs.
std::string GetCACertificatesPath(const std::string& content_subdir) {
  if (content_subdir.empty()) {
    SB_LOG(ERROR) << "content_subdir must be set.";
    return "";
  }

  std::string ca_certificates_path =
      GetCACertificatesPathFromSubdir(content_subdir);
  if (!SbFileExists(ca_certificates_path.c_str())) {
    SB_LOG(ERROR) << "Failed to get CA certificates path";
    return "";
  }

  return ca_certificates_path;
}

std::string GetCACertificatesPath() {
  // Try the Evergreen content path first.
  std::string content_subdir = std::string("app") + kSbFileSepString +
                               "cobalt" + kSbFileSepString + "content";
  std::string ca_certificates_path =
      GetCACertificatesPathFromSubdir(content_subdir);

  // Then fall back to the regular content path if necessary.
  if (!SbFileExists(ca_certificates_path.c_str())) {
    ca_certificates_path = GetCACertificatesPathFromSubdir("");
  }

  if (!SbFileExists(ca_certificates_path.c_str())) {
    SB_LOG(ERROR) << "Failed to get CA certificates path";
    return "";
  }

  return ca_certificates_path;
}

}  // namespace common
}  // namespace starboard
