// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/unzip/unzip_impl_cobalt.h"

#include <utility>
#include "base/callback.h"
#include "base/files/file_path.h"
#include "starboard/time.h"
#include "third_party/zlib/google/zip.h"

namespace update_client {

namespace {

class UnzipperImpl : public update_client::Unzipper {
 public:
  UnzipperImpl() = default;

  void Unzip(const base::FilePath& zip_path,
             const base::FilePath& output_path,
             UnzipCompleteCallback callback) override {
    std::move(callback).Run(zip::Unzip(zip_path, output_path));
  }
};

}  // namespace

UnzipCobaltFactory::UnzipCobaltFactory() = default;

std::unique_ptr<Unzipper> UnzipCobaltFactory::Create() const {
  return std::make_unique<UnzipperImpl>();
}

UnzipCobaltFactory::~UnzipCobaltFactory() = default;

}  // namespace update_client
