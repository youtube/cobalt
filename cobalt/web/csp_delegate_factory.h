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

#ifndef COBALT_WEB_CSP_DELEGATE_FACTORY_H_
#define COBALT_WEB_CSP_DELEGATE_FACTORY_H_

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/functional/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/memory/singleton.h"
#include "cobalt/csp/content_security_policy.h"
#include "cobalt/network_bridge/net_poster.h"
#include "cobalt/web/csp_delegate.h"
#include "cobalt/web/csp_delegate_type.h"
#include "url/gurl.h"

namespace cobalt {

namespace browser {
class BrowserModule;
class DebugConsole;
class SplashScreen;
}  // namespace browser

namespace web {

class CspDelegate;
class CspViolationReporter;

class CspDelegateFactory {
 public:
  static CspDelegateFactory* GetInstance();
  static std::unique_ptr<CspDelegate> Create(
      WindowOrWorkerGlobalScope* global, const CspDelegate::Options& options,
      const base::Closure& policy_changed_callback = base::Closure()) {
    return GetInstance()->CreateDelegate(global, options,
                                         policy_changed_callback);
  }


  typedef CspDelegate* (*CspDelegateCreator)(
      std::unique_ptr<CspViolationReporter> violation_reporter, const GURL& url,
      csp::CSPHeaderPolicy csp_header_policy,
      const base::Closure& policy_changed_callback, int insecure_allowed_token);

#if !defined(COBALT_FORCE_CSP)
  // Allow tests to have the factory create a different delegate type.
  void OverrideCreator(CspEnforcementType type, CspDelegateCreator creator);
#endif
 protected:
  std::unique_ptr<CspDelegate> CreateDelegate(
      WindowOrWorkerGlobalScope* global, const CspDelegate::Options& options,
      const base::Closure& policy_changed_callback);

 private:
#if !defined(COBALT_FORCE_CSP)
  // By default it's not possible to create an insecure CspDelegate. To allow
  // this, get a token using |GetInsecureAllowedToken| and pass into
  // CspDelegateFactory::Create().
  static int GetInsecureAllowedToken();

  // Only these classes may create insecure Csp delegates. Be careful
  // before adding new ones.
  friend class browser::DebugConsole;
  FRIEND_TEST(CspDelegateFactoryTest, InsecureAllowed);

  // Permit setting --csp_mode=disable on the command line.
  friend class browser::BrowserModule;
#endif

  CspDelegateFactory();
  static bool SetInsecureAllowed(bool allowed);

  CspDelegateCreator method_[kCspEnforcementCount];

  friend struct base::DefaultSingletonTraits<CspDelegateFactory>;
  DISALLOW_COPY_AND_ASSIGN(CspDelegateFactory);
};

}  // namespace web
}  // namespace cobalt

#endif  // COBALT_WEB_CSP_DELEGATE_FACTORY_H_
