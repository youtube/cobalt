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

#ifndef COBALT_SHELL_BROWSER_SHELL_CONTENT_INDEX_PROVIDER_H_
#define COBALT_SHELL_BROWSER_SHELL_CONTENT_INDEX_PROVIDER_H_

#include <map>
#include <string>
#include <vector>

#include "content/public/browser/content_index_provider.h"
#include "ui/gfx/geometry/size.h"
#include "url/origin.h"

namespace content {

// When using ShellContentIndexProvider, IDs need to be globally unique,
// instead of per Service Worker.
class ShellContentIndexProvider : public ContentIndexProvider {
 public:
  ShellContentIndexProvider();

  ShellContentIndexProvider(const ShellContentIndexProvider&) = delete;
  ShellContentIndexProvider& operator=(const ShellContentIndexProvider&) =
      delete;

  ~ShellContentIndexProvider() override;

  // ContentIndexProvider implementation.
  std::vector<gfx::Size> GetIconSizes(
      blink::mojom::ContentCategory category) override;
  void OnContentAdded(ContentIndexEntry entry) override;
  void OnContentDeleted(int64_t service_worker_registration_id,
                        const url::Origin& origin,
                        const std::string& description_id) override;

  // Returns the Service Worker Registration ID and the origin of the Content
  // Index entry registered with |id|. If |id| does not exist, invalid values
  // are returned, which would cause DB tasks to fail.
  std::pair<int64_t, url::Origin> GetRegistrationDataFromId(
      const std::string& id);

  void set_icon_sizes(std::vector<gfx::Size> icon_sizes) {
    icon_sizes_ = std::move(icon_sizes);
  }

 private:
  // Map from |description_id| to <|service_worker_registration_id|, |origin|>.
  std::map<std::string, std::pair<int64_t, url::Origin>> entries_;

  std::vector<gfx::Size> icon_sizes_;
};

}  // namespace content

#endif  // COBALT_SHELL_BROWSER_SHELL_CONTENT_INDEX_PROVIDER_H_
