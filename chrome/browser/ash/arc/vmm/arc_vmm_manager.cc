// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/vmm/arc_vmm_manager.h"

#include "ash/accelerators/accelerator_controller_impl.h"
#include "ash/components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "ash/components/arc/arc_features.h"
#include "ash/components/arc/arc_util.h"
#include "ash/public/cpp/accelerators.h"
#include "ash/shell.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ash/arc/vmm/arc_vmm_swap_scheduler.h"
#include "chromeos/ash/components/dbus/concierge/concierge_client.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/accelerators/accelerator.h"

namespace arc {

namespace {
class ArcVmmManagerFactory
    : public internal::ArcBrowserContextKeyedServiceFactoryBase<
          ArcVmmManager,
          ArcVmmManagerFactory> {
 public:
  static constexpr const char* kName = "ArcVmmManagerFactory";
  static ArcVmmManagerFactory* GetInstance() {
    static base::NoDestructor<ArcVmmManagerFactory> instance;
    return instance.get();
  }

 private:
  friend class base::NoDestructor<ArcVmmManagerFactory>;

  ArcVmmManagerFactory() = default;
  ~ArcVmmManagerFactory() override = default;
};

}  // namespace

// static
ArcVmmManager* ArcVmmManager::GetForBrowserContext(
    content::BrowserContext* context) {
  return ArcVmmManagerFactory::GetForBrowserContext(context);
}

// static
void ArcVmmManager::EnsureFactoryBuilt() {
  ArcVmmManagerFactory::GetInstance();
}

// static
ArcVmmManager* ArcVmmManager::GetForBrowserContextForTesting(
    content::BrowserContext* context) {
  return ArcVmmManagerFactory::GetForBrowserContextForTesting(context);
}

ArcVmmManager::ArcVmmManager(content::BrowserContext* context,
                             ArcBridgeService* bridge) {
  if (base::FeatureList::IsEnabled(kVmmSwapKeyboardShortcut)) {
    accelerator_ = std::make_unique<AcceleratorTarget>(this);
  }
  if (base::FeatureList::IsEnabled(kVmmSwapPolicy)) {
    swap_out_delay_ = base::Seconds(kVmmSwapOutDelaySecond.Get());
    scheduler_ = std::make_unique<ArcVmmSwapScheduler>(
        base::BindRepeating(
            [](base::WeakPtr<ArcVmmManager> manager, bool enable) {
              if (manager) {
                manager->SetSwapState(enable ? SwapState::ENABLE
                                             : SwapState::DISABLE);
              }
            },
            weak_ptr_factory_.GetWeakPtr()),
        /* minimum_swapout_interval= */
        base::Seconds(kVmmSwapOutTimeIntervalSecond.Get()),
        /* swappable_checking_period= */
        base::Seconds(kVmmSwapArcSilenceIntervalSecond.Get()),
        std::make_unique<ArcSystemStateObservation>(context));
  }
}

ArcVmmManager::~ArcVmmManager() = default;

void ArcVmmManager::SetSwapState(SwapState state) {
  switch (state) {
    case SwapState::ENABLE:
      SendSwapRequest(vm_tools::concierge::SwapOperation::ENABLE,
                      base::DoNothing());
      break;
    case SwapState::ENABLE_WITH_SWAPOUT:
      SendSwapRequest(
          vm_tools::concierge::SwapOperation::ENABLE,
          base::BindOnce(
              &ArcVmmManager::PostWithSwapDelay, weak_ptr_factory_.GetWeakPtr(),
              base::BindOnce(&ArcVmmManager::SendSwapRequest,
                             weak_ptr_factory_.GetWeakPtr(),
                             vm_tools::concierge::SwapOperation::SWAPOUT,
                             base::DoNothing())));
      break;
    case SwapState::DISABLE:
      SendSwapRequest(vm_tools::concierge::SwapOperation::DISABLE,
                      base::DoNothing());
      break;
  }
}

void ArcVmmManager::SendSwapRequest(
    vm_tools::concierge::SwapOperation operation,
    base::OnceClosure success_callback) {
  auto* client = ash::ConciergeClient::Get();
  if (!client) {
    LOG(ERROR) << "Cannot find concierge client to swap ARCVM";
    return;
  }

  vm_tools::concierge::SwapVmRequest request;
  request.set_name(kArcVmName);
  request.set_owner_id(user_id_hash_);
  request.set_operation(operation);
  client->SwapVm(
      request,
      base::BindOnce(
          [](vm_tools::concierge::SwapOperation op, base::OnceClosure cb,
             absl::optional<vm_tools::concierge::SwapVmResponse> response) {
            if (!response->success()) {
              LOG(ERROR) << "Failed to send request: "
                         << vm_tools::concierge::SwapOperation_Name(op)
                         << ". Reason: " << response->failure_reason();
            } else {
              std::move(cb).Run();
            }
          },
          operation, std::move(success_callback)));
}

void ArcVmmManager::PostWithSwapDelay(base::OnceClosure callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, std::move(callback), swap_out_delay_);
}

// ArcVmmManager::AcceleratorTarget --------------------------------------------

class ArcVmmManager::AcceleratorTarget : public ui::AcceleratorTarget {
 public:
  explicit AcceleratorTarget(ArcVmmManager* manager)
      : manager_(manager),
        vmm_swap_enabled_(ui::VKEY_O, ash::kDebugModifier),
        vmm_swap_disabled_(ui::VKEY_P, ash::kDebugModifier) {
    ash::Shell::Get()->accelerator_controller()->Register(
        {vmm_swap_enabled_, vmm_swap_disabled_}, this);
  }
  AcceleratorTarget(const AcceleratorTarget&) = delete;
  AcceleratorTarget& operator=(const AcceleratorTarget&) = delete;
  ~AcceleratorTarget() override = default;

 private:
  // ui::AcceleratorTarget:
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override {
    if (accelerator == vmm_swap_enabled_) {
      manager_->SetSwapState(SwapState::ENABLE_WITH_SWAPOUT);
    } else if (accelerator == vmm_swap_disabled_) {
      manager_->SetSwapState(SwapState::DISABLE);
    } else {
      NOTREACHED();
      return false;
    }
    return true;
  }

  bool CanHandleAccelerators() const override { return true; }

  // The manager responsible for executing vmm commands.
  const raw_ptr<ArcVmmManager, ExperimentalAsh> manager_;

  // The accelerator to enable vmm swap for ARCVM.
  const ui::Accelerator vmm_swap_enabled_;

  // The accelerator to disable vmm swap for ARCVM.
  const ui::Accelerator vmm_swap_disabled_;
};

}  // namespace arc
