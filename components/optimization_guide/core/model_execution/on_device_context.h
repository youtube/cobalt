// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_ON_DEVICE_CONTEXT_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_ON_DEVICE_CONTEXT_H_

#include <memory>

#include "components/optimization_guide/core/model_execution/multimodal_message.h"
#include "components/optimization_guide/core/model_execution/on_device_model_feature_adapter.h"
#include "components/optimization_guide/core/model_execution/safety_checker.h"
#include "components/optimization_guide/core/optimization_guide_logger.h"
#include "components/optimization_guide/proto/model_quality_metadata.pb.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace optimization_guide {

struct OnDeviceOptions final {
  OnDeviceOptions();
  OnDeviceOptions(const OnDeviceOptions&);
  OnDeviceOptions(OnDeviceOptions&&);
  ~OnDeviceOptions();

  class Client {
   public:
    virtual ~Client() = 0;
    // Create another client for the same model.
    virtual std::unique_ptr<Client> Clone() const = 0;
    // Called to check whether this client is still usable.
    virtual bool ShouldUse() = 0;
    // Called to retrieve connection the managed model.
    virtual mojo::Remote<on_device_model::mojom::OnDeviceModel>&
    GetModelRemote() = 0;
    // Called to report a successful execution of the model.
    virtual void OnResponseCompleted() = 0;
  };

  std::unique_ptr<Client> model_client;
  proto::OnDeviceModelVersions model_versions;
  scoped_refptr<const OnDeviceModelFeatureAdapter> adapter;
  std::unique_ptr<SafetyChecker> safety_checker;
  TokenLimits token_limits;

  base::WeakPtr<OptimizationGuideLogger> logger;
  base::WeakPtr<ModelQualityLogsUploaderService> log_uploader;

  // Returns true if the on-device model may be used.
  bool ShouldUse() const;
};

// Constructs an on-device session and populates it with input context.
// Context is processed incrementally. After the min context size has been
// processed, any pending context processing will be cancelled if an
// CloneSession() call is made.
class OnDeviceContext : public on_device_model::mojom::ContextClient {
 public:
  OnDeviceContext(OnDeviceOptions opts, ModelBasedCapabilityKey feature);
  ~OnDeviceContext() override;

  // Constructs the input context and begins processing it.
  bool SetInput(MultimodalMessageReadView request);

  // Get the session that we've sent the input to, creating it if does not
  // exist (e.g. due to a disconnect.)
  mojo::Remote<on_device_model::mojom::Session>& GetOrCreateSession();

  // Clones from the session to begin processing a request, terminating any
  // optional processing, and logging data about the processing.
  void CloneSession(
      mojo::PendingReceiver<on_device_model::mojom::Session> clone,
      proto::OnDeviceModelServiceRequest* logged_request,
      bool ignore_context);

  const OnDeviceOptions& opts() { return opts_; }

  // Whether using this session is still allowed.
  // This should be checked before called any other public methods.
  bool CanUse() { return opts_.ShouldUse(); }

 private:
  void AddContext();

  // on_device_model::mojom::ContextClient:
  void OnComplete(uint32_t tokens_processed) override;

  OnDeviceOptions opts_;
  ModelBasedCapabilityKey feature_;
  mojo::Remote<on_device_model::mojom::Session> session_;
  on_device_model::mojom::InputPtr input_;
  mojo::Receiver<on_device_model::mojom::ContextClient> client_{this};
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_ON_DEVICE_CONTEXT_H_
