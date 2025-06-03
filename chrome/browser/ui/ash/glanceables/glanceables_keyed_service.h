// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_GLANCEABLES_GLANCEABLES_KEYED_SERVICE_H_
#define CHROME_BROWSER_UI_ASH_GLANCEABLES_GLANCEABLES_KEYED_SERVICE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "components/account_id/account_id.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"

class Profile;

namespace google_apis {
class RequestSender;
}  // namespace google_apis

namespace net {
struct NetworkTrafficAnnotationTag;
}  // namespace net

namespace signin {
class IdentityManager;
}  // namespace signin

namespace ash {

class GlanceablesClassroomClientImpl;
class TasksClientImpl;

// Browser context keyed service that owns implementations of interfaces from
// ash/ needed to communicate with different Google services as part of
// Glanceables project.
//
// As of March 2023, this service is created only for primary profiles (see
// `chrome/browser/ash/login/session/user_session_initializer.cc`) and does not
// support multi-user sign-in.
// TODO(b/269750741): Confirm timelines and revisit whether multi-user sign-in
// support is needed.
class GlanceablesKeyedService : public KeyedService {
 public:
  explicit GlanceablesKeyedService(Profile* profile);
  GlanceablesKeyedService(const GlanceablesKeyedService&) = delete;
  GlanceablesKeyedService& operator=(const GlanceablesKeyedService&) = delete;
  ~GlanceablesKeyedService() override;

  // KeyedService:
  void Shutdown() override;

 private:
  // Returns whether glanceables are enabled for the profile that owns the
  // GlanceablesKeyedService.
  bool AreGlanceablesEnabled() const;

  // Helper method that creates a `google_apis::RequestSender` instance.
  // `scopes` - OAuth 2 scopes needed for a client.
  // `traffic_annotation_tag` - describes requests issued by a client (for more
  // details see docs/network_traffic_annotations.md and
  // chrome/browser/privacy/traffic_annotation.proto).
  std::unique_ptr<google_apis::RequestSender> CreateRequestSenderForClient(
      const std::vector<std::string>& scopes,
      const net::NetworkTrafficAnnotationTag& traffic_annotation_tag) const;

  // Creates clients needed to communicate with different Google services, and
  // registers them with glanceables v2 controller in ash.
  void RegisterClients();

  // Resets clients needed to communicate with different Google services, and
  // clears any existing registrations with glanceables v2 controller in ash.
  void ClearClients();

  // Creates or clear clients that communicated with Google services, and
  // notifies `ash/` about created clients for `account_id_`.
  void UpdateRegistration();

  // The profile for which this keyed service was created.
  const raw_ptr<Profile, ExperimentalAsh> profile_;

  // Identity manager associated with `profile_`.
  const raw_ptr<signin::IdentityManager, DanglingUntriaged | ExperimentalAsh>
      identity_manager_;

  // Account id associated with the primary profile.
  const AccountId account_id_;

  // Instance of the `GlanceablesClassroomClient` interface implementation.
  std::unique_ptr<GlanceablesClassroomClientImpl> classroom_client_;

  // Instance of the `api::TasksClient` interface implementation.
  std::unique_ptr<TasksClientImpl> tasks_client_;

  // The registrar used to watch prefs changes.
  PrefChangeRegistrar pref_change_registrar_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_ASH_GLANCEABLES_GLANCEABLES_KEYED_SERVICE_H_
