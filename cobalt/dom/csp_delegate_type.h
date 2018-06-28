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

#ifndef COBALT_DOM_CSP_DELEGATE_TYPE_H_
#define COBALT_DOM_CSP_DELEGATE_TYPE_H_

namespace cobalt {
namespace dom {

enum CspEnforcementType {
  // Require CSP policy to be delivered in HTTP headers, otherwise consider
  // it an error. The main web module should require CSP in production.
  kCspEnforcementEnable = 0,
#if !defined(COBALT_FORCE_CSP)
  // Do no CSP checks, regardless of policy from the server. This is
  // for testing, so CSP can be disabled on the command line.
  // Also, debug web modules may want to disable CSP.
  kCspEnforcementDisable = 1,
#endif  // defined(COBALT_FORCE_CSP)
  kCspEnforcementCount,
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_CSP_DELEGATE_TYPE_H_
