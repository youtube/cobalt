// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_OSAUTH_PUBLIC_AUTH_SESSION_STORAGE_H_
#define CHROMEOS_ASH_COMPONENTS_OSAUTH_PUBLIC_AUTH_SESSION_STORAGE_H_

#include <memory>

#include "base/functional/callback.h"
#include "chromeos/ash/components/osauth/public/auth_parts.h"
#include "chromeos/ash/components/osauth/public/common_types.h"

namespace base {

class Location;

}  // namespace base

namespace ash {

class UserContext;

// Helper class that stores and manages lifetime of authenticated UserContext.
// Main usa cases for this class are the situations where authenticated
// operations do not happen immediately after authentication, but require some
// user input, e.g. setting up additional factors during user onboarding on a
// first run, or entering authentication-related section of
// `chrome://os-settings`.
//
// When context is added to storage, storage would return a token as a
// replacement, this token can be relatively safely be passed between components
// as it does not contain any sensitive information.
//
// UserContext can be borrowed to perform authenticated operations and should be
// returned to storage as soon as operation completes.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_OSAUTH) AuthSessionStorage {
 public:
  // TODO (b/271249180): Define an observer for notifications about token
  // expiration/borrowing.

  // Convenience method.
  static inline AuthSessionStorage* Get() {
    return AuthParts::Get()->GetAuthSessionStorage();
  }

  virtual ~AuthSessionStorage() = default;

  // Gets the ownership of (authenticated) context, and returns an
  // authentication proof token that can be safely passed around between
  // components. Storage manages the lifetime of the context (and
  // underlying cyrptohome authsession).
  virtual AuthProofToken Store(std::unique_ptr<UserContext> context) = 0;
  // Checks if given token is valid (exists and has not expired).
  virtual bool IsValid(const AuthProofToken& token) = 0;

  // Borrows UserContext to perform some authenticated operation. Borrowing
  // a context does not make it invalid.
  // Borrow is guaranteed to return non-null Context, and it would crash if
  // context is already borrowed.
  // TODO (b/271249180): There will be a way to determine if context can
  // be borrowed.
  virtual std::unique_ptr<UserContext> Borrow(const base::Location& location,
                                              const AuthProofToken& token) = 0;
  // Return context back to Storage.
  virtual void Return(const AuthProofToken& token,
                      std::unique_ptr<UserContext> context) = 0;

  // Cleans up UserContext and all associated resources (like cryptohome
  // AuthSession) once authentication is no longer needed.
  // In case when context is borrowed at the time of this call,
  // it would be properly invalidated once it is returned.
  virtual void Invalidate(const AuthProofToken& token,
                          base::OnceClosure on_invalidated) = 0;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_OSAUTH_PUBLIC_AUTH_SESSION_STORAGE_H_
