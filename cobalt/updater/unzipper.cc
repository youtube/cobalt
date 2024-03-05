// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cobalt/updater/unzipper.h"

#include <string>
#include <utility>
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/time/time.h"
#include "starboard/common/time.h"
#include "third_party/zlib/google/zip.h"

namespace cobalt {
namespace updater {

namespace {

class UnzipperImpl : public update_client::Unzipper {
 public:
  UnzipperImpl() = default;

  void Unzip(const base::FilePath& zip_path, const base::FilePath& output_path,
             UnzipCompleteCallback callback) override {
    int64_t time_before_unzip = starboard::CurrentMonotonicTime();
    std::move(callback).Run(zip::Unzip(zip_path, output_path));
    int64_t time_unzip_took_usec =
        starboard::CurrentMonotonicTime() - time_before_unzip;
    LOG(INFO) << "Unzip file path = " << zip_path;
    LOG(INFO) << "output_path = " << output_path;
    LOG(INFO) << "Unzip took "
              << time_unzip_took_usec / base::Time::kMicrosecondsPerMillisecond
              << " milliseconds.";
  }

#if defined(IN_MEMORY_UPDATES)
  void Unzip(const std::string& zip_str, const base::FilePath& output_path,
             UnzipCompleteCallback callback) override {
    int64_t time_before_unzip = starboard::CurrentMonotonicTime();
    std::move(callback).Run(zip::Unzip(zip_str, output_path));
    int64_t time_unzip_took_usec =
        starboard::CurrentMonotonicTime() - time_before_unzip;
    LOG(INFO) << "Unzip from string";
    LOG(INFO) << "output_path = " << output_path;
    LOG(INFO) << "Unzip took "
              << time_unzip_took_usec / base::Time::kMicrosecondsPerMillisecond
              << " milliseconds.";
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
