// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/shell/browser/shell_content_index_provider.h"

namespace content {

ShellContentIndexProvider::ShellContentIndexProvider()
    : icon_sizes_({{96, 96}}) {}

ShellContentIndexProvider::~ShellContentIndexProvider() = default;

std::vector<gfx::Size> ShellContentIndexProvider::GetIconSizes(
    blink::mojom::ContentCategory category) {
  return icon_sizes_;
}

void ShellContentIndexProvider::OnContentAdded(ContentIndexEntry entry) {
  entries_[entry.description->id] = {
      entry.service_worker_registration_id,
      url::Origin::Create(entry.launch_url.DeprecatedGetOriginAsURL())};
}

void ShellContentIndexProvider::OnContentDeleted(
    int64_t service_worker_registration_id,
    const url::Origin& origin,
    const std::string& description_id) {
  entries_.erase(description_id);
}

std::pair<int64_t, url::Origin>
ShellContentIndexProvider::GetRegistrationDataFromId(const std::string& id) {
  if (!entries_.count(id)) {
    return {-1, url::Origin()};
  }
  return entries_[id];
}

}  // namespace content
