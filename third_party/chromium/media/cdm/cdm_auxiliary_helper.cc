// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cdm/cdm_auxiliary_helper.h"

#include "media/base/cdm_context.h"
#include "media/cdm/cdm_helpers.h"

#if defined(OS_WIN)
#include "media/cdm/media_foundation_cdm_data.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#endif  // defined(OS_WIN)

namespace media {

CdmAuxiliaryHelper::CdmAuxiliaryHelper() = default;
CdmAuxiliaryHelper::~CdmAuxiliaryHelper() = default;

void CdmAuxiliaryHelper::SetFileReadCB(FileReadCB file_read_cb) {}

cdm::FileIO* CdmAuxiliaryHelper::CreateCdmFileIO(cdm::FileIOClient* client) {
  return nullptr;
}

url::Origin CdmAuxiliaryHelper::GetCdmOrigin() {
  return url::Origin();
}

cdm::Buffer* CdmAuxiliaryHelper::CreateCdmBuffer(size_t capacity) {
  return nullptr;
}

std::unique_ptr<VideoFrameImpl> CdmAuxiliaryHelper::CreateCdmVideoFrame() {
  return nullptr;
}

void CdmAuxiliaryHelper::QueryStatus(QueryStatusCB callback) {
  std::move(callback).Run(false, 0, 0);
}

void CdmAuxiliaryHelper::EnableProtection(uint32_t desired_protection_mask,
                                          EnableProtectionCB callback) {
  std::move(callback).Run(false);
}

void CdmAuxiliaryHelper::ChallengePlatform(const std::string& service_id,
                                           const std::string& challenge,
                                           ChallengePlatformCB callback) {
  std::move(callback).Run(false, "", "", "");
}

void CdmAuxiliaryHelper::GetStorageId(uint32_t version, StorageIdCB callback) {
  std::move(callback).Run(version, std::vector<uint8_t>());
}

#if defined(OS_WIN)
void CdmAuxiliaryHelper::GetMediaFoundationCdmData(
    GetMediaFoundationCdmDataCB callback) {
  std::move(callback).Run(std::make_unique<MediaFoundationCdmData>(
      base::UnguessableToken::Null(), absl::nullopt, base::FilePath()));
}

void CdmAuxiliaryHelper::SetCdmClientToken(
    const std::vector<uint8_t>& client_token) {}
#endif  // defined(OS_WIN)

}  // namespace media
