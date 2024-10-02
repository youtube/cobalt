// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_package/signed_web_bundles/signed_web_bundle_integrity_block.h"

#include "base/strings/stringprintf.h"
#include "components/web_package/mojom/web_bundle_parser.mojom.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_signature_stack.h"

namespace web_package {

SignedWebBundleIntegrityBlock::~SignedWebBundleIntegrityBlock() = default;

base::expected<SignedWebBundleIntegrityBlock, std::string>
SignedWebBundleIntegrityBlock::Create(
    mojom::BundleIntegrityBlockPtr integrity_block) {
  if (integrity_block->size == 0) {
    return base::unexpected("Cannot create integrity block with a size of 0.");
  }

  std::vector<SignedWebBundleSignatureStackEntry> signature_stack_entries;
  for (const auto& raw_entry : integrity_block->signature_stack) {
    signature_stack_entries.emplace_back(
        raw_entry->complete_entry_cbor, raw_entry->attributes_cbor,
        raw_entry->public_key, raw_entry->signature);
  }

  auto signature_stack =
      SignedWebBundleSignatureStack::Create(signature_stack_entries);
  if (!signature_stack.has_value()) {
    return base::unexpected(
        base::StringPrintf("Cannot create an integrity block: %s",
                           signature_stack.error().c_str()));
  }

  return SignedWebBundleIntegrityBlock(integrity_block->size,
                                       std::move(*signature_stack));
}

SignedWebBundleIntegrityBlock::SignedWebBundleIntegrityBlock(
    const uint64_t size_in_bytes,
    SignedWebBundleSignatureStack&& signature_stack)
    : size_in_bytes_(size_in_bytes), signature_stack_(signature_stack) {
  CHECK_GT(size_in_bytes_, 0ul);
}

SignedWebBundleIntegrityBlock::SignedWebBundleIntegrityBlock(
    const SignedWebBundleIntegrityBlock&) = default;
SignedWebBundleIntegrityBlock& SignedWebBundleIntegrityBlock::operator=(
    const SignedWebBundleIntegrityBlock&) = default;

bool SignedWebBundleIntegrityBlock::operator==(
    const SignedWebBundleIntegrityBlock& other) const {
  return size_in_bytes_ == other.size_in_bytes_ &&
         signature_stack_ == other.signature_stack_;
}

bool SignedWebBundleIntegrityBlock::operator!=(
    const SignedWebBundleIntegrityBlock& other) const {
  return !operator==(other);
}

}  // namespace web_package
