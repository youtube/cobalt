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

#include "cobalt/shell/common/shell_content_client.h"

#include "base/command_line.h"
#include "base/strings/string_piece.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "cobalt/shell/common/shell_switches.h"
#include "cobalt/shell/grit/shell_resources.h"
#include "content/public/common/content_switches.h"
#include "third_party/blink/public/strings/grit/blink_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

namespace content {

ShellContentClient::ShellContentClient() {}

ShellContentClient::~ShellContentClient() {}

std::u16string ShellContentClient::GetLocalizedString(int message_id) {
  if (switches::IsRunWebTestsSwitchPresent()) {
    switch (message_id) {
      case IDS_FORM_OTHER_DATE_LABEL:
        return u"<<OtherDate>>";
      case IDS_FORM_OTHER_MONTH_LABEL:
        return u"<<OtherMonth>>";
      case IDS_FORM_OTHER_WEEK_LABEL:
        return u"<<OtherWeek>>";
      case IDS_FORM_CALENDAR_CLEAR:
        return u"<<Clear>>";
      case IDS_FORM_CALENDAR_TODAY:
        return u"<<Today>>";
      case IDS_FORM_THIS_MONTH_LABEL:
        return u"<<ThisMonth>>";
      case IDS_FORM_THIS_WEEK_LABEL:
        return u"<<ThisWeek>>";
    }
  }
  return l10n_util::GetStringUTF16(message_id);
}

base::StringPiece ShellContentClient::GetDataResource(
    int resource_id,
    ui::ResourceScaleFactor scale_factor) {
  return ui::ResourceBundle::GetSharedInstance().GetRawDataResourceForScale(
      resource_id, scale_factor);
}

base::RefCountedMemory* ShellContentClient::GetDataResourceBytes(
    int resource_id) {
  return ui::ResourceBundle::GetSharedInstance().LoadDataResourceBytes(
      resource_id);
}

std::string ShellContentClient::GetDataResourceString(int resource_id) {
  return ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
      resource_id);
}

gfx::Image& ShellContentClient::GetNativeImageNamed(int resource_id) {
  return ui::ResourceBundle::GetSharedInstance().GetNativeImageNamed(
      resource_id);
}

blink::OriginTrialPolicy* ShellContentClient::GetOriginTrialPolicy() {
  return nullptr;
}

void ShellContentClient::AddAdditionalSchemes(Schemes* schemes) {
#if BUILDFLAG(IS_ANDROID)
  schemes->local_schemes.push_back(url::kContentScheme);
#endif
}

}  // namespace content
