/*
 * Copyright 2016 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "cobalt/dom/csp_delegate_factory.h"

#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/threading/thread_local.h"

#include "cobalt/dom/csp_delegate.h"

namespace cobalt {
namespace dom {

namespace {
#if !defined(COBALT_FORCE_CSP)
// ThreadLocalBoolean defaults to |false|
base::LazyInstance<base::ThreadLocalBoolean>::Leaky s_insecure_allowed =
    LAZY_INSTANCE_INITIALIZER;

bool InsecureAllowed() {
  if (s_insecure_allowed.Get().Get()) {
    return true;
  } else {
    LOG(FATAL) << "This thread is not permitted to create insecure CSP "
                  "delegates. To allow this, create a "
                  "CspDelegateFactory::ScopedAllowInsecure on the stack.";
    return false;
  }
}

CspDelegate* CreateInsecureDelegate(
    scoped_ptr<CspViolationReporter> /*violation_reporter*/,
    const GURL& /*url*/, const std::string& /*location_policy*/,
    const base::Closure& /*policy_changed_callback*/) {
  if (InsecureAllowed()) {
    return new CspDelegateInsecure();
  } else {
    return NULL;
  }
}
#endif

CspDelegate* CreateSecureDelegate(
    scoped_ptr<CspViolationReporter> violation_reporter, const GURL& url,
    const std::string& location_policy,
    const base::Closure& policy_changed_callback) {
  return new CspDelegateSecure(violation_reporter.Pass(), url, location_policy,
                               policy_changed_callback);
}
}  // namespace

CspDelegateFactory* CspDelegateFactory::GetInstance() {
  return Singleton<CspDelegateFactory>::get();
}

CspDelegateFactory::CspDelegateFactory() {
  method_[kCspEnforcementEnable] = CreateSecureDelegate;
#if !defined(COBALT_FORCE_CSP)
  method_[kCspEnforcementDisable] = CreateInsecureDelegate;
#endif
}

scoped_ptr<CspDelegate> CspDelegateFactory::Create(
    CspEnforcementType type,
    scoped_ptr<CspViolationReporter> violation_reporter, const GURL& url,
    const std::string& location_policy,
    const base::Closure& policy_changed_callback) {
  scoped_ptr<CspDelegate> delegate(method_[type](violation_reporter.Pass(), url,
                                                 location_policy,
                                                 policy_changed_callback));
  return delegate.Pass();
}

#if !defined(COBALT_FORCE_CSP)
// static
bool CspDelegateFactory::SetInsecureAllowed(bool allowed) {
  bool previous_allowed = s_insecure_allowed.Get().Get();
  s_insecure_allowed.Get().Set(allowed);
  return previous_allowed;
}

// For use by tests.
void CspDelegateFactory::OverrideCreator(CspEnforcementType type,
                                         CspDelegateCreator creator) {
  DCHECK_LT(type, kCspEnforcementCount);
  method_[type] = creator;
}
#endif  // !defined(COBALT_FORCE_CSP)

}  // namespace dom
}  // namespace cobalt
