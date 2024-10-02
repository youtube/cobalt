// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SHARED_STORAGE_SHARED_STORAGE_DOCUMENT_SERVICE_IMPL_H_
#define CONTENT_BROWSER_SHARED_STORAGE_SHARED_STORAGE_DOCUMENT_SERVICE_IMPL_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "content/common/content_export.h"
#include "content/public/browser/document_user_data.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/shared_storage/shared_storage.mojom.h"
#include "url/origin.h"

namespace storage {
class SharedStorageManager;
}

namespace content {

class RenderFrameHost;
class SharedStorageWorkletHost;
class SharedStorageWorkletHostManager;

extern CONTENT_EXPORT const char kSharedStorageDisabledMessage[];
extern CONTENT_EXPORT const char kSharedStorageSelectURLDisabledMessage[];
extern CONTENT_EXPORT const char kSharedStorageAddModuleDisabledMessage[];
extern CONTENT_EXPORT const char kSharedStorageSelectURLLimitReachedMessage[];
extern CONTENT_EXPORT const char kSharedStorageWorkletExpiredMessage[];

// Handle renderer-initiated shared storage access and worklet operations. The
// worklet operations (i.e. `addModule()`, `selectURL()`, `run()`) will be
// dispatched to the `SharedStorageWorkletHost` to be handled.
class CONTENT_EXPORT SharedStorageDocumentServiceImpl final
    : public DocumentUserData<SharedStorageDocumentServiceImpl>,
      public blink::mojom::SharedStorageDocumentService {
 public:
  // If true, allows operations to bypass the permission check in
  // `IsSharedStorageAllowed()` for testing, in order to simulate the situation
  // where permission is allowed at the stage where `run()` is called but
  // becomes disallowed when subsequent operations are called from inside the
  // worklet.
  static bool& GetBypassIsSharedStorageAllowedForTesting();

  ~SharedStorageDocumentServiceImpl() final;

  const url::Origin& main_frame_origin() const { return main_frame_origin_; }

  std::string main_frame_id() const { return main_frame_id_; }

  void Bind(mojo::PendingAssociatedReceiver<
            blink::mojom::SharedStorageDocumentService> receiver);

  // blink::mojom::SharedStorageDocumentService.
  void AddModuleOnWorklet(const GURL& script_source_url,
                          AddModuleOnWorkletCallback callback) override;
  void RunOperationOnWorklet(const std::string& name,
                             const std::vector<uint8_t>& serialized_data,
                             bool keep_alive_after_operation,
                             const absl::optional<std::string>& context_id,
                             RunOperationOnWorkletCallback callback) override;
  void RunURLSelectionOperationOnWorklet(
      const std::string& name,
      std::vector<blink::mojom::SharedStorageUrlWithMetadataPtr>
          urls_with_metadata,
      const std::vector<uint8_t>& serialized_data,
      bool keep_alive_after_operation,
      const absl::optional<std::string>& context_id,
      RunURLSelectionOperationOnWorkletCallback callback) override;
  void SharedStorageSet(const std::u16string& key,
                        const std::u16string& value,
                        bool ignore_if_present,
                        SharedStorageSetCallback callback) override;
  void SharedStorageAppend(const std::u16string& key,
                           const std::u16string& value,
                           SharedStorageAppendCallback callback) override;
  void SharedStorageDelete(const std::u16string& key,
                           SharedStorageDeleteCallback callback) override;
  void SharedStorageClear(SharedStorageClearCallback callback) override;

  base::WeakPtr<SharedStorageDocumentServiceImpl> GetWeakPtr();

 private:
  friend class DocumentUserData;

  static bool& GetBypassIsSharedStorageAllowed();

  explicit SharedStorageDocumentServiceImpl(RenderFrameHost*);

  SharedStorageWorkletHostManager* GetSharedStorageWorkletHostManager();

  SharedStorageWorkletHost* GetSharedStorageWorkletHost();

  storage::SharedStorageManager* GetSharedStorageManager();

  bool IsSharedStorageAllowed();

  bool IsSharedStorageSelectURLAllowed();

  bool IsSharedStorageAddModuleAllowed();

  std::string SerializeLastCommittedOrigin() const;

  mojo::AssociatedReceiver<blink::mojom::SharedStorageDocumentService>
      receiver_{this};

  // To avoid race conditions associated with top frame navigations, we need to
  // save the value of the main frame origin in the constructor.
  const url::Origin main_frame_origin_;

  // The DevTools frame token for the main frame, to be used by notifications
  // to DevTools.
  const std::string main_frame_id_;

  // Whether or not the worklet should be kept alive after the current worklet
  // operation (i.e. `addModule()`, `run()`, or `selectURL()`). If
  // `keep_alive_worklet_after_operation_` is false and a subsequent call to one
  // of these operations is placed, then that new call will fail. Otherwise,
  // when `keep_alive_worklet_after_operation_` is true, a subsequent call to
  // `run()` or `selectURL()` will be permitted but the value of
  // `keep_alive_worklet_after_operation_` will update to the value of the
  // call's parameter `keep_alive_worklet_after_operation` for any future call.
  bool keep_alive_worklet_after_operation_ = true;

  DOCUMENT_USER_DATA_KEY_DECL();

  base::WeakPtrFactory<SharedStorageDocumentServiceImpl> weak_ptr_factory_{
      this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_SHARED_STORAGE_SHARED_STORAGE_DOCUMENT_SERVICE_IMPL_H_
