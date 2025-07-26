// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_INTEGRITY_METADATA_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_INTEGRITY_METADATA_H_

#include "services/network/public/mojom/integrity_algorithm.mojom-blink.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hasher.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class IntegrityMetadata;

using IntegrityAlgorithm = network::mojom::blink::IntegrityAlgorithm;
using IntegrityMetadataPair = std::pair<String, IntegrityAlgorithm>;

// Contains the result of SRI's "Parse Metadata" algorithm:
//
// https://wicg.github.io/signature-based-sri/#abstract-opdef-parse-metadata
struct IntegrityMetadataSet {
  IntegrityMetadataSet() {}
  bool empty() const { return hashes.empty() && signatures.empty(); }
  WTF::HashSet<IntegrityMetadataPair> hashes;
  WTF::HashSet<IntegrityMetadataPair> signatures;

  bool operator==(const IntegrityMetadataSet& other) const {
    return this->hashes == other.hashes && this->signatures == other.signatures;
  }
};

class PLATFORM_EXPORT IntegrityMetadata {
  STACK_ALLOCATED();

 public:
  IntegrityMetadata() = default;
  IntegrityMetadata(String digest, IntegrityAlgorithm);
  IntegrityMetadata(IntegrityMetadataPair);

  String Digest() const { return digest_; }
  void SetDigest(const String& digest) { digest_ = digest; }
  IntegrityAlgorithm Algorithm() const { return algorithm_; }
  void SetAlgorithm(IntegrityAlgorithm algorithm) { algorithm_ = algorithm; }

  IntegrityMetadataPair ToPair() const;

 private:
  String digest_;
  IntegrityAlgorithm algorithm_;
};

enum class ResourceIntegrityDisposition : uint8_t {
  kNotChecked = 0,
  kNetworkError,
  kFailedUnencodedDigest,
  kFailedIntegrityMetadata,
  kPassed
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_INTEGRITY_METADATA_H_
