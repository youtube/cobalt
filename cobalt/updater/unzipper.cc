// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cobalt/updater/unzipper.h"

#include <utility>
#include "base/callback.h"
#include "base/files/file_path.h"
#include "starboard/time.h"
#include "third_party/zlib/google/zip.h"

namespace cobalt {
namespace updater {

namespace {

class UnzipperImpl : public update_client::Unzipper {
 public:
  UnzipperImpl() = default;

  void Unzip(const base::FilePath& zip_path, const base::FilePath& output_path,
             UnzipCompleteCallback callback) override {
    SbTimeMonotonic time_before_unzip = SbTimeGetMonotonicNow();
    std::move(callback).Run(zip::Unzip(zip_path, output_path));
    SbTimeMonotonic time_unzip_took =
        SbTimeGetMonotonicNow() - time_before_unzip;
    SB_LOG(INFO) << "Unzip file path = " << zip_path;
    SB_LOG(INFO) << "output_path = " << output_path;
    SB_LOG(INFO) << "Unzip took " << time_unzip_took / kSbTimeMillisecond
                 << " milliseconds.";
  }
};

}  // namespace

UnzipperFactory::UnzipperFactory() = default;

std::unique_ptr<update_client::Unzipper> UnzipperFactory::Create() const {
  return std::make_unique<UnzipperImpl>();
}

UnzipperFactory::~UnzipperFactory() = default;

}  // namespace updater
}  // namespace cobalt
