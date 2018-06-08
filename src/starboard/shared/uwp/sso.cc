// Copyright 2017 Google Inc. All Rights Reserved.
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

#include "starboard/shared/uwp/app_accessors.h"

#include "starboard/file.h"
#include "starboard/log.h"
#include "starboard/shared/win32/wchar_utils.h"

namespace sbuwp = starboard::shared::uwp;
namespace sbwin32 = starboard::shared::win32;

using Windows::Security::Authentication::Web::Core::
    WebAuthenticationCoreManager;
using Windows::Security::Authentication::Web::Core::WebTokenRequest;
using Windows::Security::Authentication::Web::Core::WebTokenResponse;
using Windows::Security::Authentication::Web::Core::WebTokenRequestResult;
using Windows::Security::Authentication::Web::Core::WebTokenRequestStatus;
using Windows::Security::Credentials::WebAccountProvider;

namespace {

const char kXboxLiveAccountProviderId[] = "https://xsts.auth.xboxlive.com";

// Filename, placed in the cache dir, whose presence indicates
// that the user has rejected the SSO approval dialog
const char kSsoRejectionFilename[] = "sso_rejection";

std::string GetSsoRejectionFilePath() {
  char path[SB_FILE_MAX_PATH];
  bool success = SbSystemGetPath(kSbSystemPathCacheDirectory,
                                 path, SB_FILE_MAX_PATH);
  SB_DCHECK(success);
  std::string file_path(path);
  file_path.append(1, '/');
  file_path.append(kSsoRejectionFilename);

  return file_path;
}

}  // namespace

namespace starboard {
namespace shared {
namespace uwp {

concurrency::task<WebTokenRequestResult^> TryToFetchSsoToken(
    const std::string& url, SsoPromptPolicy prompt_policy,
    bool dialog_required);

concurrency::task<WebTokenRequestResult^> TryToFetchSsoToken(
    const std::string& url) {
  return TryToFetchSsoToken(url, kPromptIfNeverDeclined, false);
}

concurrency::task<WebTokenRequestResult^> TryToFetchSsoToken(
    const std::string& url,
    SsoPromptPolicy prompt_policy) {
  return TryToFetchSsoToken(url, prompt_policy, false);
}

concurrency::task<WebTokenRequestResult^> TryToFetchSsoTokenDialogRequired(
    const std::string& url, SsoPromptPolicy prompt_policy) {
  return TryToFetchSsoToken(url, prompt_policy, true);
}

// When |dialog_required| is true, the codepath that allows
// a dialog to appear (and causes a visible flicker if the dialog is not
// required) will be called. That is, RequestTokenAsync will be used instead
// of GetTokenSilentlyAsync
//
// |dialog_required| is only set to true in the case where a previous
// attempt to fetch the token returned UserInteractionRequired.
//
concurrency::task<WebTokenRequestResult^> TryToFetchSsoToken(
    const std::string& url, SsoPromptPolicy prompt_policy,
    bool dialog_required) {
  if (dialog_required) {
    SB_DCHECK(sbuwp::GetDispatcher()->HasThreadAccess)
        << "When dialog_required is true, must be called on main UWP thread";
  }
  if (dialog_required && prompt_policy == kPromptIfNeverDeclined &&
      SbFileExists(GetSsoRejectionFilePath().c_str())) {
    concurrency::task_completion_event<WebTokenRequestResult^> result;
    result.set(nullptr);
    return concurrency::task<WebTokenRequestResult^>(result);
  }
  return concurrency::create_task(
      WebAuthenticationCoreManager::FindAccountProviderAsync(
          sbwin32::stringToPlatformString(kXboxLiveAccountProviderId)))
  .then([url, dialog_required](
      concurrency::task<WebAccountProvider^> previous_task) {
    WebAccountProvider^ xbox_provider = nullptr;
    try {
      xbox_provider = previous_task.get();
    } catch (Platform::Exception^) {
      SB_LOG(INFO) << "Exception with FindAccountProviderAsync";
      concurrency::task_completion_event<WebTokenRequestResult^> result;
      result.set(nullptr);
      return concurrency::task<WebTokenRequestResult^>(result);
    }
    WebTokenRequest^ request = ref new WebTokenRequest(xbox_provider);
    request->Properties->Insert("Url",
        sbwin32::stringToPlatformString(url));
    request->Properties->Insert("Target", "xboxlive.signin");
    request->Properties->Insert("Policy", "DELEGATION");

    if (dialog_required) {
      return concurrency::create_task(
          WebAuthenticationCoreManager::RequestTokenAsync(request));
    } else {
      return concurrency::create_task(
          WebAuthenticationCoreManager::GetTokenSilentlyAsync(request));
    }
  })
  .then([url, prompt_policy](
      concurrency::task<WebTokenRequestResult^> previous_task) {
    WebTokenRequestResult^ token_result = previous_task.get();
    try {
      token_result = previous_task.get();
    } catch(Platform::Exception^ ex) {
      SB_LOG(INFO) << "Exception during RequestTokenAsync";
      concurrency::task_completion_event<WebTokenRequestResult^> result;
      result.set_exception(ex);
      return concurrency::task<WebTokenRequestResult^>(result);
    }

    if (!token_result) {
      return previous_task;
    }

    switch (token_result->ResponseStatus) {
      case WebTokenRequestStatus::UserCancel: {
        bool created;
        SbFile file = SbFileOpen(GetSsoRejectionFilePath().c_str(),
            kSbFileRead | kSbFileOpenAlways, &created, nullptr);
        SbFileClose(file);
        return previous_task;
      }

      case WebTokenRequestStatus::UserInteractionRequired: {
        if (prompt_policy == kNeverPrompt) {
          return previous_task;
        }
        concurrency::task_completion_event<WebTokenRequestResult^> completion;
        RunInMainThreadAsync([url, prompt_policy, completion]() {
          // When we run TryToFetchSsoTokenAndPrompt in the main thread,
          // we'll always ask for user input via RequestTokenAsync, which
          // never returns this case.
          TryToFetchSsoTokenDialogRequired(url, prompt_policy)
          .then([completion](concurrency::task<WebTokenRequestResult^> result) {
            try {
              completion.set(result.get());
            } catch (Platform::Exception^ ex) {
              completion.set_exception(ex);
            }
          });
        });

        return concurrency::task<WebTokenRequestResult^>(completion);
      }

      case WebTokenRequestStatus::Success:
        SbFileDelete(GetSsoRejectionFilePath().c_str());
        return previous_task;

      default:
        return previous_task;
    }
  });
}

}  // namespace uwp
}  // namespace shared
}  // namespace starboard
