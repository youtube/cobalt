// Copyright 2019 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_UPDATER_UNZIPPER_H_
#define COBALT_UPDATER_UNZIPPER_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "components/update_client/unzipper.h"

namespace cobalt {
namespace updater {

class UnzipperFactory : public update_client::UnzipperFactory {
 public:
  UnzipperFactory();

  std::unique_ptr<update_client::Unzipper> Create() const override;

 protected:
  ~UnzipperFactory() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(UnzipperFactory);
};

}  // namespace updater
}  // namespace cobalt

#endif  // COBALT_UPDATER_UNZIPPER_H_
