// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_LIBASSISTANT_CHROMIUM_API_DELEGATE_H_
#define CHROMEOS_ASH_SERVICES_LIBASSISTANT_CHROMIUM_API_DELEGATE_H_

#include <memory>

#include "chromeos/ash/services/libassistant/chromium_http_connection.h"
#include "chromeos/assistant/internal/libassistant/shared_headers.h"

namespace network {
class PendingSharedURLLoaderFactory;
}  // namespace network

namespace ash::libassistant {

class ChromiumHttpConnectionFactory;

class ChromiumApiDelegate : public assistant_client::ChromeOSApiDelegate {
 public:
  explicit ChromiumApiDelegate(
      std::unique_ptr<network::PendingSharedURLLoaderFactory>
          pending_url_loader_factory);

  ChromiumApiDelegate(const ChromiumApiDelegate&) = delete;
  ChromiumApiDelegate& operator=(const ChromiumApiDelegate&) = delete;

  ~ChromiumApiDelegate() override;

  // assistant_client::FuchsiaApiDelegate overrides:
  assistant_client::HttpConnectionFactory* GetHttpConnectionFactory() override;

  void OverrideDoNotDisturb(bool do_not_disturb_enabled) override {}

 private:
  ChromiumHttpConnectionFactory http_connection_factory_;
};

}  // namespace ash::libassistant

#endif  // CHROMEOS_ASH_SERVICES_LIBASSISTANT_CHROMIUM_API_DELEGATE_H_
