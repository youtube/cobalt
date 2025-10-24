// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cobalt/updater/patcher.h"

#include <utility>
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "components/update_client/buildflags.h"
#include "courgette/courgette.h"
#include "courgette/third_party/bsdiff/bsdiff.h"
#if BUILDFLAG(ENABLE_PUFFIN_PATCHES)
// TODO(crbug.com/1349060) once Puffin patches are fully implemented,
// we should remove this #if.
#include "third_party/puffin/src/include/puffin/puffpatch.h"
#endif

// TODO(b/174699165): Investigate differential updates

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
    base::File input_file(input_path,
                          base::File::FLAG_OPEN | base::File::FLAG_READ);
    base::File patch_file(patch_path,
                          base::File::FLAG_OPEN | base::File::FLAG_READ);
    base::File output_file(output_path,
                           base::File::FLAG_CREATE | base::File::FLAG_WRITE |
                               base::File::FLAG_WIN_EXCLUSIVE_WRITE);
    if (!input_file.IsValid() || !patch_file.IsValid() ||
        !output_file.IsValid()) {
      std::move(callback).Run(-1);
      return;
    }
    std::move(callback).Run(bsdiff::ApplyBinaryPatch(
        std::move(input_file), std::move(patch_file), std::move(output_file)));
  }

  // TODO(crbug.com/1349158): Remove this function once PatchPuffPatch is
  // implemented as this becomes obsolete.
  void PatchCourgette(const base::FilePath& input_path,
                      const base::FilePath& patch_path,
                      const base::FilePath& output_path,
                      PatchCompleteCallback callback) const override {
    base::File input_file(input_path,
                          base::File::FLAG_OPEN | base::File::FLAG_READ);
    base::File patch_file(patch_path,
                          base::File::FLAG_OPEN | base::File::FLAG_READ);
    base::File output_file(output_path,
                           base::File::FLAG_CREATE | base::File::FLAG_WRITE |
                               base::File::FLAG_WIN_EXCLUSIVE_WRITE);
    if (!input_file.IsValid() || !patch_file.IsValid() ||
        !output_file.IsValid()) {
      std::move(callback).Run(-1);
      return;
    }
    std::move(callback).Run(courgette::ApplyEnsemblePatch(
        std::move(input_file), std::move(patch_file), std::move(output_file)));
  }

  void PatchPuffPatch(base::File input_file,
                      base::File patch_file,
                      base::File output_file,
                      PatchCompleteCallback callback) const override {
#if BUILDFLAG(ENABLE_PUFFIN_PATCHES)
    // TODO(crbug.com/1349060) once Puffin patches are fully implemented,
    // we should remove this #if.
    std::move(callback).Run(puffin::ApplyPuffPatch(
        std::move(input_file), std::move(patch_file), std::move(output_file)));
#else
    NOTREACHED();
#endif
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
