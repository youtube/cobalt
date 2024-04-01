// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FUCHSIA_CDM_CLIENT_MOJO_FUCHSIA_CDM_PROVIDER_H_
#define MEDIA_FUCHSIA_CDM_CLIENT_MOJO_FUCHSIA_CDM_PROVIDER_H_

#include "base/macros.h"
#include "media/fuchsia/cdm/fuchsia_cdm_provider.h"
#include "media/fuchsia/mojom/fuchsia_cdm_provider.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace blink {
class BrowserInterfaceBrokerProxy;
}

namespace media {

class MojoFuchsiaCdmProvider : public FuchsiaCdmProvider {
 public:
  // |interface_broker| must outlive this class.
  explicit MojoFuchsiaCdmProvider(
      blink::BrowserInterfaceBrokerProxy* interface_broker);

  MojoFuchsiaCdmProvider(const MojoFuchsiaCdmProvider&) = delete;
  MojoFuchsiaCdmProvider& operator=(const MojoFuchsiaCdmProvider&) = delete;

  ~MojoFuchsiaCdmProvider() override;

  // FuchsiaCdmProvider implementation:
  void CreateCdmInterface(
      const std::string& key_system,
      fidl::InterfaceRequest<fuchsia::media::drm::ContentDecryptionModule>
          cdm_request) override;

 private:
  blink::BrowserInterfaceBrokerProxy* const interface_broker_;
  mojo::Remote<media::mojom::FuchsiaCdmProvider> cdm_provider_;
};

}  // namespace media

#endif  // MEDIA_FUCHSIA_CDM_CLIENT_MOJO_FUCHSIA_CDM_PROVIDER_H_
