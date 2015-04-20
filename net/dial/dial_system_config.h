// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Dial requires some system information exposed to the world, for example
// the device name, etc. This class lets it be configurable, but throw default
// values from implementation.

#ifndef NET_DIAL_DIAL_SYSTEM_CONFIG_H
#define NET_DIAL_DIAL_SYSTEM_CONFIG_H

#include <string>

#include "base/basictypes.h"
#include "base/memory/singleton.h"

namespace net {

class DialSystemConfig {
 public:
  static DialSystemConfig* GetInstance();

  // Stores the pointers to various system info.
  const char* friendly_name_;
  const char* manufacturer_name_;
  const char* model_name_;

  // Get's the model uuid.
  const char* model_uuid() const;

 private:
  DialSystemConfig();

  friend struct DefaultSingletonTraits<DialSystemConfig>;

  static void CreateDialUuid();

  // These are implemented in the platform specific file.
  static const char* kDefaultFriendlyName;
  static const char* kDefaultManufacturerName;
  static const char* kDefaultModelName;
  static std::string GeneratePlatformUuid();

  DISALLOW_COPY_AND_ASSIGN(DialSystemConfig);
};

}

#endif // NET_DIAL_DIAL_SYSTEM_CONFIG_H

