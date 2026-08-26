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

#include "components/update_client/unzip/unzip_impl_cobalt.h"

#include <memory>
#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "components/services/unzip/in_process_unzipper.h"
#include "components/services/unzip/public/cpp/unzip.h"
#include "third_party/zlib/google/zip.h"

namespace update_client {

namespace {

class UnzipperCobalt : public Unzipper {
 public:
  UnzipperCobalt() = default;

  void Unzip(const base::FilePath& zip_path,
             const base::FilePath& output_path,
             UnzipCompleteCallback callback) override {
    std::move(callback).Run(zip::Unzip(zip_path, output_path));
  }

  void Unzip(const std::string& zip_str,
             const base::FilePath& output_path,
             UnzipCompleteCallback callback) override {
    std::move(callback).Run(zip::Unzip(zip_str, output_path));
  }

  base::OnceClosure DecodeXz(const base::FilePath& xz_file,
                             const base::FilePath& destination,
                             UnzipCompleteCallback callback) override {
    return unzip::DecodeXz(unzip::LaunchInProcessUnzipper(), xz_file,
                           destination, std::move(callback));
  }
};

}  // namespace

std::unique_ptr<Unzipper> UnzipCobaltFactory::Create() const {
  return std::make_unique<UnzipperCobalt>();
}

}  // namespace update_client
