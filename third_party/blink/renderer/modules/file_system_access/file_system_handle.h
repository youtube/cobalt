// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_FILE_SYSTEM_ACCESS_FILE_SYSTEM_HANDLE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_FILE_SYSTEM_ACCESS_FILE_SYSTEM_HANDLE_H_

#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_directory_handle.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_error.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_transfer_token.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom-blink-forward.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {
class ExceptionState;
class ExecutionContext;
class FileSystemHandlePermissionDescriptor;
class FileSystemRemoveOptions;
class FileSystemDirectoryHandle;

class FileSystemHandle : public ScriptWrappable, public ExecutionContextClient {
  DEFINE_WRAPPERTYPEINFO();

 public:
  FileSystemHandle(ExecutionContext* execution_context, const String& name);
  static FileSystemHandle* CreateFromMojoEntry(
      mojom::blink::FileSystemAccessEntryPtr,
      ExecutionContext* execution_context);

  virtual bool isFile() const { return false; }
  virtual bool isDirectory() const { return false; }
  const String kind() const { return isFile() ? "file" : "directory"; }
  const String& name() const { return name_; }

  ScriptPromise queryPermission(ScriptState*,
                                const FileSystemHandlePermissionDescriptor*);
  ScriptPromise requestPermission(ScriptState*,
                                  const FileSystemHandlePermissionDescriptor*,
                                  ExceptionState&);

  ScriptPromise move(ScriptState*,
                     const String& new_entry_name,
                     ExceptionState&);
  ScriptPromise move(ScriptState*,
                     FileSystemDirectoryHandle* destination_directory,
                     ExceptionState&);
  ScriptPromise move(ScriptState*,
                     FileSystemDirectoryHandle* destination_directory,
                     const String& new_entry_name,
                     ExceptionState&);
  ScriptPromise remove(ScriptState*,
                       const FileSystemRemoveOptions* options,
                       ExceptionState&);

  ScriptPromise isSameEntry(ScriptState*,
                            FileSystemHandle* other,
                            ExceptionState&);
  ScriptPromise getUniqueId(ScriptState*);

  // Grab a handle to a transfer token. This may return an invalid PendingRemote
  // if the context is already destroyed.
  virtual mojo::PendingRemote<mojom::blink::FileSystemAccessTransferToken>
  Transfer() = 0;

  void Trace(Visitor*) const override;

 private:
  virtual void QueryPermissionImpl(
      bool writable,
      base::OnceCallback<void(mojom::blink::PermissionStatus)>) = 0;
  virtual void RequestPermissionImpl(
      bool writable,
      base::OnceCallback<void(mojom::blink::FileSystemAccessErrorPtr,
                              mojom::blink::PermissionStatus)>) = 0;
  virtual void MoveImpl(
      mojo::PendingRemote<mojom::blink::FileSystemAccessTransferToken> dest,
      const String& new_entry_name,
      base::OnceCallback<void(mojom::blink::FileSystemAccessErrorPtr)>) = 0;
  virtual void RemoveImpl(
      const FileSystemRemoveOptions* options,
      base::OnceCallback<void(mojom::blink::FileSystemAccessErrorPtr)>) = 0;
  virtual void IsSameEntryImpl(
      mojo::PendingRemote<mojom::blink::FileSystemAccessTransferToken> other,
      base::OnceCallback<void(mojom::blink::FileSystemAccessErrorPtr,
                              bool)>) = 0;
  virtual void GetUniqueIdImpl(
      base::OnceCallback<void(const WTF::String&)>) = 0;

  String name_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_FILE_SYSTEM_ACCESS_FILE_SYSTEM_HANDLE_H_
