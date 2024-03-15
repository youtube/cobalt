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

#ifndef COBALT_H5VCC_H5VCC_ACCOUNT_MANAGER_INTERNAL_H_
#define COBALT_H5VCC_H5VCC_ACCOUNT_MANAGER_INTERNAL_H_

#include <memory>
#include <queue>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/threading/thread.h"
#include "base/threading/thread_checker.h"
#include "cobalt/account/user_authorizer.h"
#include "cobalt/script/callback_function.h"
#include "cobalt/script/environment_settings.h"
#include "cobalt/script/script_value.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace h5vcc {

// Implementation of the H5vccAccountManagerInternal interface. Requests will
// be handled one-at-time on another thread in FIFO order. When a request is
// complete, the AccessTokenCallback will be fired on the thread that the
// H5vccAccountManagerInternal was created on.
class H5vccAccountManagerInternal
    : public script::Wrappable,
      public base::SupportsWeakPtr<H5vccAccountManagerInternal> {
 public:
  typedef script::CallbackFunction<bool(const std::string&, uint64_t)>
      AccessTokenCallback;
  typedef script::ScriptValue<AccessTokenCallback> AccessTokenCallbackHolder;

  // Some platforms may enable the account manager interface at compile-time
  // but need a runtime check to determine if it should be presented to the
  // web app. If so, H5vccAccountManagerInternal will be bound to
  // "H5vccAccountManager".
  static bool IsSupported();

  explicit H5vccAccountManagerInternal(
      script::EnvironmentSettings* environment_settings);

  // H5vccAccountManagerInternal interface.
  void GetAuthToken(const AccessTokenCallbackHolder& callback);
  void RequestPairing(const AccessTokenCallbackHolder& callback);
  void RequestUnpairing(const AccessTokenCallbackHolder& callback);

  DEFINE_WRAPPABLE_TYPE(H5vccAccountManagerInternal);

 private:
  typedef script::ScriptValue<AccessTokenCallback>::Reference
      AccessTokenCallbackReference;
  enum OperationType {
    kPairing,
    kUnpairing,
    kGetToken,
  };

  ~H5vccAccountManagerInternal();

  // Posts an operation to the account manager thread.
  void PostOperation(OperationType operation_type,
                     const AccessTokenCallbackHolder& callback);

  // Processes an operation on the account manager thread. Static because
  // H5vccAccountManagerInternal may have been destructed before this runs.
  static void RequestOperationInternal(
      account::UserAuthorizer* user_authorizer, OperationType operation,
      const base::Callback<void(const std::string&, uint64_t)>& post_result);

  // Posts the result of an operation from the account manager thread back to
  // the owning thread. Static because H5vccAccountManagerInternal may have been
  // destructed before this runs.
  static void PostResult(
      base::MessageLoop* message_loop,
      base::WeakPtr<H5vccAccountManagerInternal> h5vcc_account_manager,
      AccessTokenCallbackReference* token_callback, const std::string& token,
      uint64_t expiration_in_seconds);

  // Sends the result of an operation to the callback on the owning thread.
  void SendResult(AccessTokenCallbackReference* token_callback,
                  const std::string& token, uint64_t expiration_in_seconds);

  // The platform-specific user authorizer for getting access tokens.
  std::unique_ptr<account::UserAuthorizer> user_authorizer_;

  // Scoped holder of the callbacks that are currently waiting for a response.
  std::vector<std::unique_ptr<AccessTokenCallbackReference>> pending_callbacks_;

  // Thread checker for the thread that creates this instance.
  THREAD_CHECKER(thread_checker_);

  // The message loop that the H5vccAccountManagerInternal was created on. The
  // public interface must be called from this message loop, and callbacks will
  // be fired on this loop as well.
  base::MessageLoop* owning_message_loop_;

  // Each incoming request will have a corresponding task posted to this
  // thread's message loop and will be handled in a FIFO manner.
  // This is last so that all the other fields are valid before the thread gets
  // constructed, and they remain valid until the thread gets destructed and its
  // message loop gets flushed.
  base::Thread thread_;

  script::EnvironmentSettings* environment_settings_;

  friend class scoped_refptr<H5vccAccountManagerInternal>;
  DISALLOW_COPY_AND_ASSIGN(H5vccAccountManagerInternal);
};

}  // namespace h5vcc
}  // namespace cobalt

#endif  // COBALT_H5VCC_H5VCC_ACCOUNT_MANAGER_INTERNAL_H_
