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
#include "base/synchronization/lock.h"

namespace net {

class DialSystemConfig {
 public:
  static DialSystemConfig* GetInstance();

  // Stores the pointers to various system info.
  const char* friendly_name() const { return friendly_name_.c_str(); }

  const char* manufacturer_name() const { return manufacturer_name_.c_str(); }

  const char* model_name() const { return model_name_.c_str(); }

  // Get's the model uuid.
  const char* model_uuid() const;

 private:
  DialSystemConfig();

  friend struct base::DefaultSingletonTraits<DialSystemConfig>;

  static void CreateDialUuid();

  // These 4 functions must be defined by a platform-specific source file.
  static std::string GetFriendlyName();
  static std::string GetManufacturerName();
  static std::string GetModelName();
  static std::string GeneratePlatformUuid();

  static const int kMaxNameSize = 64;

  std::string friendly_name_;
  std::string manufacturer_name_;
  std::string model_name_;

  DISALLOW_COPY_AND_ASSIGN(DialSystemConfig);
  mutable base::Lock lock_;
};

}  // namespace net

#endif  // NET_DIAL_DIAL_SYSTEM_CONFIG_H
