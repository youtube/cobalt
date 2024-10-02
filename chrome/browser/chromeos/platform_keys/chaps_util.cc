// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/platform_keys/chaps_util.h"

#include <ostream>

#include "base/functional/callback.h"
#include "base/no_destructor.h"
#include "chrome/browser/chromeos/platform_keys/chaps_slot_session.h"
#include "chrome/browser/chromeos/platform_keys/chaps_util_impl.h"

namespace chromeos {
namespace platform_keys {

namespace {

ChapsUtil::FactoryCallback& GetFactoryCallback() {
  static base::NoDestructor<ChapsUtil::FactoryCallback> s_callback;
  return *s_callback;
}

}  // namespace

// static
std::unique_ptr<ChapsUtil> ChapsUtil::Create() {
  if (!GetFactoryCallback().is_null()) {
    return GetFactoryCallback().Run();
  }
  return std::make_unique<ChapsUtilImpl>(
      std::make_unique<ChapsSlotSessionFactoryImpl>());
}

// static
void ChapsUtil::SetFactoryForTesting(const FactoryCallback& factory) {
  DCHECK(factory.is_null() || GetFactoryCallback().is_null())
      << "It is not expected that this is called with non-null callback when "
      << "another overriding callback is already set.";
  GetFactoryCallback() = factory;
}

}  // namespace platform_keys
}  // namespace chromeos
