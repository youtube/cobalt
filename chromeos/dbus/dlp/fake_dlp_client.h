// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_DLP_FAKE_DLP_CLIENT_H_
#define CHROMEOS_DBUS_DLP_FAKE_DLP_CLIENT_H_

#include <string>

#include "base/containers/flat_map.h"
#include "base/observer_list.h"
#include "chromeos/dbus/dlp/dlp_client.h"
#include "chromeos/dbus/dlp/dlp_service.pb.h"
#include "dbus/object_proxy.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace chromeos {

class COMPONENT_EXPORT(DLP) FakeDlpClient : public DlpClient,
                                            public DlpClient::TestInterface {
 public:
  FakeDlpClient();
  FakeDlpClient(const FakeDlpClient&) = delete;
  FakeDlpClient& operator=(const FakeDlpClient&) = delete;
  ~FakeDlpClient() override;

  // DlpClient implementation:
  void SetDlpFilesPolicy(const dlp::SetDlpFilesPolicyRequest request,
                         SetDlpFilesPolicyCallback callback) override;
  void AddFile(const dlp::AddFileRequest request,
               AddFileCallback callback) override;
  void GetFilesSources(const dlp::GetFilesSourcesRequest request,
                       GetFilesSourcesCallback callback) override;
  void CheckFilesTransfer(const dlp::CheckFilesTransferRequest request,
                          CheckFilesTransferCallback callback) override;
  void RequestFileAccess(const dlp::RequestFileAccessRequest request,
                         RequestFileAccessCallback callback) override;
  bool IsAlive() const override;
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  bool HasObserver(const Observer* observer) const override;
  DlpClient::TestInterface* GetTestInterface() override;

  // DlpClient::TestInterface implementation:
  int GetSetDlpFilesPolicyCount() const override;
  void SetFakeSource(const std::string& fake_source) override;
  void SetCheckFilesTransferResponse(
      dlp::CheckFilesTransferResponse response) override;
  void SetFileAccessAllowed(bool allowed) override;
  void SetIsAlive(bool is_alive) override;
  void SetAddFileMock(AddFileCall mock) override;
  void SetGetFilesSourceMock(GetFilesSourceCall mock) override;
  dlp::CheckFilesTransferRequest GetLastCheckFilesTransferRequest()
      const override;
  void SetRequestFileAccessMock(RequestFileAccessCall mock) override;

 private:
  int set_dlp_files_policy_count_ = 0;
  bool file_access_allowed_ = true;
  bool is_alive_ = true;
  base::flat_map<ino_t, std::string> files_database_;
  absl::optional<std::string> fake_source_;
  absl::optional<dlp::CheckFilesTransferResponse>
      check_files_transfer_response_;
  absl::optional<AddFileCall> add_file_mock_;
  absl::optional<GetFilesSourceCall> get_files_source_mock_;
  dlp::CheckFilesTransferRequest last_check_files_transfer_request_;
  absl::optional<RequestFileAccessCall> request_file_access_mock_;
  base::ObserverList<Observer> observers_;
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_DLP_FAKE_DLP_CLIENT_H_
