// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/ble_adapter_manager.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"

namespace device {

BleAdapterManager::BleAdapterManager(FidoRequestHandlerBase* request_handler)
    : request_handler_(request_handler) {
  BluetoothAdapterFactory::Get()->GetAdapter(
      base::BindOnce(&BleAdapterManager::Start, weak_factory_.GetWeakPtr()));
}

BleAdapterManager::~BleAdapterManager() {
  if (adapter_) {
    adapter_->RemoveObserver(this);
  }
}

void BleAdapterManager::SetAdapterPower(bool set_power_on) {
  adapter_->SetPowered(set_power_on, base::DoNothing(), base::DoNothing());
}

void BleAdapterManager::AdapterPoweredChanged(BluetoothAdapter* adapter,
                                              bool powered) {
  request_handler_->OnBluetoothAdapterPowerChanged(powered);
}

void BleAdapterManager::Start(scoped_refptr<BluetoothAdapter> adapter) {
  DCHECK(!adapter_);
  adapter_ = std::move(adapter);
  DCHECK(adapter_);
  adapter_->AddObserver(this);

  request_handler_->OnBluetoothAdapterEnumerated(
      adapter_->IsPresent(), adapter_->IsPowered(), adapter_->CanPower(),
      adapter_->IsPeripheralRoleSupported());
}

}  // namespace device
