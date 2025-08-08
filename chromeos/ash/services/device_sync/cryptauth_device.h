// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_DEVICE_SYNC_CRYPTAUTH_DEVICE_H_
#define CHROMEOS_ASH_SERVICES_DEVICE_SYNC_CRYPTAUTH_DEVICE_H_

#include <map>
#include <ostream>
#include <string>

#include "base/time/time.h"
#include "base/values.h"
#include "chromeos/ash/components/multidevice/software_feature.h"
#include "chromeos/ash/components/multidevice/software_feature_state.h"
#include "chromeos/ash/services/device_sync/proto/cryptauth_better_together_device_metadata.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {

namespace device_sync {

// Holds information for a device managed by CryptAuth.
class CryptAuthDevice {
 public:
  // Returns null if |dict| cannot be converted into a CryptAuthDevice.
  static absl::optional<CryptAuthDevice> FromDictionary(
      const base::Value::Dict& dict);

  // |instance_id|: The Instance ID, used as a unique device identifier. Cannot
  //                be empty.
  explicit CryptAuthDevice(const std::string& instance_id);
  CryptAuthDevice(
      const std::string& instance_id,
      const std::string& device_name,
      const std::string& device_better_together_public_key,
      const base::Time& last_update_time,
      const absl::optional<cryptauthv2::BetterTogetherDeviceMetadata>&
          better_together_device_metadata,
      const std::map<multidevice::SoftwareFeature,
                     multidevice::SoftwareFeatureState>& feature_states);

  CryptAuthDevice(const CryptAuthDevice&);

  ~CryptAuthDevice();

  const std::string& instance_id() const { return instance_id_; }

  // Converts CryptAuthDevice to a dictionary-type Value of the form
  //   {
  //     "instance_id": |instance_id_|,
  //     "device_name": |device_name_|,
  //     "device_better_together_public_key_":
  //         <|device_better_together_public_key_| base64 encoded>,
  //     "last_update_time": <|last_update_time_| converted to string>,
  //     "better_together_device_metadata":
  //         <|better_together_device_metadata_| serialized and base64 encoded>,
  //     "feature_states": <|feature_states_| as dictionary>
  //   }
  base::Value::Dict AsDictionary() const;

  // Converts the device to a human-readable dictionary.
  // Note: We remove |last_update_time_| which can be misleading in the logs.
  base::Value::Dict AsReadableDictionary() const;

  bool operator==(const CryptAuthDevice& other) const;
  bool operator!=(const CryptAuthDevice& other) const;

  // The name known to the CryptAuth server or which was assigned by the user to
  // the device. Might contain personally identifiable information.
  std::string device_name;

  // The device's "DeviceSync:BetterTogether" public key that is enrolled with
  // CryptAuth--known as CryptAuthKeyBundle::kDeviceSyncBetterTogether on Chrome
  // OS. This is not to be confused with the shared, unenrolled group public
  // key.
  std::string device_better_together_public_key;

  // The last time the device's information in CryptAuth server was touched.
  // Note: This is not a reliable indicator of the last-used time. A device's
  // data in CryptAuth can be changed without any action from the device. See
  // GetDevicesActivityStatus for more reliable timestamps.
  base::Time last_update_time;

  // Device metadata relevant to the suite of multi-device ("Better Together")
  // features. Null if metadata could not be decrypted.
  absl::optional<cryptauthv2::BetterTogetherDeviceMetadata>
      better_together_device_metadata;

  // A map from the multi-device feature type (example: kBetterTogetherHost) to
  // feature state (example: kEnabled).
  std::map<multidevice::SoftwareFeature, multidevice::SoftwareFeatureState>
      feature_states;

 private:
  std::string instance_id_;
};

std::ostream& operator<<(std::ostream& stream, const CryptAuthDevice& device);

}  // namespace device_sync

}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_DEVICE_SYNC_CRYPTAUTH_DEVICE_H_
