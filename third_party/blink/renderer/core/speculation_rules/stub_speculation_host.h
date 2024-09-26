// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/callback.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "third_party/blink/public/mojom/speculation_rules/speculation_rules.mojom-blink.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SPECULATION_RULES_STUB_SPECULATION_HOST_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SPECULATION_RULES_STUB_SPECULATION_HOST_H_

namespace blink {

class StubSpeculationHost : public mojom::blink::SpeculationHost {
 public:
  using Candidates = Vector<mojom::blink::SpeculationCandidatePtr>;

  const Candidates& candidates() const { return candidates_; }
  void SetDoneClosure(base::OnceClosure done) {
    done_closure_ = std::move(done);
  }

  // It's usually easier to use `SetDoneClosure` and then check `candidates`,
  // but this is available in case you want to inspect the exact calls that are
  // made. For example, this might matter if multiple calls to this are made,
  // and one might race with the run loop exit.
  void SetCandidatesUpdatedCallback(
      base::RepeatingCallback<void(const Candidates&)> callback) {
    candidates_updated_callback_ = callback;
  }

  void BindUnsafe(mojo::ScopedMessagePipeHandle handle);
  void Bind(mojo::PendingReceiver<SpeculationHost> receiver);

  // mojom::blink::SpeculationHost.
  void UpdateSpeculationCandidates(
      const base::UnguessableToken& devtools_navigation_token,
      Candidates candidates) override;

  // mojom::blink::SpeculationHost.
  void EnableNoVarySearchSupport() override;

  void OnConnectionLost();

  bool is_bound() const { return receiver_.is_bound(); }

  bool sent_no_vary_search_support_to_browser() const {
    return sent_no_vary_search_support_to_browser_;
  }

  absl::optional<base::UnguessableToken> devtools_navigation_token() const {
    return devtools_navigation_token_;
  }

 private:
  mojo::Receiver<SpeculationHost> receiver_{this};
  Vector<mojom::blink::SpeculationCandidatePtr> candidates_;
  bool sent_no_vary_search_support_to_browser_ = false;
  base::OnceClosure done_closure_;
  base::RepeatingCallback<void(const Candidates&)> candidates_updated_callback_;
  // A non-nul devtools_navigation_token_ is expected to be always consistent.
  absl::optional<base::UnguessableToken> devtools_navigation_token_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_SPECULATION_RULES_STUB_SPECULATION_HOST_H_
