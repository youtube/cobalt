// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_NIGORI_CRYPTOGRAPHER_IMPL_H_
#define COMPONENTS_SYNC_NIGORI_CRYPTOGRAPHER_IMPL_H_

#include <memory>
#include <string>

#include "components/sync/engine/nigori/cryptographer.h"
#include "components/sync/engine/nigori/key_derivation_params.h"
#include "components/sync/engine/nigori/nigori.h"
#include "components/sync/nigori/nigori_key_bag.h"
#include "components/sync/protocol/nigori_local_data.pb.h"

namespace sync_pb {
class CryptographerData;
}  // namespace sync_pb

namespace syncer {

// This class manages the Nigori objects used to encrypt and decrypt sensitive
// sync data (eg. passwords). Each Nigori object knows how to handle data
// protected with a particular encryption key.
//
// The implementation consists of a keybag representing all known keys and
// optionally the notion of a default encryption key which, if it exists, is
// present in the keybag.
class CryptographerImpl : public Cryptographer {
 public:
  // Factory methods.
  static std::unique_ptr<CryptographerImpl> CreateEmpty();
  static std::unique_ptr<CryptographerImpl> FromSingleKeyForTesting(
      const std::string& passphrase,
      const KeyDerivationParams& derivation_params =
          KeyDerivationParams::CreateForPbkdf2());
  // Returns null in case of error (e.g. default key not present in keybag).
  static std::unique_ptr<CryptographerImpl> FromProto(
      const sync_pb::CryptographerData& proto);

  CryptographerImpl& operator=(const CryptographerImpl&) = delete;

  ~CryptographerImpl() override;

  // Serialization.
  sync_pb::CryptographerData ToProto() const;

  // Creates and registers a new key after deriving Nigori keys. Returns the
  // name of the key, or an empty string in case of error. Note that emplacing
  // an already-known key is not considered an error (just a no-op).
  //
  // Does NOT set or change the default encryption key.
  std::string EmplaceKey(const std::string& passphrase,
                         const KeyDerivationParams& derivation_params);

  // Adds all keys from |key_bag| that weren't previously known.
  //
  // Does NOT set or change the default encryption key.
  void EmplaceKeysFrom(const NigoriKeyBag& key_bag);

  // Sets or changes the default encryption key, which causes CanEncrypt() to
  // return true. |key_name| must not be empty and must represent a known key.
  void SelectDefaultEncryptionKey(const std::string& key_name);

  // Adds all keys in |other| that weren't previously known, and selects the
  // same default key. |other| must have selected a default key.
  void EmplaceKeysAndSelectDefaultKeyFrom(const CryptographerImpl& other);

  // Clears the default encryption key, which causes CanEncrypt() to return
  // false.
  void ClearDefaultEncryptionKey();

  // Reverts the cryptographer to an empty one, i.e. what would be returned by
  // CreateEmpty(). The default key is also cleared.
  // The set of known encryption keys shouldn't decrease in general, since this
  // may lead to data becoming undecryptable. This method can be called a) if
  // sync is disabled, or b) if sync finds an error that can be solved by
  // resetting the encryption state.
  void ClearAllKeys();

  // Determines whether |key_name| represents a known key.
  bool HasKey(const std::string& key_name) const;

  // Returns a proto representation of the default encryption key. |*this| must
  // have a default encryption key set, as reflected by CanEncrypt().
  sync_pb::NigoriKey ExportDefaultKey() const;

  std::unique_ptr<CryptographerImpl> Clone() const;

  size_t KeyBagSizeForTesting() const;

  // Cryptographer overrides.
  bool CanEncrypt() const override;
  bool CanDecrypt(const sync_pb::EncryptedData& encrypted) const override;
  std::string GetDefaultEncryptionKeyName() const override;
  bool EncryptString(const std::string& decrypted,
                     sync_pb::EncryptedData* encrypted) const override;
  bool DecryptToString(const sync_pb::EncryptedData& encrypted,
                       std::string* decrypted) const override;

 private:
  CryptographerImpl(NigoriKeyBag key_bag,
                    std::string default_encryption_key_name);

  // The actual keys we know about.
  NigoriKeyBag key_bag_;

  // The key name associated with the default encryption key. If non-empty, it
  // must correspond to a key within |key_bag_|. May be empty even if |key_bag_|
  // is not.
  std::string default_encryption_key_name_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_NIGORI_CRYPTOGRAPHER_IMPL_H_
