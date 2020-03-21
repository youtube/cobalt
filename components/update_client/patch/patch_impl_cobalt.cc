// Copyright 2020 The Cobalt Authors. All Rights Reserved.
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

#include "components/update_client/patch/patch_impl_cobalt.h"

#include "components/update_client/component_patcher_operation.h"

namespace update_client {

namespace {

class PatcherImplCobalt : public Patcher {
 public:
  PatcherImplCobalt() {}

  void PatchBsdiff(const base::FilePath& old_file,
                   const base::FilePath& patch_file,
                   const base::FilePath& destination,
                   PatchCompleteCallback callback) const override {
    // TODO: implement when Evergreen supports patcher.
  }

  void PatchCourgette(const base::FilePath& old_file,
                      const base::FilePath& patch_file,
                      const base::FilePath& destination,
                      PatchCompleteCallback callback) const override {
    // TODO: implement when Evergreen supports patcher.
  }

 protected:
  ~PatcherImplCobalt() override = default;
};

}  // namespace

PatchCobaltFactory::PatchCobaltFactory() {}

scoped_refptr<Patcher> PatchCobaltFactory::Create() const {
  return base::MakeRefCounted<PatcherImplCobalt>();
}

PatchCobaltFactory::~PatchCobaltFactory() = default;

}  // namespace update_client
