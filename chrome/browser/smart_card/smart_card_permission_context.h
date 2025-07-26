// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SMART_CARD_SMART_CARD_PERMISSION_CONTEXT_H_
#define CHROME_BROWSER_SMART_CARD_SMART_CARD_PERMISSION_CONTEXT_H_

#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/smart_card/smart_card_permission_request.h"
#include "chrome/browser/smart_card/smart_card_reader_tracker.h"
#include "components/permissions/object_permission_context_base.h"
#include "url/origin.h"

class Profile;

namespace content {
class RenderFrameHost;
}  // namespace content
namespace settings {
class SmartCardReaderPermissionsSiteSettingsHandlerTest;
}  // namespace settings

class SmartCardPermissionContext
    : public permissions::ObjectPermissionContextBase {
 public:
  // Callback type to report whether the user allowed the connection request.
  using RequestReaderPermissionCallback = base::OnceCallback<void(bool)>;

  explicit SmartCardPermissionContext(Profile* profile);
  SmartCardPermissionContext(const SmartCardPermissionContext&) = delete;
  SmartCardPermissionContext& operator=(const SmartCardPermissionContext&) =
      delete;
  ~SmartCardPermissionContext() override;

  bool HasReaderPermission(content::RenderFrameHost& render_frame_host,
                           const std::string& reader_name);

  void RequestReaderPermisssion(content::RenderFrameHost& render_frame_host,
                                const std::string& reader_name,
                                RequestReaderPermissionCallback callback);

  // permissions::ObjectPermissionContextBase:
  std::string GetKeyForObject(const base::Value::Dict& object) override;
  bool IsValidObject(const base::Value::Dict& object) override;
  std::u16string GetObjectDisplayName(const base::Value::Dict& object) override;

  void RevokeEphemeralPermissions();
  void RevokeAllPermissions();
  void RevokePersistentPermission(const std::string& reader_name,
                                  const url::Origin& origin);

  struct ReaderGrants {
    ReaderGrants(const std::string& reader_name,
                 const std::vector<url::Origin>& origins);
    ~ReaderGrants();

    ReaderGrants(const ReaderGrants& other);
    bool operator==(const ReaderGrants& other) const;

    std::string reader_name;
    std::vector<url::Origin> origins;
  };

  // Returns persistent grants, grouped by reader.
  std::vector<ReaderGrants> GetPersistentReaderGrants();

  // Checks whether |origin|'s value of |guard_content_settings_type_| is both:
  // - set to "allow"
  // - set by policy
  bool IsAllowlistedByPolicy(const url::Origin& origin);

  // Overridden to expose a symbolic "All readers" device in case of
  // allowlisting via policy.
  std::vector<std::unique_ptr<Object>> GetGrantedObjects(
      const url::Origin& origin) override;

 private:
  friend class SmartCardPermissionContextTest;
  friend class settings::SmartCardReaderPermissionsSiteSettingsHandlerTest;
  friend class PageInfoBubbleViewInteractiveUiTest;

  class OneTimeObserver;
  class PowerSuspendObserver;
  class ReaderObserver;

  bool HasReaderPermission(const url::Origin& origin,
                           const std::string& reader_name);

  // The given permission won't be persisted.
  void GrantEphemeralReaderPermission(const url::Origin& origin,
                                      const std::string& reader_name);

  void GrantPersistentReaderPermission(const url::Origin& origin,
                                       const std::string& reader_name);

  bool HasPersistentReaderPermission(const url::Origin& origin,
                                     const std::string& reader_name);

  void RevokeEphemeralPermissionsForReader(const std::string& reader_name);
  void RevokeEphemeralPermissionsForOrigin(const url::Origin& origin);

  void OnTrackingStarted(
      std::optional<std::vector<SmartCardReaderTracker::ReaderInfo>>);

  void StopObserving();

  void OnPermissionRequestDecided(const url::Origin& origin,
                                  const std::string& reader_name,
                                  RequestReaderPermissionCallback callback,
                                  SmartCardPermissionRequest::Result result);

  void OnPermissionDenied(const url::Origin& origin);

  SmartCardReaderTracker& GetReaderTracker() const;

  // Set of readers to which an origin has ephemeral access to.
  std::map<url::Origin, std::set<std::string>> ephemeral_grants_;

  // this is for tracking consecutive denials (after 3, guard setting is to be
  // set to blocked)
  std::map<url::Origin, uint8_t> consecutive_denials_;

  std::unique_ptr<OneTimeObserver> one_time_observer_;
  std::unique_ptr<PowerSuspendObserver> power_suspend_observer_;
  std::unique_ptr<ReaderObserver> reader_observer_;

  // Instance is owned by this profile.
  base::raw_ref<Profile> profile_;

  base::WeakPtrFactory<SmartCardPermissionContext> weak_ptr_factory_;
};

#endif  // CHROME_BROWSER_SMART_CARD_SMART_CARD_PERMISSION_CONTEXT_H_
