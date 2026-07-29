// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#ifndef COMPONENTS_UPDATE_CLIENT_UNZIP_UNZIP_IMPL_COBALT_H_
#define COMPONENTS_UPDATE_CLIENT_UNZIP_UNZIP_IMPL_COBALT_H_

#include <memory>

#include "components/update_client/unzipper.h"

namespace update_client {

// Unzipper factory for Cobalt Evergreen builds. IN_MEMORY_UPDATES (defined by
// the Evergreen platform configuration) adds an in-memory (std::string) Unzip
// overload to the Unzipper interface that the Chromium UnzipperImpl does not
// implement, so unzip_impl cannot be built for Evergreen. The unzippers
// created here implement the full Evergreen interface by calling the zip
// library directly.
class UnzipCobaltFactory : public UnzipperFactory {
 public:
  UnzipCobaltFactory() = default;

  UnzipCobaltFactory(const UnzipCobaltFactory&) = delete;
  UnzipCobaltFactory& operator=(const UnzipCobaltFactory&) = delete;

  std::unique_ptr<Unzipper> Create() const override;

 protected:
  ~UnzipCobaltFactory() override = default;
};

}  // namespace update_client

#endif  // COMPONENTS_UPDATE_CLIENT_UNZIP_UNZIP_IMPL_COBALT_H_
