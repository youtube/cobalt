// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/patchpanel/fake_patchpanel_client.h"

#include <utility>

#include "base/check_op.h"
#include "base/functional/bind.h"

namespace ash {

namespace {

FakePatchPanelClient* g_instance = nullptr;

}  // namespace

// static
FakePatchPanelClient* FakePatchPanelClient::Get() {
  return g_instance;
}

FakePatchPanelClient::FakePatchPanelClient() {
  DCHECK(!g_instance);
  g_instance = this;
}

FakePatchPanelClient::~FakePatchPanelClient() {
  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

void FakePatchPanelClient::GetDevices(GetDevicesCallback callback) {}

void FakePatchPanelClient::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void FakePatchPanelClient::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void FakePatchPanelClient::NotifyNetworkConfigurationChanged() {
  for (auto& observer : observer_list_)
    observer.NetworkConfigurationChanged();
}

}  // namespace ash
