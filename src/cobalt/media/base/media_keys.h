// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COBALT_MEDIA_BASE_MEDIA_KEYS_H_
#define COBALT_MEDIA_BASE_MEDIA_KEYS_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_vector.h"
#include "cobalt/media/base/eme_constants.h"
#include "cobalt/media/base/media_export.h"
#include "googleurl/src/gurl.h"

namespace base {
class Time;
}

namespace media {

class CdmContext;
struct CdmKeyInformation;
struct MediaKeysTraits;

template <typename... T>
class CdmPromiseTemplate;

typedef CdmPromiseTemplate<std::string> NewSessionCdmPromise;
typedef CdmPromiseTemplate<> SimpleCdmPromise;
typedef ScopedVector<CdmKeyInformation> CdmKeysInfo;

// An interface that represents the Content Decryption Module (CDM) in the
// Encrypted Media Extensions (EME) spec in Chromium.
// See http://w3c.github.io/encrypted-media/#cdm
//
// * Ownership
//
// This class is ref-counted. However, a ref-count should only be held by:
// - The owner of the CDM. This is usually some class in the EME stack, e.g.
//   CdmSessionAdapter in the render process, or MojoCdmService in a non-render
//   process.
// - The media player that uses the CDM, to prevent the CDM from being
//   destructed while still being used by the media player.
//
// When binding class methods into callbacks, prefer WeakPtr to using |this|
// directly to avoid having a ref-count held by the callback.
//
// * Thread Safety
//
// Most CDM operations happen on one thread. However, it is not uncommon that
// the media player lives on a different thread and may call into the CDM from
// that thread. For example, if the CDM supports a Decryptor interface, the
// Decryptor methods could be called on a different thread. The CDM
// implementation should make sure it's thread safe for these situations.
//
// TODO(xhwang): Rename MediaKeys to ContentDecryptionModule. See
// http://crbug.com/309237

class MEDIA_EXPORT MediaKeys
    : public base::RefCountedThreadSafe<MediaKeys, MediaKeysTraits> {
 public:
  // TODO(xhwang): Remove after prefixed EME support is removed. See
  // http://crbug.com/249976
  // Must be a superset of cdm::MediaKeyException.
  enum Exception {
    NOT_SUPPORTED_ERROR,
    INVALID_STATE_ERROR,
    INVALID_ACCESS_ERROR,
    QUOTA_EXCEEDED_ERROR,
    UNKNOWN_ERROR,
    CLIENT_ERROR,
    OUTPUT_ERROR,
    EXCEPTION_MAX = OUTPUT_ERROR
  };

  // Type of license required when creating/loading a session.
  // Must be consistent with the values specified in the spec:
  // https://w3c.github.io/encrypted-media/#idl-def-MediaKeySessionType
  enum SessionType {
    TEMPORARY_SESSION,
    PERSISTENT_LICENSE_SESSION,
    PERSISTENT_RELEASE_MESSAGE_SESSION,
    SESSION_TYPE_MAX = PERSISTENT_RELEASE_MESSAGE_SESSION
  };

  // Type of message being sent to the application.
  // Must be consistent with the values specified in the spec:
  // https://w3c.github.io/encrypted-media/#idl-def-MediaKeyMessageType
  enum MessageType {
    LICENSE_REQUEST,
    LICENSE_RENEWAL,
    LICENSE_RELEASE,
    MESSAGE_TYPE_MAX = LICENSE_RELEASE
  };

  // Provides a server certificate to be used to encrypt messages to the
  // license server.
  virtual void SetServerCertificate(
      const std::vector<uint8_t>& certificate,
      std::unique_ptr<SimpleCdmPromise> promise) = 0;

  // Creates a session with |session_type|. Then generates a request with the
  // |init_data_type| and |init_data|.
  // Note:
  // 1. The session ID will be provided when the |promise| is resolved.
  // 2. The generated request should be returned through a SessionMessageCB,
  //    which must be AFTER the |promise| is resolved. Otherwise, the session ID
  //    in the callback will not be recognized.
  // 3. UpdateSession(), CloseSession() and RemoveSession() should only be
  //    called after the |promise| is resolved.
  virtual void CreateSessionAndGenerateRequest(
      SessionType session_type, EmeInitDataType init_data_type,
      const std::vector<uint8_t>& init_data,
      std::unique_ptr<NewSessionCdmPromise> promise) = 0;

  // Loads a session with the |session_id| provided.
  // Note: UpdateSession(), CloseSession() and RemoveSession() should only be
  //       called after the |promise| is resolved.
  virtual void LoadSession(SessionType session_type,
                           const std::string& session_id,
                           std::unique_ptr<NewSessionCdmPromise> promise) = 0;

  // Updates a session specified by |session_id| with |response|.
  virtual void UpdateSession(const std::string& session_id,
                             const std::vector<uint8_t>& response,
                             std::unique_ptr<SimpleCdmPromise> promise) = 0;

  // Closes the session specified by |session_id|. The CDM should resolve or
  // reject the |promise| when the call has been processed. This may be before
  // the session is closed. Once the session is closed, a SessionClosedCB must
  // also be called.
  virtual void CloseSession(const std::string& session_id,
                            std::unique_ptr<SimpleCdmPromise> promise) = 0;

  // Removes stored session data associated with the session specified by
  // |session_id|.
  virtual void RemoveSession(const std::string& session_id,
                             std::unique_ptr<SimpleCdmPromise> promise) = 0;

  // Returns the CdmContext associated with |this|. The returned CdmContext is
  // owned by |this| and the caller needs to make sure it is not used after
  // |this| is destructed.
  // Returns null if CdmContext is not supported. Instead the media player may
  // use the CDM via some platform specific method.
  // By default this method returns null.
  // TODO(xhwang): Convert all SetCdm() implementations to use CdmContext so
  // that this function should never return NULL.
  virtual CdmContext* GetCdmContext();

  // Deletes |this| on the correct thread. By default |this| is deleted
  // immediately. Override this method if |this| needs to be deleted on a
  // specific thread.
  virtual void DeleteOnCorrectThread() const;

 protected:
  friend class base::RefCountedThreadSafe<MediaKeys, MediaKeysTraits>;

  MediaKeys();
  virtual ~MediaKeys();

 private:
  DISALLOW_COPY_AND_ASSIGN(MediaKeys);
};

struct MEDIA_EXPORT MediaKeysTraits {
  // Destroys |media_keys| on the correct thread.
  static void Destruct(const MediaKeys* media_keys);
};

// CDM session event callbacks.

// Called when the CDM needs to queue a message event to the session object.
// See http://w3c.github.io/encrypted-media/#dom-evt-message
typedef base::Callback<void(
    const std::string& session_id, MediaKeys::MessageType message_type,
    const std::vector<uint8_t>& message)> SessionMessageCB;

// Called when the session specified by |session_id| is closed. Note that the
// CDM may close a session at any point, such as in response to a CloseSession()
// call, when the session is no longer needed, or when system resources are
// lost. See http://w3c.github.io/encrypted-media/#session-close
typedef base::Callback<void(const std::string& session_id)> SessionClosedCB;

// Called when there has been a change in the keys in the session or their
// status. See http://w3c.github.io/encrypted-media/#dom-evt-keystatuseschange
typedef base::Callback<void(const std::string& session_id,
                            bool has_additional_usable_key,
                            CdmKeysInfo keys_info)> SessionKeysChangeCB;

// Called when the CDM changes the expiration time of a session.
// See http://w3c.github.io/encrypted-media/#update-expiration
typedef base::Callback<void(const std::string& session_id,
                            const base::Time& new_expiry_time)>
    SessionExpirationUpdateCB;

}  // namespace media

#endif  // COBALT_MEDIA_BASE_MEDIA_KEYS_H_
