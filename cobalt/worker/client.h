// Copyright 2022 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_WORKER_CLIENT_H_
#define COBALT_WORKER_CLIENT_H_

#include <string>

#include "cobalt/script/wrappable.h"
#include "cobalt/worker/client_type.h"
#include "cobalt/worker/frame_type.h"

namespace cobalt {
namespace worker {

class Client : public script::Wrappable {
 public:
  std::string url() { return std::string("foo"); }
  FrameType frame_type() { return kFrameTypeNone; }

  std::string id() { return std::string("foo"); }
  ClientType type() { return kClientTypeAll; }

  void PostMessage(const std::string& message);


  DEFINE_WRAPPABLE_TYPE(Client);
};

}  // namespace worker
}  // namespace cobalt

#endif  // COBALT_WORKER_CLIENT_H_
