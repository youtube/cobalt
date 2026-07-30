// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cobalt/updater/patcher.h"

#include <utility>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "components/zucchini/zucchini.h"
#include "components/zucchini/zucchini_integration.h"
#include "third_party/puffin/src/include/puffin/puffpatch.h"

// TODO(b/174699165): Investigate differential updates

namespace cobalt {
namespace updater {

namespace {

class PatcherImpl : public update_client::Patcher {
 public:
  PatcherImpl() = default;

  void PatchPuffPatch(base::File input_file,
                      base::File patch_file,
                      base::File output_file,
                      PatchCompleteCallback callback) const override {
    std::move(callback).Run(puffin::ApplyPuffPatch(
        std::move(input_file), std::move(patch_file), std::move(output_file)));
  }

  void PatchZucchini(base::File old_file,
                     base::File patch_file,
                     base::File destination_file,
                     PatchCompleteCallback callback) const override {
    std::move(callback).Run(static_cast<int>(
        zucchini::Apply(std::move(old_file), std::move(patch_file),
                        std::move(destination_file))));
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
