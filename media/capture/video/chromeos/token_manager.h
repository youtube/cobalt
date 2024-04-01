// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_CHROMEOS_TOKEN_MANAGER_H_
#define MEDIA_CAPTURE_VIDEO_CHROMEOS_TOKEN_MANAGER_H_

#include <array>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/thread_annotations.h"
#include "base/unguessable_token.h"
#include "media/capture/capture_export.h"
#include "media/capture/video/chromeos/mojom/cros_camera_service.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace media {

class CAPTURE_EXPORT TokenManager {
 public:
  static constexpr char kServerTokenPath[] = "/run/camera_tokens/server/token";
  static constexpr char kServerSensorClientTokenPath[] =
      "/run/camera_tokens/server/sensor_client_token";
  static constexpr char kTestClientTokenPath[] =
      "/run/camera_tokens/testing/token";
  static constexpr std::array<cros::mojom::CameraClientType, 3>
      kTrustedClientTypes = {cros::mojom::CameraClientType::CHROME,
                             cros::mojom::CameraClientType::ANDROID,
                             cros::mojom::CameraClientType::TESTING};

  TokenManager();
  ~TokenManager();

  bool GenerateServerToken();
  bool GenerateServerSensorClientToken();

  bool GenerateTestClientToken();

  base::UnguessableToken GetTokenForTrustedClient(
      cros::mojom::CameraClientType type);

  void RegisterPluginVmToken(const base::UnguessableToken& token);
  void UnregisterPluginVmToken(const base::UnguessableToken& token);

  bool AuthenticateServer(const base::UnguessableToken& token);
  bool AuthenticateServerSensorClient(const base::UnguessableToken& token);

  // Authenticates client with the given |type| and |token|. When |type| is
  // cros::mojom::CameraClientType::UNKNOWN, it tries to figure out the actual
  // client type by the supplied |token|. If authentication succeeds, it returns
  // the authenticated type of the client. If authentication fails,
  // absl::nullopt is returned.
  absl::optional<cros::mojom::CameraClientType> AuthenticateClient(
      cros::mojom::CameraClientType type,
      const base::UnguessableToken& token);

 private:
  friend class TokenManagerTest;
  friend class CameraHalDispatcherImplTest;

  void AssignServerTokenForTesting(const base::UnguessableToken& token);
  void AssignClientTokenForTesting(cros::mojom::CameraClientType type,
                                   const base::UnguessableToken& token);

  base::UnguessableToken server_token_;

  base::Lock client_token_map_lock_;
  base::flat_map<cros::mojom::CameraClientType,
                 base::flat_set<base::UnguessableToken>>
      client_token_map_ GUARDED_BY(client_token_map_lock_);
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_CHROMEOS_TOKEN_MANAGER_H_
