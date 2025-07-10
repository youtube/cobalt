// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COBALT_SHELL_COMMON_SHELL_CONTENT_CLIENT_H_
#define COBALT_SHELL_COMMON_SHELL_CONTENT_CLIENT_H_

#include <string>
#include <vector>

#include "cobalt/shell/common/shell_origin_trial_policy.h"
#include "content/public/common/content_client.h"

namespace content {

class ShellContentClient : public ContentClient {
 public:
  ShellContentClient();
  ~ShellContentClient() override;

  std::u16string GetLocalizedString(int message_id) override;
  base::StringPiece GetDataResource(
      int resource_id,
      ui::ResourceScaleFactor scale_factor) override;
  base::RefCountedMemory* GetDataResourceBytes(int resource_id) override;
  std::string GetDataResourceString(int resource_id) override;
  gfx::Image& GetNativeImageNamed(int resource_id) override;
  blink::OriginTrialPolicy* GetOriginTrialPolicy() override;
  void AddAdditionalSchemes(Schemes* schemes) override;

 private:
  ShellOriginTrialPolicy origin_trial_policy_;
};

}  // namespace content

#endif  // CONTENT_SHELL_COMMON_SHELL_CONTENT_CLIENT_H_
