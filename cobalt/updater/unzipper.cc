// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cobalt/updater/unzipper.h"

#include <utility>
#include "base/callback.h"
#include "base/files/file_path.h"
// #include "third_party/zlib/google/zip.h"

namespace cobalt {
namespace updater {

namespace {

class UnzipperImpl : public update_client::Unzipper {
 public:
  UnzipperImpl() = default;

  void Unzip(const base::FilePath& zip_path, const base::FilePath& output_path,
             UnzipCompleteCallback callback) override {
    // TODO: zip::Unzip() is not ported to third_party/zlib yet.
    // std::move(callback).Run(zip::Unzip(zip_path, output_path));
    SB_NOTREACHED();
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
