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

#ifndef COBALT_SHELL_COMMON_SHELL_CONTENT_CLIENT_H_
#define COBALT_SHELL_COMMON_SHELL_CONTENT_CLIENT_H_

#include <string>
#include <string_view>
#include <vector>

#include "content/public/common/content_client.h"

namespace content {

class ShellContentClient : public ContentClient {
 public:
  ShellContentClient();
  ~ShellContentClient() override;

  std::u16string GetLocalizedString(int message_id) override;
  std::string_view GetDataResource(
      int resource_id,
      ui::ResourceScaleFactor scale_factor) override;
  base::RefCountedMemory* GetDataResourceBytes(int resource_id) override;
  std::string GetDataResourceString(int resource_id) override;
  gfx::Image& GetNativeImageNamed(int resource_id) override;
  blink::OriginTrialPolicy* GetOriginTrialPolicy() override;
  void AddAdditionalSchemes(Schemes* schemes) override;
};

}  // namespace content

#endif  // COBALT_SHELL_COMMON_SHELL_CONTENT_CLIENT_H_
