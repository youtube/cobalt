// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_TEST_MOCK_DISPLAY_CLIENT_H_
#define COMPONENTS_VIZ_TEST_MOCK_DISPLAY_CLIENT_H_

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "gpu/command_buffer/common/context_result.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/viz/privileged/mojom/compositing/display_private.mojom.h"
#include "services/viz/privileged/mojom/compositing/frame_sink_manager.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace viz {

class MockDisplayClient : public mojom::DisplayClient {
 public:
  MockDisplayClient();

  MockDisplayClient(const MockDisplayClient&) = delete;
  MockDisplayClient& operator=(const MockDisplayClient&) = delete;

  ~MockDisplayClient() override;

  mojo::PendingRemote<mojom::DisplayClient> BindRemote();

  // mojom::DisplayClient implementation.
#if BUILDFLAG(IS_APPLE)
  MOCK_METHOD1(OnDisplayReceivedCALayerParams, void(const gfx::CALayerParams&));
#endif
#if BUILDFLAG(IS_WIN)
  MOCK_METHOD1(CreateLayeredWindowUpdater,
               void(mojo::PendingReceiver<mojom::LayeredWindowUpdater>));
  MOCK_METHOD1(AddChildWindowToBrowser, void(gpu::SurfaceHandle child_window));
#endif
#if BUILDFLAG(IS_ANDROID)
  MOCK_METHOD1(DidCompleteSwapWithSize, void(const gfx::Size&));
  MOCK_METHOD1(OnContextCreationResult, void(gpu::ContextResult));
  MOCK_METHOD1(SetWideColorEnabled, void(bool enabled));
  MOCK_METHOD1(SetPreferredRefreshRate, void(float refresh_rate));
#endif
// TODO(crbug.com/1052397): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
  MOCK_METHOD1(DidCompleteSwapWithNewSize, void(const gfx::Size&));
#endif

 private:
  mojo::Receiver<mojom::DisplayClient> receiver_{this};
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_TEST_MOCK_DISPLAY_CLIENT_H_
