// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PLATFORM_KEYS_CHAPS_SLOT_SESSION_H_
#define CHROME_BROWSER_CHROMEOS_PLATFORM_KEYS_CHAPS_SLOT_SESSION_H_

#include <pkcs11t.h>

#include <memory>

namespace chromeos {
namespace platform_keys {

// A PKCS#11 session for a slot provided by the chaps daemon.
// This class provides a subset of PKCS#11 operations relevant for Chrome OS.
// When a ChapsSlotSession is destructed, the PKCS#11 session is closed.
// Operations on a ChapsSlotSession are blocking and expensive, so they may only
// be performed on a worker thread.
class ChapsSlotSession {
 public:
  virtual ~ChapsSlotSession() = default;

  // Close and re-open this session.
  virtual bool ReopenSession() = 0;

  // Calls C_GenerateKeyPair.
  // PKCS #11 v2.20 section 11.14 page 176.
  virtual CK_RV GenerateKeyPair(CK_MECHANISM_PTR pMechanism,
                                CK_ATTRIBUTE_PTR pPublicKeyTemplate,
                                CK_ULONG ulPublicKeyAttributeCount,
                                CK_ATTRIBUTE_PTR pPrivateKeyTemplate,
                                CK_ULONG ulPrivateKeyAttributeCount,
                                CK_OBJECT_HANDLE_PTR phPublicKey,
                                CK_OBJECT_HANDLE_PTR phPrivateKey) = 0;

  // Calls C_GetAttributeValue.
  // PKCS #11 v2.20 section 11.7 page 133.
  virtual CK_RV GetAttributeValue(CK_OBJECT_HANDLE hObject,
                                  CK_ATTRIBUTE_PTR pTemplate,
                                  CK_ULONG ulCount) = 0;

  // Calls C_SetAttributeValue.
  // PKCS #11 v2.20 section 11.7 page 135.
  virtual CK_RV SetAttributeValue(CK_OBJECT_HANDLE hObject,
                                  CK_ATTRIBUTE_PTR pTemplate,
                                  CK_ULONG ulCount) = 0;
};

// Creates ChapsSlotSession instances. The factory is used to replace
// ChapsSlotSession with fakes in tests.
class ChapsSlotSessionFactory {
 public:
  virtual ~ChapsSlotSessionFactory() = default;

  // Returns a ChapsSlotSession for the slot identified by slot ID |slot_id|.
  // If loading libchaps.so, resolving symbols, or opening the session fails,
  // returns nullptr.
  virtual std::unique_ptr<ChapsSlotSession> CreateChapsSlotSession(
      CK_SLOT_ID slot_id) = 0;
};

// This is the default implementation of the ChapsSlotSessionFactory.
// Creates ChapsSlotSession instances which call functions in libchaps.so.
class ChapsSlotSessionFactoryImpl : public ChapsSlotSessionFactory {
 public:
  ChapsSlotSessionFactoryImpl() = default;
  ~ChapsSlotSessionFactoryImpl() override = default;

  std::unique_ptr<ChapsSlotSession> CreateChapsSlotSession(
      CK_SLOT_ID slot_id) override;
};

}  // namespace platform_keys
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PLATFORM_KEYS_CHAPS_SLOT_SESSION_H_
