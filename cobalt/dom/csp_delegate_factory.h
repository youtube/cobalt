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

#ifndef COBALT_DOM_CSP_DELEGATE_FACTORY_H_
#define COBALT_DOM_CSP_DELEGATE_FACTORY_H_

#include <string>

#include "base/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/singleton.h"
#include "cobalt/dom/csp_delegate_type.h"
#include "googleurl/src/gurl.h"

namespace cobalt {

namespace browser {
class BrowserModule;
class DebugConsole;
class SplashScreen;
}  // namespace browser

namespace dom {

class CspDelegate;
class CspViolationReporter;

class CspDelegateFactory {
 public:
  static CspDelegateFactory* GetInstance();
  scoped_ptr<CspDelegate> Create(
      CspEnforcementType type,
      scoped_ptr<CspViolationReporter> violation_reporter, const GURL& url,
      const std::string& location_policy,
      const base::Closure& policy_changed_callback);

  typedef CspDelegate* (*CspDelegateCreator)(
      scoped_ptr<CspViolationReporter> violation_reporter, const GURL&,
      const std::string&, const base::Closure&);

#if !defined(COBALT_FORCE_CSP)
  // Allow tests to have the factory create a different delegate type.
  void OverrideCreator(CspEnforcementType type, CspDelegateCreator creator);
#endif

 private:
#if !defined(COBALT_FORCE_CSP)
  // By default it's not possible to create an insecure CspDelegate. To allow
  // this, instantiate a ScopedAllowInsecure before calling
  // CspDelegateFactory::Create().
  class ScopedAllowInsecure {
   public:
    ScopedAllowInsecure() { previous_value_ = SetInsecureAllowed(true); }
    ~ScopedAllowInsecure() { SetInsecureAllowed(previous_value_); }

   private:
    // Whether Insecure CSP delegates were allowed, when the
    // ScopedAllowInsecure was constructed.
    bool previous_value_;

    DISALLOW_COPY_AND_ASSIGN(ScopedAllowInsecure);
  };

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

  friend struct DefaultSingletonTraits<CspDelegateFactory>;
  DISALLOW_COPY_AND_ASSIGN(CspDelegateFactory);
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_CSP_DELEGATE_FACTORY_H_
