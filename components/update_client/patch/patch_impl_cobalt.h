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

#ifndef COMPONENTS_UPDATE_CLIENT_PATCH_PATCH_IMPL_COBALT_H_
#define COMPONENTS_UPDATE_CLIENT_PATCH_PATCH_IMPL_COBALT_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "components/update_client/patcher.h"

namespace update_client {

class PatchCobaltFactory : public PatcherFactory {
 public:
  PatchCobaltFactory();

  scoped_refptr<Patcher> Create() const override;

 protected:
  ~PatchCobaltFactory() override;

  DISALLOW_COPY_AND_ASSIGN(PatchCobaltFactory);
};

}  // namespace update_client

#endif  // COMPONENTS_UPDATE_CLIENT_PATCH_PATCH_IMPL_COBALT_H_
