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
#include "cobalt/web/environment_settings.h"
#include "cobalt/web/message_port.h"
#include "cobalt/worker/client_type.h"
#include "cobalt/worker/frame_type.h"

namespace cobalt {
namespace worker {

class Client : public web::MessagePort {
 public:
  // https://www.w3.org/TR/2022/CRD-service-workers-20220712/#create-client-algorithm
  static scoped_refptr<Client> Create(web::EnvironmentSettings* client) {
    return new Client(client);
  }
  ~Client() { service_worker_client_ = nullptr; }

  std::string url() {
    // https://www.w3.org/TR/2022/CRD-service-workers-20220712/#client-url
    return service_worker_client_->creation_url().spec();
  }

  // https://www.w3.org/TR/2022/CRD-service-workers-20220712/#client-frametype
  void set_frame_type(FrameType frame_type) { frame_type_ = frame_type; }
  FrameType frame_type() { return frame_type_; }

  // https://www.w3.org/TR/2022/CRD-service-workers-20220712/#dom-client-id
  std::string id() { return service_worker_client_->id(); }
  ClientType type();

  DEFINE_WRAPPABLE_TYPE(Client);

 protected:
  explicit Client(web::EnvironmentSettings* client);

 private:
  FrameType frame_type_ = kFrameTypeNone;
  web::EnvironmentSettings* service_worker_client_ = nullptr;
};


}  // namespace worker
}  // namespace cobalt

#endif  // COBALT_WORKER_CLIENT_H_
