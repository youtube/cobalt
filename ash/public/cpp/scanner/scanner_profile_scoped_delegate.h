// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_SCANNER_SCANNER_PROFILE_SCOPED_DELEGATE_H_
#define ASH_PUBLIC_CPP_SCANNER_SCANNER_PROFILE_SCOPED_DELEGATE_H_

#include "ash/public/cpp/ash_public_export.h"
#include "ash/public/cpp/scanner/scanner_system_state.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "components/manta/proto/scanner.pb.h"
#include "components/manta/scanner_provider.h"

namespace drive {
class DriveServiceInterface;
}

namespace google_apis {
class RequestSender;
}

namespace ash {

// Provides access to the browser. The access provided is scoped to a single
// profile instance and will not be shared between profiles.
class ASH_PUBLIC_EXPORT ScannerProfileScopedDelegate {
 public:
  virtual ~ScannerProfileScopedDelegate() = default;

  // Returns the current state of the system. For example, is the feature
  // disabled? If so why was it disabled.
  virtual ScannerSystemState GetSystemState() const = 0;

  // Fetches Scanner actions that are available to the user based on the
  // contents of `jpeg_bytes`. The actions response is returned via `callback`.
  virtual void FetchActionsForImage(
      scoped_refptr<base::RefCountedMemory> jpeg_bytes,
      manta::ScannerProvider::ScannerProtoResponseCallback callback) = 0;

  // Fetches the details of the Scanner action selected by the user based on the
  // contents of `jpeg_bytes`. The actions response is returned via `callback`.
  virtual void FetchActionDetailsForImage(
      scoped_refptr<base::RefCountedMemory> jpeg_bytes,
      manta::proto::ScannerAction selected_action,
      manta::ScannerProvider::ScannerProtoResponseCallback callback) = 0;

  // Returns a reference to a `drive::DriveServiceInterface` to upload files.
  virtual drive::DriveServiceInterface* GetDriveService() = 0;

  // Returns a reference to a `google_apis::RequestSender` to send Google API
  // requests.
  virtual google_apis::RequestSender* GetGoogleApisRequestSender() = 0;

  // Returns true if the currently logged in user account is a Googler account.
  virtual bool IsGoogler() = 0;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_SCANNER_SCANNER_PROFILE_SCOPED_DELEGATE_H_
