// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COBALT_MEDIA_BASE_CDM_CONTEXT_H_
#define COBALT_MEDIA_BASE_CDM_CONTEXT_H_

#include "base/basictypes.h"
#include "base/callback.h"
#include "cobalt/media/base/media_export.h"

namespace media {

class Decryptor;

// An interface representing the context that a media player needs from a
// content decryption module (CDM) to decrypt (and decode) encrypted buffers.
// This is used to pass the CDM to the media player (e.g. SetCdm()).
class MEDIA_EXPORT CdmContext {
 public:
  // Indicates an invalid CDM ID. See GetCdmId() for details.
  static const int kInvalidCdmId;

  virtual ~CdmContext();

  // Gets the Decryptor object associated with the CDM. Returns NULL if the
  // CDM does not support a Decryptor (i.e. platform-based CDMs where decryption
  // occurs implicitly along with decoding). The returned object is only
  // guaranteed to be valid during the CDM's lifetime.
  virtual Decryptor* GetDecryptor() = 0;

  // Returns an ID that can be used to find a remote CDM, in which case this CDM
  // serves as a proxy to the remote one. Returns kInvalidCdmId when remote CDM
  // is not supported (e.g. this CDM is a local CDM).
  virtual int GetCdmId() const = 0;

 protected:
  CdmContext();

 private:
  DISALLOW_COPY_AND_ASSIGN(CdmContext);
};

// Callback to notify that the CdmContext has been completely attached to
// the media pipeline. Parameter indicates whether the operation succeeded.
typedef base::Callback<void(bool)> CdmAttachedCB;

// A dummy implementation of CdmAttachedCB.
MEDIA_EXPORT void IgnoreCdmAttached(bool success);

}  // namespace media

#endif  // COBALT_MEDIA_BASE_CDM_CONTEXT_H_
