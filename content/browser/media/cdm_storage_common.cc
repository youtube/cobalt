// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/cdm_storage_common.h"

namespace content {

CdmFileId::CdmFileId(const std::string& name, const media::CdmType& cdm_type)
    : name(name), cdm_type(cdm_type) {}
CdmFileId::CdmFileId(const CdmFileId&) = default;
CdmFileId::~CdmFileId() = default;

CdmFileIdTwo::CdmFileIdTwo(const std::string& name,
                           const media::CdmType& cdm_type,
                           const blink::StorageKey& storage_key)
    : name(name), cdm_type(cdm_type), storage_key(storage_key) {}
CdmFileIdTwo::CdmFileIdTwo(const CdmFileIdTwo&) = default;
CdmFileIdTwo::~CdmFileIdTwo() = default;

CdmFileIdAndContents::CdmFileIdAndContents(const CdmFileId& file,
                                           std::vector<uint8_t> data)
    : file(file), data(std::move(data)) {}
CdmFileIdAndContents::CdmFileIdAndContents(const CdmFileIdAndContents&) =
    default;
CdmFileIdAndContents::~CdmFileIdAndContents() = default;

}  // namespace content
