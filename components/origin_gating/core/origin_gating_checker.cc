// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/origin_gating/core/origin_gating_checker.h"

#include "base/functional/callback.h"
#include "base/task/sequenced_task_runner.h"
#include "components/origin_gating/core/origin_gating_cache.h"
#include "components/origin_gating/core/types.h"

namespace origin_gating {

namespace {

void ResolveGatingDecision(GatingDecisionCallback callback,
                           std::unique_ptr<GatingDecisionContext> context,
                           GatingDecision decision) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(context),
                                std::move(decision)));
}

}  // namespace

OriginGatingChecker::OriginGatingChecker(Delegate& delegate,
                                         bool use_site_not_origin)
    : delegate_(delegate), cache_(use_site_not_origin) {}

OriginGatingChecker::~OriginGatingChecker() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void OriginGatingChecker::ComputeGatingDecision(
    std::unique_ptr<GatingDecisionContext> context,
    const GURL& source,
    const GURL& destination,
    GatingDecisionCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  url::Origin destination_origin = url::Origin::Create(destination);

  if (cache_.IsNavigationConfirmedByUser(destination_origin)) {
    ResolveGatingDecision(std::move(callback), std::move(context),
                          GatingDecision{
                              .is_allowed = true,
                              .source = DecisionSource::kCache,
                          });
    return;
  }

  GatingDecisionContext* raw_context = context.get();
  delegate_->DoesOriginRequireUserConfirmation(
      raw_context, source, destination,
      base::BindOnce(&OriginGatingChecker::OnUserConfirmationRequiredAnswer,
                     weak_ptr_factory_.GetWeakPtr(), std::move(context), source,
                     destination, std::move(callback)));
}

void OriginGatingChecker::OnUserConfirmationRequiredAnswer(
    std::unique_ptr<GatingDecisionContext> context,
    const GURL& source,
    const GURL& destination,
    GatingDecisionCallback callback,
    bool requires_user_confirmation) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!requires_user_confirmation &&
      cache_.IsNavigationAllowed(url::Origin::Create(source),
                                 url::Origin::Create(destination))) {
    ResolveGatingDecision(std::move(callback), std::move(context),
                          GatingDecision{
                              .is_allowed = true,
                              .source = DecisionSource::kCache,
                          });
    return;
  }

  GatingDecisionContext* raw_context = context.get();
  delegate_->OnNoVerdict(
      raw_context, source, destination, requires_user_confirmation,
      base::BindOnce(&OriginGatingChecker::OnNoVerdictAnswer,
                     weak_ptr_factory_.GetWeakPtr(), std::move(context),
                     destination, std::move(callback)));
}

void OriginGatingChecker::OnNoVerdictAnswer(
    std::unique_ptr<GatingDecisionContext> context,
    const GURL& destination,
    GatingDecisionCallback callback,
    Delegate::NoVerdictResult result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (result.is_allowed) {
    AllowNavigationTo(url::Origin::Create(destination), result.did_prompt_user);
  }

  ResolveGatingDecision(std::move(callback), std::move(context),
                        GatingDecision{
                            .is_allowed = result.is_allowed,
                            .source = DecisionSource::kNoVerdict,
                        });
}

}  // namespace origin_gating
