// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#include <memory>

#include "cobalt/dom/csp_delegate_factory.h"

#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/threading/thread_local.h"

#include "cobalt/dom/csp_delegate.h"

namespace cobalt {
namespace dom {

namespace {
#if !defined(COBALT_FORCE_CSP)
// 32-bit random number.
// Not exactly cryptographically secure, but hopefully enough to stop us
// mistakenly allowing insecure access.
const int kInsecureAllowedToken = 0x5b38b65b;

// static
bool InsecureAllowed(int token) {
  if (token == kInsecureAllowedToken) {
    return true;
  } else {
    LOG(FATAL) << "Not permitted to create insecure CSP delegates. "
                  "To allow this, get a token from a call to "
                  "GetInsecureAllowedToken and pass as the last argument to "
                  "the CspDelegateFactory::Create function.";
    return false;
  }
}

CspDelegate* CreateInsecureDelegate(
    std::unique_ptr<CspViolationReporter> violation_reporter, const GURL& url,
    csp::CSPHeaderPolicy require_csp,
    const base::Closure& policy_changed_callback, int insecure_allowed_token) {
  if (InsecureAllowed(insecure_allowed_token)) {
    return new CspDelegateInsecure();
  } else {
    return NULL;
  }
}
#endif  // !defined(COBALT_FORCE_CSP)

CspDelegate* CreateSecureDelegate(
    std::unique_ptr<CspViolationReporter> violation_reporter, const GURL& url,
    csp::CSPHeaderPolicy require_csp,
    const base::Closure& policy_changed_callback, int insecure_allowed_token) {
  return new CspDelegateSecure(std::move(violation_reporter), url, require_csp,
                               policy_changed_callback);
}
}  // namespace

#if !defined(COBALT_FORCE_CSP)
int CspDelegateFactory::GetInsecureAllowedToken() {
  return kInsecureAllowedToken;
}
#endif  // !defined(COBALT_FORCE_CSP)

CspDelegateFactory* CspDelegateFactory::GetInstance() {
  return base::Singleton<CspDelegateFactory>::get();
}

CspDelegateFactory::CspDelegateFactory() {
  method_[kCspEnforcementEnable] = CreateSecureDelegate;
#if !defined(COBALT_FORCE_CSP)
  method_[kCspEnforcementDisable] = CreateInsecureDelegate;
#endif  // !defined(COBALT_FORCE_CSP)
}

std::unique_ptr<CspDelegate> CspDelegateFactory::Create(
    CspEnforcementType type,
    std::unique_ptr<CspViolationReporter> violation_reporter, const GURL& url,
    csp::CSPHeaderPolicy require_csp,
    const base::Closure& policy_changed_callback, int insecure_allowed_token) {
  std::unique_ptr<CspDelegate> delegate(
      method_[type](std::move(violation_reporter), url, require_csp,
                    policy_changed_callback, insecure_allowed_token));
  return delegate;
}

#if !defined(COBALT_FORCE_CSP)

// For use by tests.
void CspDelegateFactory::OverrideCreator(CspEnforcementType type,
                                         CspDelegateCreator creator) {
  DCHECK_LT(type, kCspEnforcementCount);
  method_[type] = creator;
}
#endif  // !defined(COBALT_FORCE_CSP)

}  // namespace dom
}  // namespace cobalt
