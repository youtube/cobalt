// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/machine_learning/machine_learning_client.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/dbus/machine_learning/fake_machine_learning_client.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

// TODO(crbug.com/1163656): the tast test platform.MLServiceBootstrap flakiness
// shows ml-service bootstrap fails occasionally for timeout. Try to fix this
// with a long period (2 minutes).
constexpr base::TimeDelta kLongTimeout = base::Minutes(2);

MachineLearningClient* g_instance = nullptr;

class MachineLearningClientImpl : public MachineLearningClient {
 public:
  MachineLearningClientImpl() = default;

  MachineLearningClientImpl(const MachineLearningClientImpl&) = delete;
  MachineLearningClientImpl& operator=(const MachineLearningClientImpl&) =
      delete;

  ~MachineLearningClientImpl() override = default;

  // MachineLearningClient:
  void BootstrapMojoConnection(
      base::ScopedFD fd,
      base::OnceCallback<void(bool success)> result_callback) override {
    dbus::MethodCall method_call(ml::kMachineLearningInterfaceName,
                                 ml::kBootstrapMojoConnectionMethod);
    dbus::MessageWriter writer(&method_call);
    writer.AppendFileDescriptor(fd.get());
    ml_service_proxy_->CallMethod(
        &method_call, kLongTimeout.InMilliseconds(),
        base::BindOnce(
            &MachineLearningClientImpl::OnBootstrapMojoConnectionResponse,
            weak_ptr_factory_.GetWeakPtr(), std::move(result_callback)));
  }

  void Init(dbus::Bus* const bus) {
    ml_service_proxy_ =
        bus->GetObjectProxy(ml::kMachineLearningServiceName,
                            dbus::ObjectPath(ml::kMachineLearningServicePath));
  }

 private:
  raw_ptr<dbus::ObjectProxy> ml_service_proxy_ = nullptr;

  // Passes the success/failure of |dbus_response| on to |result_callback|.
  void OnBootstrapMojoConnectionResponse(
      base::OnceCallback<void(bool success)> result_callback,
      dbus::Response* const dbus_response) {
    const bool success = dbus_response != nullptr;
    std::move(result_callback).Run(success);
  }

  // Must be last class member.
  base::WeakPtrFactory<MachineLearningClientImpl> weak_ptr_factory_{this};
};

}  // namespace

MachineLearningClient::MachineLearningClient() {
  DCHECK(!g_instance);
  g_instance = this;
}

MachineLearningClient::~MachineLearningClient() {
  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

// static
void MachineLearningClient::Initialize(dbus::Bus* bus) {
  DCHECK(bus);
  (new MachineLearningClientImpl())->Init(bus);
}

// static
void MachineLearningClient::InitializeFake() {
  new FakeMachineLearningClient();
}

// static
void MachineLearningClient::Shutdown() {
  DCHECK(g_instance);
  delete g_instance;
}

// static
MachineLearningClient* MachineLearningClient::Get() {
  return g_instance;
}

}  // namespace chromeos
