// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CDM_CDM_DOCUMENT_SERVICE_H_
#define MEDIA_CDM_CDM_DOCUMENT_SERVICE_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "media/base/media_export.h"

#if defined(OS_WIN)
#include "media/cdm/media_foundation_cdm_data.h"
#endif  // defined(OS_WIN)

namespace media {

class MEDIA_EXPORT CdmDocumentService {
 public:
  CdmDocumentService() = default;
  CdmDocumentService(const CdmDocumentService& other) = delete;
  CdmDocumentService& operator=(const CdmDocumentService& other) = delete;
  virtual ~CdmDocumentService() = default;

  using ChallengePlatformCB =
      base::OnceCallback<void(bool success,
                              const std::string& signed_data,
                              const std::string& signed_data_signature,
                              const std::string& platform_key_certificate)>;
  using StorageIdCB =
      base::OnceCallback<void(uint32_t version,
                              const std::vector<uint8_t>& storage_id)>;

#if defined(OS_WIN)
  using GetMediaFoundationCdmDataCB =
      base::OnceCallback<void(std::unique_ptr<MediaFoundationCdmData>)>;
#endif  // defined(OS_WIN)

  // Allows authorized services to verify that the underlying platform is
  // trusted. An example of a trusted platform is a Chrome OS device in
  // verified boot mode. This can be used for protected content playback.
  //
  // |service_id| is the service ID for the |challenge|. |challenge| is the
  // challenge data. |callback| will be called with the following values:
  // - |success|: whether the platform is successfully verified. If true/false
  //              the following 3 parameters should be non-empty/empty.
  // - |signed_data|: the data signed by the platform.
  // - |signed_data_signature|: the signature of the signed data block.
  // - |platform_key_certificate|: the device specific certificate for the
  //                               requested service.
  virtual void ChallengePlatform(const std::string& service_id,
                                 const std::string& challenge,
                                 ChallengePlatformCB callback) = 0;

  // Requests a specific version of the device's Storage Id. If |version| = 0,
  // the latest available version will be returned. |callback| will be called
  // with the following values:
  // - |version|:    The version of the device's Storage Id being requested.
  // - |storage_id|: The device's Storage Id. It may be empty if Storage Id
  //                 is not supported by the platform, or if the requested
  //                 version does not exist.
  virtual void GetStorageId(uint32_t version, StorageIdCB callback) = 0;

#if defined(OS_WIN)
  // Gets the Media Foundation cdm data for the origin associated with the CDM.
  virtual void GetMediaFoundationCdmData(
      GetMediaFoundationCdmDataCB callback) = 0;

  // Sets the client token for the origin associated with the CDM. The token is
  // set by the content during license exchange. The token is then saved in the
  // Pref Service so that it can be reused next time the CDM request a new
  // license for that origin.
  virtual void SetCdmClientToken(const std::vector<uint8_t>& client_token) = 0;
#endif  // defined(OS_WIN)
};

}  // namespace media

#endif  // MEDIA_CDM_CDM_DOCUMENT_SERVICE_H_
