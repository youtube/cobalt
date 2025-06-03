// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/nigori/cross_user_sharing_keys.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "components/sync/protocol/nigori_local_data.pb.h"

namespace syncer {
namespace {

sync_pb::CrossUserSharingPrivateKey KeyPairToPrivateKeyProto(
    const uint32_t version,
    const CrossUserSharingPublicPrivateKeyPair& key_pair) {
  const auto raw_private_key = key_pair.GetRawPrivateKey();
  sync_pb::CrossUserSharingPrivateKey output;
  output.set_version(version);
  output.set_x25519_private_key(
      std::string(std::begin(raw_private_key), std::end(raw_private_key)));
  return output;
}

CrossUserSharingPublicPrivateKeyPair CloneKeyPair(
    const CrossUserSharingPublicPrivateKeyPair& key_pair) {
  absl::optional<CrossUserSharingPublicPrivateKeyPair> clone =
      CrossUserSharingPublicPrivateKeyPair::CreateByImport(
          key_pair.GetRawPrivateKey());
  CHECK(clone.has_value());
  return std::move(clone.value());
}

}  // namespace

// static
CrossUserSharingKeys CrossUserSharingKeys::CreateEmpty() {
  return CrossUserSharingKeys();
}

// static
CrossUserSharingKeys CrossUserSharingKeys::CreateFromProto(
    const sync_pb::CrossUserSharingKeys& proto) {
  CrossUserSharingKeys output;
  for (const sync_pb::CrossUserSharingPrivateKey& key : proto.private_key()) {
    if (!output.AddKeyPairFromProto(key)) {
      DLOG(WARNING) << "Could not add PrivateKey protocol buffer message.";
    }
  }

  return output;
}

CrossUserSharingKeys::CrossUserSharingKeys() = default;

CrossUserSharingKeys::CrossUserSharingKeys(CrossUserSharingKeys&& other) =
    default;

CrossUserSharingKeys::~CrossUserSharingKeys() = default;

sync_pb::CrossUserSharingKeys CrossUserSharingKeys::ToProto() const {
  sync_pb::CrossUserSharingKeys output;
  for (const auto& [key_version, key_pair] : key_pairs_map_) {
    *output.add_private_key() = KeyPairToPrivateKeyProto(key_version, key_pair);
  }

  return output;
}

CrossUserSharingKeys CrossUserSharingKeys::Clone() const {
  CrossUserSharingKeys copy;
  copy.AddAllUnknownKeysFrom(*this);
  return copy;
}

size_t CrossUserSharingKeys::size() const {
  return key_pairs_map_.size();
}

bool CrossUserSharingKeys::HasKeyPair(uint32_t key_pair_version) const {
  return key_pairs_map_.contains(key_pair_version);
}

void CrossUserSharingKeys::AddAllUnknownKeysFrom(
    const CrossUserSharingKeys& other) {
  for (const auto& [public_key, key_pair] : other.key_pairs_map_) {
    key_pairs_map_.emplace(public_key, CloneKeyPair(key_pair));
  }
}

bool CrossUserSharingKeys::AddKeyPairFromProto(
    const sync_pb::CrossUserSharingPrivateKey& key) {
  std::vector<uint8_t> private_key(key.x25519_private_key().begin(),
                                   key.x25519_private_key().end());
  absl::optional<CrossUserSharingPublicPrivateKeyPair> key_pair =
      CrossUserSharingPublicPrivateKeyPair::CreateByImport(private_key);

  if (!key_pair.has_value()) {
    return false;
  }

  AddKeyPair(std::move(key_pair.value()), key.version());
  return true;
}

void CrossUserSharingKeys::AddKeyPair(
    CrossUserSharingPublicPrivateKeyPair key_pair,
    uint32_t version) {
  key_pairs_map_.emplace(version, std::move(key_pair));
}

const CrossUserSharingPublicPrivateKeyPair& CrossUserSharingKeys::GetKeyPair(
    uint32_t version) const {
  CHECK(HasKeyPair(version));
  return key_pairs_map_.at(version);
}

}  // namespace syncer
