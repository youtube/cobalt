/*
 * Copyright 2022 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef FCP_CLIENT_CACHE_FILE_BACKED_RESOURCE_CACHE_H_
#define FCP_CLIENT_CACHE_FILE_BACKED_RESOURCE_CACHE_H_

#include <memory>

#include "absl/status/statusor.h"
#include "absl/time/clock_interface.h"
#include "fcp/client/cache/resource_cache.h"
#include "fcp/client/log_manager.h"

namespace fcp::client::cache {

class FileBackedResourceCache : public ResourceCache {
 public:
  static inline absl::StatusOr<std::unique_ptr<FileBackedResourceCache>> Create(
      absl::string_view base_dir,
      absl::string_view cache_dir,
      LogManager* log_manager,
      absl::Clock* clock,
      int64_t max_cache_size_bytes,
      bool sanitize_client_cache_id = false) {
    return absl::UnimplementedError("Not implemented in Chromium");
  }

  inline absl::Status Put(absl::string_view cache_id,
                   const absl::Cord& resource,
                   const google::protobuf::Any& metadata,
                   absl::Duration max_age) override {
    return absl::UnimplementedError("Not implemented in Chromium");
  }

  inline absl::StatusOr<ResourceAndMetadata> Get(
      absl::string_view cache_id,
      std::optional<absl::Duration> max_age) override {
    return absl::UnimplementedError("Not implemented in Chromium");
  }
};

}  // namespace fcp::client::cache

#endif  // FCP_CLIENT_CACHE_FILE_BACKED_RESOURCE_CACHE_H_
