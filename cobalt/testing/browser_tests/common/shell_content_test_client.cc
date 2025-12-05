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

#include "cobalt/testing/browser_tests/common/shell_content_test_client.h"

#include "cobalt/shell/common/shell_test_switches.h"
#include "third_party/blink/public/strings/grit/blink_strings.h"

namespace content {

ShellContentTestClient::ShellContentTestClient() = default;

ShellContentTestClient::~ShellContentTestClient() = default;

std::u16string ShellContentTestClient::GetLocalizedString(int message_id) {
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
  return ShellContentClient::GetLocalizedString(message_id);
}

}  // namespace content
