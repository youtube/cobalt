// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/updater/unzipper.h"

#include <string>
#include <utility>
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/time/time.h"
#include "third_party/zlib/google/zip.h"

namespace cobalt {
namespace updater {

namespace {

class UnzipperImpl : public update_client::Unzipper {
 public:
  UnzipperImpl() = default;

  void Unzip(const base::FilePath& zip_path,
             const base::FilePath& output_path,
             UnzipCompleteCallback callback) override {
    auto time_before_unzip = base::TimeTicks::Now();
    std::move(callback).Run(zip::Unzip(zip_path, output_path));
    auto time_unzip_took_usec =
        (base::TimeTicks::Now() - time_before_unzip).InMilliseconds();
    LOG(INFO) << "Unzip file path = " << zip_path;
    LOG(INFO) << "output_path = " << output_path;
    LOG(INFO) << "Unzip took " << time_unzip_took_usec << " milliseconds.";
  }

#if defined(IN_MEMORY_UPDATES)
  void Unzip(const std::string& zip_str,
             const base::FilePath& output_path,
             UnzipCompleteCallback callback) override {
    auto time_before_unzip = base::TimeTicks::Now();
    std::move(callback).Run(zip::Unzip(zip_str, output_path));
    auto time_unzip_took_usec =
        (base::TimeTicks::Now() - time_before_unzip).InMilliseconds();
    LOG(INFO) << "Unzip from string";
    LOG(INFO) << "output_path = " << output_path;
    LOG(INFO) << "Unzip took " << time_unzip_took_usec << " milliseconds.";
  }
#endif
};

}  // namespace

UnzipperFactory::UnzipperFactory() = default;

std::unique_ptr<update_client::Unzipper> UnzipperFactory::Create() const {
  return std::make_unique<UnzipperImpl>();
}

UnzipperFactory::~UnzipperFactory() = default;

}  // namespace updater
}  // namespace cobalt
