// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Dial requires some system information exposed to the world, for example
// the device name, etc. This class lets it be configurable, but throw default
// values from implementation.

#include "base/basictypes.h"
#include "base/memory/singleton.h"

namespace net {

class DialSystemConfig {
 public:
  static DialSystemConfig* GetInstance() {
    return Singleton<DialSystemConfig>::get();
  }

  // Stores the pointers to various system info.
  const char* friendly_name_;
  const char* manufacturer_name_;
  const char* model_name_;
  const char* model_uuid_;

 private:
  DialSystemConfig()
      : friendly_name_(kDefaultFriendlyName)
      , manufacturer_name_(kDefaultManufacturerName)
      , model_name_(kDefaultModelName)
      , model_uuid_(kDefaultModelUuid) {
  }

  // These are implemented in the platform specific file.
  static const char* kDefaultFriendlyName;
  static const char* kDefaultManufacturerName;
  static const char* kDefaultModelName;
  static const char* kDefaultModelUuid;

  friend struct DefaultSingletonTraits<DialSystemConfig>;

  DISALLOW_COPY_AND_ASSIGN(DialSystemConfig);
};

}

