// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CLOUD_DEVICES_COMMON_CLOUD_DEVICE_DESCRIPTION_H_
#define COMPONENTS_CLOUD_DEVICES_COMMON_CLOUD_DEVICE_DESCRIPTION_H_

#include <memory>
#include <string>

#include "base/strings/string_piece_forward.h"
#include "base/values.h"

namespace cloud_devices {

// Provides parsing, serialization and validation Cloud Device Description or
// Cloud Job Ticket.
// https://developers.google.com/cloud-print/docs/cdd
class CloudDeviceDescription {
 public:
  CloudDeviceDescription();

  CloudDeviceDescription(const CloudDeviceDescription&) = delete;
  CloudDeviceDescription& operator=(const CloudDeviceDescription&) = delete;

  ~CloudDeviceDescription();

  bool InitFromString(const std::string& json);
  bool InitFromValue(base::Value::Dict value);

  std::string ToStringForTesting() const;

  base::Value ToValue() &&;

  // Returns item of given type with capability/option.
  // Returns nullptr if missing.
  const base::Value::Dict* GetDictItem(base::StringPiece path) const;
  const base::Value::List* GetListItem(base::StringPiece path) const;

  // Creates item with given type for capability/option.
  // Returns nullptr if an intermediate Value in the path is not a dictionary.
  base::Value::Dict* CreateDictItem(base::StringPiece path);
  base::Value::List* CreateListItem(base::StringPiece path);

 private:
  base::Value::Dict root_;
};

}  // namespace cloud_devices

#endif  // COMPONENTS_CLOUD_DEVICES_COMMON_CLOUD_DEVICE_DESCRIPTION_H_
