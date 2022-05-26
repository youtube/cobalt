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

#ifndef COBALT_WORKER_WORKER_LOCATION_H_
#define COBALT_WORKER_WORKER_LOCATION_H_

#include <string>

#include "cobalt/script/wrappable.h"
#include "cobalt/web/location_base.h"
#include "url/gurl.h"

namespace cobalt {
namespace worker {

// WorkerLocation objects provide a representation of the address of the active
// script of the web context.
//   https://www.w3.org/TR/html50/workers.html#worker-locations
class WorkerLocation : public web::LocationBase {
 public:
  // If any navigation is triggered, all these callbacks should be provided,
  // otherwise they can be empty.
  explicit WorkerLocation(const GURL& url) : web::LocationBase(url) {}
  WorkerLocation(const WorkerLocation&) = delete;
  WorkerLocation& operator=(const WorkerLocation&) = delete;

  DEFINE_WRAPPABLE_TYPE(WorkerLocation);

 private:
  ~WorkerLocation() override {}
};

}  // namespace worker
}  // namespace cobalt

#endif  // COBALT_WORKER_WORKER_LOCATION_H_
