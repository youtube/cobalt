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

#ifndef COBALT_H5VCC_H5VCC_ACCOUNT_MANAGER_H_
#define COBALT_H5VCC_H5VCC_ACCOUNT_MANAGER_H_

#include <queue>
#include <string>

#include "base/message_loop.h"
#include "base/threading/thread.h"
#include "base/threading/thread_checker.h"
#include "cobalt/script/callback_function.h"
#include "cobalt/script/script_object.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace h5vcc {

// Implementation of the H5vccAccountManager interface. Requests will be handled
// one-at-time on another thread in FIFO order. When a request is complete, the
// AccessTokenCallback will be fired on the thread that the H5vccAccountManager
// was created on.
class H5vccAccountManager : public script::Wrappable {
 public:
  typedef script::CallbackFunction<bool(const std::string&, uint64_t)>
      AccessTokenCallback;
  typedef script::ScriptObject<AccessTokenCallback> AccessTokenCallbackHolder;

  H5vccAccountManager();
  // H5vccAccountManager interface.
  void GetAuthToken(const AccessTokenCallbackHolder& callback);
  void RequestPairing(const AccessTokenCallbackHolder& callback);
  void RequestUnpairing(const AccessTokenCallbackHolder& callback);

  DEFINE_WRAPPABLE_TYPE(H5vccAccountManager);

 private:
  typedef script::ScriptObject<AccessTokenCallback>::Reference
      AccessTokenCallbackReference;
  enum OperationType {
    kPairing,
    kUnpairing,
    kGetToken,
  };

  ~H5vccAccountManager();

  void RequestOperationInternal(
      OperationType operation,
      scoped_ptr<AccessTokenCallbackReference> token_callback);
  void SendResult(scoped_ptr<AccessTokenCallbackReference> token_callback,
                        const std::string& token,
                        uint64_t expiration_in_seconds);

  // Thread checker for the thread that creates this instance.
  base::ThreadChecker thread_checker_;

  // Each incoming request will have a corresponding task posted to this
  // thread's message loop and will be handled in a FIFO manner.
  base::Thread thread_;

  // The message loop that the H5vccAccountManager was created on. The public
  // interface must be called from this message loop, and callbacks will be
  // fired on this loop as well.
  MessageLoop* owning_message_loop_;

  friend class scoped_refptr<H5vccAccountManager>;
  DISALLOW_COPY_AND_ASSIGN(H5vccAccountManager);
};

}  // namespace h5vcc
}  // namespace cobalt

#endif  // COBALT_H5VCC_H5VCC_ACCOUNT_MANAGER_H_
