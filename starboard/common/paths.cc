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

#include <sys/stat.h>

#include <string>
#include <vector>

#include "starboard/common/log.h"
#include "starboard/configuration_constants.h"
#include "starboard/system.h"

namespace starboard {

namespace {

constexpr char kSSLDirName[] = "ssl";
constexpr char kCertsDirName[] = "certs";

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
  std::string path =
      std::string(kSSLDirName) + kSbFileSepString + kCertsDirName;
  if (!content_subdir.empty()) {
    path = content_subdir + kSbFileSepString + path;
  }

  std::string ca_certificates_path = PrependContentPath(path);
  struct stat info;
  if (stat(ca_certificates_path.c_str(), &info) != 0) {
    SB_LOG(ERROR) << "Failed to get CA certificates path: "
                  << ca_certificates_path;
    return "";
  }

  return ca_certificates_path;
}

}  // namespace starboard
