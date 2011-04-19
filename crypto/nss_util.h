// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTO_NSS_UTIL_H_
#define CRYPTO_NSS_UTIL_H_
#pragma once

#include <string>
#include "base/basictypes.h"

#if defined(USE_NSS)
class FilePath;
#endif  // defined(USE_NSS)

namespace base {
class Lock;
class Time;
}  // namespace base

// This file specifically doesn't depend on any NSS or NSPR headers because it
// is included by various (non-crypto) parts of chrome to call the
// initialization functions.
namespace crypto {

#if defined(USE_NSS)
// EarlySetupForNSSInit performs lightweight setup which must occur before the
// process goes multithreaded. This does not initialise NSS. For test, see
// EnsureNSSInit.
void EarlySetupForNSSInit();
#endif

// Initialize NRPR if it isn't already initialized.  This function is
// thread-safe, and NSPR will only ever be initialized once.
void EnsureNSPRInit();

// Initialize NSS if it isn't already initialized.  This must be called before
// any other NSS functions.  This function is thread-safe, and NSS will only
// ever be initialized once.
void EnsureNSSInit();

// Call this before calling EnsureNSSInit() will force NSS to initialize
// without a persistent DB.  This is used for the special case where access of
// persistent DB is prohibited.
//
// TODO(hclam): Isolate loading default root certs.
//
// NSS will be initialized without loading any user security modules, including
// the built-in root certificates module. User security modules need to be
// loaded manually after NSS initialization.
//
// If EnsureNSSInit() is called before then this function has no effect.
//
// Calling this method only has effect on Linux.
//
// WARNING: Use this with caution.
void ForceNSSNoDBInit();

// This methods is used to disable checks in NSS when used in a forked process.
// NSS checks whether it is running a forked process to avoid problems when
// using user security modules in a forked process.  However if we are sure
// there are no modules loaded before the process is forked then there is no
// harm disabling the check.
//
// This method must be called before EnsureNSSInit() to take effect.
//
// WARNING: Use this with caution.
void DisableNSSForkCheck();

// Load NSS library files. This function has no effect on Mac and Windows.
// This loads the necessary NSS library files so that NSS can be initialized
// after loading additional library files is disallowed, for example when the
// sandbox is active.
//
// Note that this does not load libnssckbi.so which contains the root
// certificates.
void LoadNSSLibraries();

// Check if the current NSS version is greater than or equals to |version|.
// A sample version string is "3.12.3".
bool CheckNSSVersion(const char* version);

#if defined(OS_CHROMEOS)
// Open the r/w nssdb that's stored inside the user's encrypted home
// directory.  This is the default slot returned by
// GetPublicNSSKeySlot().
void OpenPersistentNSSDB();

// A delegate class that we can use it to access the cros API for
// communication with cryptohomed and the TPM.
class TPMTokenInfoDelegate {
 public:
  TPMTokenInfoDelegate();
  virtual ~TPMTokenInfoDelegate();
  virtual bool IsTokenReady() const = 0;
  virtual void GetTokenInfo(std::string* token_name,
                            std::string* user_pin) const = 0;
};

// Indicates that NSS should load the opencryptoki library so that we
// can access the TPM through NSS.  Once this is called,
// GetPrivateNSSKeySlot() will return the TPM slot if one was found.
// Takes ownership of the passed-in delegate object so it can access
// the cros library to talk to cryptohomed.
void EnableTPMTokenForNSS(TPMTokenInfoDelegate* delegate);

// Get name and user PIN for the built-in TPM token on ChromeOS.
// Either one can safely be NULL.  Should only be called after
// EnableTPMTokenForNSS has been called with a non-null delegate.
void GetTPMTokenInfo(std::string* token_name, std::string* user_pin);

// Returns true if the TPM is owned and PKCS#11 initialized with the
// user and security officer PINs, and has been enabled in NSS by
// calling EnableTPMForNSS, and opencryptoki has been successfully
// loaded into NSS.
bool IsTPMTokenReady();
#endif

// Convert a NSS PRTime value into a base::Time object.
// We use a int64 instead of PRTime here to avoid depending on NSPR headers.
base::Time PRTimeToBaseTime(int64 prtime);

#if defined(USE_NSS)
// Exposed for unittests only.  |path| should be an existing directory under
// which the DB files will be placed.  |description| is a user-visible name for
// the DB, as a utf8 string, which will be truncated at 32 bytes.
bool OpenTestNSSDB(const FilePath& path, const char* description);
void CloseTestNSSDB();

// NSS has a bug which can cause a deadlock or stall in some cases when writing
// to the certDB and keyDB. It also has a bug which causes concurrent key pair
// generations to scribble over each other. To work around this, we synchronize
// writes to the NSS databases with a global lock. The lock is hidden beneath a
// function for easy disabling when the bug is fixed. Callers should allow for
// it to return NULL in the future.
//
// See https://bugzilla.mozilla.org/show_bug.cgi?id=564011
base::Lock* GetNSSWriteLock();

// A helper class that acquires the NSS write Lock while the AutoNSSWriteLock
// is in scope.
class AutoNSSWriteLock {
 public:
  AutoNSSWriteLock();
  ~AutoNSSWriteLock();
 private:
  base::Lock *lock_;
  DISALLOW_COPY_AND_ASSIGN(AutoNSSWriteLock);
};

#endif  // defined(USE_NSS)

}  // namespace crypto

#endif  // CRYPTO_NSS_UTIL_H_
