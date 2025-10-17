// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ON_DEVICE_INTERNALS_ON_DEVICE_INTERNALS_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ON_DEVICE_INTERNALS_ON_DEVICE_INTERNALS_PAGE_HANDLER_H_

#include "chrome/browser/ui/webui/on_device_internals/on_device_internals_page.mojom.h"
#include "components/optimization_guide/core/optimization_guide_logger.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/on_device_model/public/cpp/buildflags.h"
#include "services/on_device_model/public/cpp/model_assets.h"
#include "services/on_device_model/public/mojom/on_device_model.mojom.h"
#include "services/on_device_model/public/mojom/on_device_model_service.mojom.h"

class OptimizationGuideKeyedService;

namespace on_device_internals {

// Handler for the internals page to receive and forward the log messages.
class PageHandler : public mojom::PageHandler,
                    public OptimizationGuideLogger::Observer {
 public:
  PageHandler(mojo::PendingReceiver<mojom::PageHandler> receiver,
              mojo::PendingRemote<mojom::Page> page,
              OptimizationGuideKeyedService* optimization_guide_keyed_service);
  ~PageHandler() override;

  PageHandler(const PageHandler&) = delete;
  PageHandler& operator=(const PageHandler&) = delete;

 private:
#if BUILDFLAG(USE_CHROMEOS_MODEL_SERVICE)
  using Service = on_device_model::mojom::OnDeviceModelPlatformService;
#else
  using Service = on_device_model::mojom::OnDeviceModelService;
#endif

  Service& GetService();

#if !BUILDFLAG(USE_CHROMEOS_MODEL_SERVICE)
  void OnModelAssetsLoaded(
      mojo::PendingReceiver<on_device_model::mojom::OnDeviceModel> model,
      LoadModelCallback callback,
      ml::ModelPerformanceHint performance_hint,
      on_device_model::ModelAssets assets);
  void OnModelLoaded(LoadModelCallback callback,
                     on_device_model::ModelFile weights,
                     on_device_model::mojom::LoadModelResult result);
#endif

  // mojom::PageHandler:
  void LoadModel(
      const base::FilePath& model_path,
      ml::ModelPerformanceHint performance_hint,
      mojo::PendingReceiver<on_device_model::mojom::OnDeviceModel> model,
      LoadModelCallback callback) override;
  void GetEstimatedPerformanceClass(
      GetEstimatedPerformanceClassCallback callback) override;
  void GetPageData(GetPageDataCallback callback) override;
  void DecodeBitmap(mojo_base::BigBuffer image_buffer,
                    DecodeBitmapCallback callback) override;
  void ResetModelCrashCount() override;

  // optimization_guide::OptimizationGuideLogger::Observer:
  void OnLogMessageAdded(base::Time event_time,
                         optimization_guide_common::mojom::LogSource log_source,
                         const std::string& source_file,
                         int source_line,
                         const std::string& message) override;

  mojo::Receiver<mojom::PageHandler> receiver_;
  mojo::Remote<mojom::Page> page_;

  mojo::Remote<Service> service_;

  // Logger to receive the debug logs from the optimization guide service. Not
  // owned. Guaranteed to outlive |this|, since the logger is owned by the
  // optimization guide keyed service, while |this| is part of
  // RenderFrameHostImpl::WebUIImpl.
  raw_ptr<OptimizationGuideLogger> optimization_guide_logger_;
  raw_ptr<OptimizationGuideKeyedService> optimization_guide_keyed_service_;

  base::WeakPtrFactory<PageHandler> weak_ptr_factory_{this};
};

}  // namespace on_device_internals

#endif  // CHROME_BROWSER_UI_WEBUI_ON_DEVICE_INTERNALS_ON_DEVICE_INTERNALS_PAGE_HANDLER_H_
