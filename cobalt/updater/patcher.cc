// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cobalt/updater/patcher.h"

#include <utility>
#include "base/callback.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "components/courgette/courgette.h"
#include "components/courgette/third_party/bsdiff/bsdiff.h"

namespace cobalt {
namespace updater {

namespace {

class PatcherImpl : public update_client::Patcher {
 public:
  PatcherImpl() = default;

  void PatchBsdiff(const base::FilePath& input_path,
                   const base::FilePath& patch_path,
                   const base::FilePath& output_path,
                   PatchCompleteCallback callback) const override {
    // TODO: Patching delta update with bsdiff is not supported yet.
    std::move(callback).Run(-1);
    return;
  }

  void PatchCourgette(const base::FilePath& input_path,
                      const base::FilePath& patch_path,
                      const base::FilePath& output_path,
                      PatchCompleteCallback callback) const override {
    // TODO: Patching delta update with courgette is not supported yet.
    std::move(callback).Run(-1);
    return;
  }

 protected:
  ~PatcherImpl() override = default;
};

}  // namespace

PatcherFactory::PatcherFactory() = default;

scoped_refptr<update_client::Patcher> PatcherFactory::Create() const {
  return base::MakeRefCounted<PatcherImpl>();
}

PatcherFactory::~PatcherFactory() = default;

}  // namespace updater
}  // namespace cobalt
