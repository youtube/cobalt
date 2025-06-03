// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_LOCAL_DEVICE_DATA_FAKE_NEARBY_SHARE_LOCAL_DEVICE_DATA_MANAGER_H_
#define CHROME_BROWSER_NEARBY_SHARING_LOCAL_DEVICE_DATA_FAKE_NEARBY_SHARE_LOCAL_DEVICE_DATA_MANAGER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/nearby_sharing/local_device_data/nearby_share_local_device_data_manager.h"
#include "chrome/browser/nearby_sharing/local_device_data/nearby_share_local_device_data_manager_impl.h"
#include "chrome/browser/nearby_sharing/proto/rpc_resources.pb.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_share_settings.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class NearbyShareClientFactory;
class NearbyShareProfileInfoProvider;
class PrefService;

// A fake implementation of NearbyShareLocalDeviceDataManager, along with a fake
// factory, to be used in tests.
class FakeNearbyShareLocalDeviceDataManager
    : public NearbyShareLocalDeviceDataManager {
 public:
  // Factory that creates FakeNearbyShareLocalDeviceDataManager instances. Use
  // in NearbyShareLocalDeviceDataManagerImpl::Factory::SetFactoryForTesting()
  // in unit tests.
  class Factory : public NearbyShareLocalDeviceDataManagerImpl::Factory {
   public:
    Factory();
    ~Factory() override;

    // Returns all FakeNearbyShareLocalDeviceDataManager instances created by
    // CreateInstance().
    std::vector<FakeNearbyShareLocalDeviceDataManager*>& instances() {
      return instances_;
    }

    PrefService* latest_pref_service() const { return latest_pref_service_; }

    NearbyShareClientFactory* latest_http_client_factory() const {
      return latest_http_client_factory_;
    }

    NearbyShareProfileInfoProvider* latest_profile_info_provider() const {
      return latest_profile_info_provider_;
    }

   protected:
    std::unique_ptr<NearbyShareLocalDeviceDataManager> CreateInstance(
        PrefService* pref_service,
        NearbyShareClientFactory* http_client_factory,
        NearbyShareProfileInfoProvider* profile_info_provider) override;

   private:
    std::vector<FakeNearbyShareLocalDeviceDataManager*> instances_;
    raw_ptr<PrefService, ExperimentalAsh> latest_pref_service_ = nullptr;
    raw_ptr<NearbyShareClientFactory, DanglingUntriaged | ExperimentalAsh>
        latest_http_client_factory_ = nullptr;
    raw_ptr<NearbyShareProfileInfoProvider, DanglingUntriaged | ExperimentalAsh>
        latest_profile_info_provider_ = nullptr;
  };

  struct UploadContactsCall {
    UploadContactsCall(std::vector<nearbyshare::proto::Contact> contacts,
                       UploadCompleteCallback callback);
    UploadContactsCall(UploadContactsCall&&);
    ~UploadContactsCall();

    std::vector<nearbyshare::proto::Contact> contacts;
    UploadCompleteCallback callback;
  };

  struct UploadCertificatesCall {
    UploadCertificatesCall(
        std::vector<nearbyshare::proto::PublicCertificate> certificates,
        UploadCompleteCallback callback);
    UploadCertificatesCall(UploadCertificatesCall&&);
    ~UploadCertificatesCall();

    std::vector<nearbyshare::proto::PublicCertificate> certificates;
    UploadCompleteCallback callback;
  };

  explicit FakeNearbyShareLocalDeviceDataManager(
      const std::string& default_device_name);
  ~FakeNearbyShareLocalDeviceDataManager() override;

  // NearbyShareLocalDeviceDataManager:
  std::string GetId() override;
  std::string GetDeviceName() const override;
  absl::optional<std::string> GetFullName() const override;
  absl::optional<std::string> GetIconUrl() const override;
  nearby_share::mojom::DeviceNameValidationResult ValidateDeviceName(
      const std::string& name) override;
  nearby_share::mojom::DeviceNameValidationResult SetDeviceName(
      const std::string& name) override;
  void DownloadDeviceData() override;
  void UploadContacts(std::vector<nearbyshare::proto::Contact> contacts,
                      UploadCompleteCallback callback) override;
  void UploadCertificates(
      std::vector<nearbyshare::proto::PublicCertificate> certificates,
      UploadCompleteCallback callback) override;

  // Make protected observer-notification methods from base class public in this
  // fake class.
  using NearbyShareLocalDeviceDataManager::NotifyLocalDeviceDataChanged;

  void SetId(const std::string& id) { id_ = id; }
  void SetFullName(const absl::optional<std::string>& full_name);
  void SetIconUrl(const absl::optional<std::string>& icon_url);

  size_t num_download_device_data_calls() const {
    return num_download_device_data_calls_;
  }

  std::vector<UploadContactsCall>& upload_contacts_calls() {
    return upload_contacts_calls_;
  }

  std::vector<UploadCertificatesCall>& upload_certificates_calls() {
    return upload_certificates_calls_;
  }

  void set_next_validation_result(
      nearby_share::mojom::DeviceNameValidationResult result) {
    next_validation_result_ = result;
  }

 private:
  // NearbyShareLocalDeviceDataManager:
  void OnStart() override;
  void OnStop() override;

  std::string id_;
  std::string device_name_;
  absl::optional<std::string> full_name_;
  absl::optional<std::string> icon_url_;
  size_t num_download_device_data_calls_ = 0;
  std::vector<UploadContactsCall> upload_contacts_calls_;
  std::vector<UploadCertificatesCall> upload_certificates_calls_;
  nearby_share::mojom::DeviceNameValidationResult next_validation_result_ =
      nearby_share::mojom::DeviceNameValidationResult::kValid;
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_LOCAL_DEVICE_DATA_FAKE_NEARBY_SHARE_LOCAL_DEVICE_DATA_MANAGER_H_
