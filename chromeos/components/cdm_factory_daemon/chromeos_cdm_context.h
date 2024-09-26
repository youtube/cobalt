// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_CDM_FACTORY_DAEMON_CHROMEOS_CDM_CONTEXT_H_
#define CHROMEOS_COMPONENTS_CDM_FACTORY_DAEMON_CHROMEOS_CDM_CONTEXT_H_

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "media/base/cdm_context.h"
#include "media/base/decryptor.h"
#include "ui/gfx/geometry/size.h"

namespace media {
class DecryptConfig;
}

namespace chromeos {

// Interface for ChromeOS CDM Factory Daemon specific extensions to the
// CdmContext interface.
class ChromeOsCdmContext {
 public:
  ChromeOsCdmContext() = default;

  using GetHwKeyDataCB =
      base::OnceCallback<void(media::Decryptor::Status status,
                              const std::vector<uint8_t>& key_data)>;
  using GetHwConfigDataCB =
      base::OnceCallback<void(bool success,
                              const std::vector<uint8_t>& config_data)>;
  using GetScreenResolutionsCB =
      base::OnceCallback<void(const std::vector<gfx::Size>& resolutions)>;

  // Gets the HW specific key information for the key specified in
  // |decrypt_config| and returns it via |callback|.
  virtual void GetHwKeyData(const media::DecryptConfig* decrypt_config,
                            const std::vector<uint8_t>& hw_identifier,
                            GetHwKeyDataCB callback) = 0;

  // Used to get hardware specific configuration data from the daemon to be used
  // for setting up decrypt+decode in the GPU.
  virtual void GetHwConfigData(GetHwConfigDataCB callback) = 0;

  // Used to get screen resolutions from the browser process so we can optimize
  // our decode target size.
  virtual void GetScreenResolutions(GetScreenResolutionsCB callback) = 0;

  // Gets a CdmContextRef linked with the associated CDM for keeping it alive.
  virtual std::unique_ptr<media::CdmContextRef> GetCdmContextRef() = 0;

  // Returns true if this is coming from a CDM in ARC.
  virtual bool UsingArcCdm() const = 0;

  // Returns true if this is coming from a remote CDM in another process or
  // operating system. This is used to determine if certain processing has
  // already been done on the data such as transcryption.
  virtual bool IsRemoteCdm() const = 0;

 protected:
  virtual ~ChromeOsCdmContext() = default;
};

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_CDM_FACTORY_DAEMON_CHROMEOS_CDM_CONTEXT_H_