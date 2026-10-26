// Copyright 2026 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "cobalt/renderer/cobalt_content_renderer_client.h"

#include <string>

#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/task/thread_pool.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "media/base/audio_codecs.h"
#include "media/base/eme_constants.h"
#include "media/base/encryption_scheme.h"
#include "media/base/video_codecs.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_IOS_TVOS) && defined(COBALT_INTERNAL_BUILD)
#include "cobalt/internal/cobalt/components/cdm/renderer/starboard/platform_drm_key_system_info.h"
#endif  // BUILDFLAG(IS_IOS_TVOS) && defined(COBALT_INTERNAL_BUILD)

namespace cobalt {
namespace {

TEST(CobaltContentRendererClientTest, MediaQueriesCallableFromWorkerThread) {
  base::test::TaskEnvironment task_environment;
  CobaltContentRendererClient client;

  base::RunLoop run_loop;
  base::ThreadPool::PostTask(
      FROM_HERE,
      base::BindOnce(
          [](CobaltContentRendererClient* client,
             base::OnceClosure quit_closure) {
            client->IsDecoderSupportedAudioType({::media::AudioCodec::kAAC});
            client->IsDecoderSupportedVideoType({::media::VideoCodec::kH264});
            client->GetSupportedKeySystems(
                /*render_frame=*/nullptr,
                base::BindRepeating([](::media::KeySystemInfos key_systems) {
                  EXPECT_FALSE(key_systems.empty());
                }));
            std::move(quit_closure).Run();
          },
          base::Unretained(&client), run_loop.QuitClosure()));
  run_loop.Run();
}

#if BUILDFLAG(IS_IOS_TVOS) && defined(COBALT_INTERNAL_BUILD)

// Returns the base key system name the client registers for `key_system`, or
// an empty string if no registered key system claims that name.
std::string RegisteredBaseKeySystemName(const std::string& key_system) {
  CobaltContentRendererClient client;
  std::string base_name;
  client.GetSupportedKeySystems(
      /*render_frame=*/nullptr,
      base::BindRepeating(
          [](std::string* base_name, const std::string& key_system,
             ::media::KeySystemInfos key_systems) {
            for (const auto& info : key_systems) {
              if (info->IsSupportedKeySystem(key_system)) {
                *base_name = info->GetBaseKeySystemName();
                return;
              }
            }
          },
          base::Unretained(&base_name), key_system));
  return base_name;
}

TEST(CobaltContentRendererClientTest, PlatformDrmKeySystemIsRegistered) {
  base::test::TaskEnvironment task_environment;

  const cdm::PlatformDrmKeySystemInfo info(
      ::media::EME_CODEC_NONE, base::flat_set<::media::EncryptionScheme>{},
      ::media::EME_CODEC_NONE, ::media::EmeFeatureSupport::ALWAYS_ENABLED,
      ::media::EmeFeatureSupport::ALWAYS_ENABLED);
  const std::string key_system = info.GetBaseKeySystemName();

  EXPECT_EQ(RegisteredBaseKeySystemName(key_system), key_system);
}

TEST(CobaltContentRendererClientTest, PlatformDrmSbdlKeySystemIsRegistered) {
  base::test::TaskEnvironment task_environment;

  const cdm::PlatformDrmSbdlKeySystemInfo sbdl_info(
      base::flat_set<::media::EncryptionScheme>{},
      ::media::EmeFeatureSupport::ALWAYS_ENABLED,
      ::media::EmeFeatureSupport::ALWAYS_ENABLED);
  const std::string sbdl_key_system = sbdl_info.GetBaseKeySystemName();

  EXPECT_EQ(RegisteredBaseKeySystemName(sbdl_key_system), sbdl_key_system);
}

#endif  // BUILDFLAG(IS_IOS_TVOS) && defined(COBALT_INTERNAL_BUILD)

}  // namespace
}  // namespace cobalt
