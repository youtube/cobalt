// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/dummy_ukm_data_manager.h"

#include "base/notreached.h"

namespace segmentation_platform {

DummyUkmDataManager::DummyUkmDataManager() = default;
DummyUkmDataManager::~DummyUkmDataManager() = default;

void DummyUkmDataManager::Initialize(const base::FilePath& database_path,
                                     UkmObserver* ukm_observer) {}

bool DummyUkmDataManager::IsUkmEngineEnabled() {
  return false;
}

void DummyUkmDataManager::StartObservingUkm(const UkmConfig& config) {}

void DummyUkmDataManager::PauseOrResumeObservation(bool pause) {}

UrlSignalHandler* DummyUkmDataManager::GetOrCreateUrlHandler() {
  NOTREACHED();
  return nullptr;
}

UkmDatabase* DummyUkmDataManager::GetUkmDatabase() {
  NOTREACHED();
  return nullptr;
}

void DummyUkmDataManager::OnEntryAdded(ukm::mojom::UkmEntryPtr entry) {}

void DummyUkmDataManager::OnUkmSourceUpdated(ukm::SourceId source_id,
                                             const std::vector<GURL>& urls) {}

void DummyUkmDataManager::AddRef() {}

void DummyUkmDataManager::RemoveRef() {}

}  // namespace segmentation_platform
