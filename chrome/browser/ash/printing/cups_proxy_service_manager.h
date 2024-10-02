// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PRINTING_CUPS_PROXY_SERVICE_MANAGER_H_
#define CHROME_BROWSER_ASH_PRINTING_CUPS_PROXY_SERVICE_MANAGER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/services/cups_proxy/cups_proxy_service.h"
#include "components/keyed_service/core/keyed_service.h"

namespace ash {

// This KeyedService is responsible for helping manage the
// lifetime of the CupsProxyService. This manager is started with the Profile
// and launches the service once the CupsProxyDaemon has started. Notice that
// this manager's interface is empty; this reflects the fact that the service's
// sole client is the daemon.
//
// Note: This manager is not fault-tolerant, i.e. should the service/daemon
// fail, we do not try to restart.
class CupsProxyServiceManager : public KeyedService {
 public:
  CupsProxyServiceManager();

  CupsProxyServiceManager(const CupsProxyServiceManager&) = delete;
  CupsProxyServiceManager& operator=(const CupsProxyServiceManager&) = delete;

  ~CupsProxyServiceManager() override;

 private:
  void OnDaemonAvailable(bool daemon_available);

  base::WeakPtrFactory<CupsProxyServiceManager> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_PRINTING_CUPS_PROXY_SERVICE_MANAGER_H_
