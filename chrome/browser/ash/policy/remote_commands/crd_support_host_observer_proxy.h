// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_CRD_SUPPORT_HOST_OBSERVER_PROXY_H_
#define CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_CRD_SUPPORT_HOST_OBSERVER_PROXY_H_

#include <cstdint>
#include <string>

#include "base/observer_list.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "remoting/host/mojom/remote_support.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {
class TimeDelta;
}  // namespace base

namespace policy {

class CrdSessionObserver;
enum class ResultCode;

// A proxy that translates and forwards `SupportHostObserver` events to the
// corresponding `CrdSessionObserver` events.
class SupportHostObserverProxy : public remoting::mojom::SupportHostObserver {
 public:
  SupportHostObserverProxy();
  SupportHostObserverProxy(const SupportHostObserverProxy&) = delete;
  SupportHostObserverProxy& operator=(const SupportHostObserverProxy&) = delete;
  ~SupportHostObserverProxy() override;

  void AddObserver(CrdSessionObserver* observer);

  void Bind(
      mojo::PendingReceiver<remoting::mojom::SupportHostObserver> receiver);
  void Unbind();
  bool IsBound() const;

  // `remoting::mojom::SupportHostObserver` implementation:
  void OnHostStateStarting() override;
  void OnHostStateRequestedAccessCode() override;
  void OnHostStateReceivedAccessCode(const std::string& access_code,
                                     base::TimeDelta lifetime) override;
  void OnHostStateConnecting() override;
  void OnHostStateConnected(const std::string& remote_username) override;
  void OnHostStateDisconnected(
      const absl::optional<std::string>& disconnect_reason) override;
  void OnNatPolicyChanged(
      remoting::mojom::NatPolicyStatePtr nat_policy_state) override;
  void OnHostStateError(int64_t error_code) override;
  void OnPolicyError() override;
  void OnInvalidDomainError() override;

  void ReportHostStopped(ResultCode error_code,
                         const std::string& error_message);

 private:
  mojo::Receiver<remoting::mojom::SupportHostObserver> receiver_{this};
  base::ObserverList<CrdSessionObserver> observers_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_CRD_SUPPORT_HOST_OBSERVER_PROXY_H_
