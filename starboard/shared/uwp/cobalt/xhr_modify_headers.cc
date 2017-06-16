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

#include "cobalt/xhr/xhr_modify_headers.h"

#include <base/logging.h>
#include "starboard/shared/uwp/async_utils.h"
#include "starboard/shared/uwp/winrt_workaround.h"
#include "starboard/shared/win32/wchar_utils.h"

using Windows::Security::Authentication::Web::Core::
    WebAuthenticationCoreManager;
using Windows::Security::Authentication::Web::Core::WebTokenRequest;
using Windows::Security::Authentication::Web::Core::WebTokenResponse;
using Windows::Security::Authentication::Web::Core::WebTokenRequestResult;
using Windows::Security::Authentication::Web::Core::WebTokenRequestStatus;
using Windows::Security::Credentials::WebAccountProvider;
using Windows::System::UserAuthenticationStatus;

namespace sbwin32 = starboard::shared::win32;

namespace {
// The name of the header to send the STS token out on.
const char kXauthHeaderName[] = "Authorization";

// The prefix for the value of the Authorization header when there is an XAuth
// token to attach to the request. The token itself should directly follow this
// prefix.
const char kXauthHeaderPrefix[] = "XBL3.0 x=";

// The name of the header to detect on requests as a trigger to add an STS token
// for the given RelyingPartyId (the value of the header). The header itself
// will be removed from the outgoing request, and replaced with an Authorization
// header with a valid STS token for the current primary user.
const base::StringPiece kXauthTriggerHeaderName = "X-STS-RelyingPartyId";

inline std::ostream& operator<<(std::ostream& os,
                                const UserAuthenticationStatus& state) {
  switch (state) {
    case UserAuthenticationStatus::Unauthenticated:
      os << "Unauthenticated";
      break;
    case UserAuthenticationStatus::LocallyAuthenticated:
      os << "LocallyAuthenticated";
      break;
    case UserAuthenticationStatus::RemotelyAuthenticated:
      os << "RemotelyAuthenticated";
      break;
    default:
      os << "Unknown";
  }
  return os;
}

inline std::ostream& operator<<(std::ostream& os,
                                const WebTokenRequestStatus& status) {
  switch (status) {
    case WebTokenRequestStatus::Success:
      os << "Success";
      break;
    case WebTokenRequestStatus::AccountProviderNotAvailable:
      os << "Account provider is not available.";
      break;
    case WebTokenRequestStatus::AccountSwitch:
      os << "Account associated with the request was switched.";
      break;
    case WebTokenRequestStatus::ProviderError:
      os << "Provider Error.  See Provider's documentation.";
      break;
    case WebTokenRequestStatus::UserCancel:
      os << "User Cancel";
      break;
    case WebTokenRequestStatus::UserInteractionRequired:
      os << "User interaction is required.  Try the request with "
            "RequestTokenAsync";
      break;
    default:
      os << "Unknown case";
  }
  return os;
}

bool PopulateToken(const std::string& relying_party, std::string* out) {
  using starboard::shared::uwp::WaitForResult;
  DCHECK(out);
  WebAccountProvider^ xbox_provider =
      WaitForResult(WebAuthenticationCoreManager::FindAccountProviderAsync(
          "https://xsts.auth.xboxlive.com"));
  WebTokenRequest^ request = ref new WebTokenRequest(xbox_provider);
  Platform::String^ relying_party_cx =
    sbwin32::stringToPlatformString(relying_party);
  request->Properties->Insert("Url", relying_party_cx);
  request->Properties->Insert("Target", "xboxlive.signin");
  request->Properties->Insert("Policy", "DELEGATION");

  WebTokenRequestResult^ token_result = WaitForResult(
      WebAuthenticationCoreManager::GetTokenSilentlyAsync(request));
  if (token_result->ResponseStatus ==
      WebTokenRequestStatus::UserInteractionRequired) {
    token_result =
        WaitForResult(WebAuthenticationCoreManager::RequestTokenAsync(request));
  }

  if (token_result->ResponseStatus == WebTokenRequestStatus::Success) {
    SB_DCHECK(token_result->ResponseData->Size == 1);
    if (token_result->ResponseData->Size != 1) {
      *out = "";
      return false;
    }
    WebTokenResponse^ token_response = token_result->ResponseData->GetAt(0);
    *out = sbwin32::platformStringToString(token_response->Token);
    return true;
  } else {
    SB_DLOG(INFO) << "Response Status " << token_result->ResponseStatus;
    if (token_result->ResponseError) {
      unsigned int error_code = token_result->ResponseError->ErrorCode;
      Platform::String^ message = token_result->ResponseError->ErrorMessage;
      SB_DLOG(INFO) << "Error code: " << error_code;
      SB_DLOG(INFO) << "Error message: "
        << sbwin32::platformStringToString(message);
    }
    *out = "";
  }

  return false;
}
}  // namespace

namespace cobalt {
namespace xhr {

void CobaltXhrModifyHeader(net::HttpRequestHeaders* headers) {
  DCHECK(headers);

  std::string relying_party;
  bool trigger_header_found = headers->GetHeader(kXauthTriggerHeaderName,
    &relying_party);

  if (!trigger_header_found) {
    return;
  }
  std::string out_string;
  if (!PopulateToken(relying_party, &out_string)) {
    return;
  }
  headers->RemoveHeader(kXauthTriggerHeaderName);
  headers->SetHeader(kXauthHeaderName, out_string);
}

}  // namespace xhr
}  // namespace cobalt
