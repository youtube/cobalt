// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_TEST_SUPPORT_SCOPED_DOM_STORAGE_DATABASE_FACTORY_FOR_TESTING_H_
#define COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_TEST_SUPPORT_SCOPED_DOM_STORAGE_DATABASE_FACTORY_FOR_TESTING_H_

#include "components/services/storage/dom_storage/dom_storage_database.h"

namespace storage {

// Overrides DomStorageDatabaseFactory::Open() and Destroy() for the duration
// of this object's lifetime. Tests provide callbacks that define the behavior
// of Open() and Destroy().
class ScopedDomStorageDatabaseFactoryForTesting {
 public:
  using OpenCallback = DomStorageDatabaseFactory::OpenCallback;
  using DestroyCallback = DomStorageDatabaseFactory::DestroyCallback;

  // Overload that only takes an OpenCallback. Uses a default Destroy()
  // implementation that always reports success (DbStatus::OK()).
  explicit ScopedDomStorageDatabaseFactoryForTesting(
      OpenCallback open_callback);

  ScopedDomStorageDatabaseFactoryForTesting(OpenCallback open_callback,
                                            DestroyCallback destroy_callback);
  ~ScopedDomStorageDatabaseFactoryForTesting();

  ScopedDomStorageDatabaseFactoryForTesting(
      const ScopedDomStorageDatabaseFactoryForTesting&) = delete;
  ScopedDomStorageDatabaseFactoryForTesting& operator=(
      const ScopedDomStorageDatabaseFactoryForTesting&) = delete;

 private:
  OpenCallback default_open_callback_;
  DestroyCallback default_destroy_callback_;
};

}  // namespace storage

#endif  // COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_TEST_SUPPORT_SCOPED_DOM_STORAGE_DATABASE_FACTORY_FOR_TESTING_H_
