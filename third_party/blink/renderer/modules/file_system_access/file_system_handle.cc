// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/file_system_access/file_system_handle.h"

#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_error.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_file_system_handle_permission_descriptor.h"
#include "third_party/blink/renderer/modules/file_system_access/file_system_access_error.h"
#include "third_party/blink/renderer/modules/file_system_access/file_system_directory_handle.h"
#include "third_party/blink/renderer/modules/file_system_access/file_system_file_handle.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {
using mojom::blink::FileSystemAccessEntryPtr;
using mojom::blink::FileSystemAccessErrorPtr;

FileSystemHandle::FileSystemHandle(ExecutionContext* execution_context,
                                   const String& name)
    : ExecutionContextClient(execution_context), name_(name) {}

// static
FileSystemHandle* FileSystemHandle::CreateFromMojoEntry(
    mojom::blink::FileSystemAccessEntryPtr e,
    ExecutionContext* execution_context) {
  if (e->entry_handle->is_file()) {
    return MakeGarbageCollected<FileSystemFileHandle>(
        execution_context, e->name, std::move(e->entry_handle->get_file()));
  }
  return MakeGarbageCollected<FileSystemDirectoryHandle>(
      execution_context, e->name, std::move(e->entry_handle->get_directory()));
}

namespace {
String MojoPermissionStatusToString(mojom::blink::PermissionStatus status) {
  switch (status) {
    case mojom::blink::PermissionStatus::GRANTED:
      return "granted";
    case mojom::blink::PermissionStatus::DENIED:
      return "denied";
    case mojom::blink::PermissionStatus::ASK:
      return "prompt";
  }
  NOTREACHED();
  return "denied";
}

}  // namespace

ScriptPromise FileSystemHandle::queryPermission(
    ScriptState* script_state,
    const FileSystemHandlePermissionDescriptor* descriptor) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise result = resolver->Promise();

  QueryPermissionImpl(
      descriptor->mode() == "readwrite",
      WTF::BindOnce(
          [](FileSystemHandle* handle, ScriptPromiseResolver* resolver,
             mojom::blink::PermissionStatus result) {
            // Keep `this` alive so the handle will not be garbage-collected
            // before the promise is resolved.
            resolver->Resolve(MojoPermissionStatusToString(result));
          },
          WrapPersistent(this), WrapPersistent(resolver)));

  return result;
}

ScriptPromise FileSystemHandle::requestPermission(
    ScriptState* script_state,
    const FileSystemHandlePermissionDescriptor* descriptor,
    ExceptionState& exception_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());
  ScriptPromise result = resolver->Promise();

  RequestPermissionImpl(
      descriptor->mode() == "readwrite",
      WTF::BindOnce(
          [](FileSystemHandle*, ScriptPromiseResolver* resolver,
             FileSystemAccessErrorPtr result,
             mojom::blink::PermissionStatus status) {
            // Keep `this` alive so the handle will not be garbage-collected
            // before the promise is resolved.
            if (result->status != mojom::blink::FileSystemAccessStatus::kOk) {
              file_system_access_error::Reject(resolver, *result);
              return;
            }
            resolver->Resolve(MojoPermissionStatusToString(status));
          },
          WrapPersistent(this), WrapPersistent(resolver)));

  return result;
}

ScriptPromise FileSystemHandle::move(ScriptState* script_state,
                                     const String& new_entry_name,
                                     ExceptionState& exception_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());
  ScriptPromise result = resolver->Promise();

  MoveImpl(
      mojo::NullRemote(), new_entry_name,
      WTF::BindOnce(
          [](FileSystemHandle* handle, const String& new_name,
             ScriptPromiseResolver* resolver, FileSystemAccessErrorPtr result) {
            if (result->status == mojom::blink::FileSystemAccessStatus::kOk) {
              handle->name_ = new_name;
            }
            file_system_access_error::ResolveOrReject(resolver, *result);
          },
          WrapPersistent(this), new_entry_name, WrapPersistent(resolver)));

  return result;
}

ScriptPromise FileSystemHandle::move(
    ScriptState* script_state,
    FileSystemDirectoryHandle* destination_directory,
    ExceptionState& exception_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());
  ScriptPromise result = resolver->Promise();

  MoveImpl(destination_directory->Transfer(), name_,
           WTF::BindOnce(
               [](FileSystemHandle*, ScriptPromiseResolver* resolver,
                  FileSystemAccessErrorPtr result) {
                 // Keep `this` alive so the handle will not be
                 // garbage-collected before the promise is resolved.
                 file_system_access_error::ResolveOrReject(resolver, *result);
               },
               WrapPersistent(this), WrapPersistent(resolver)));

  return result;
}

ScriptPromise FileSystemHandle::move(
    ScriptState* script_state,
    FileSystemDirectoryHandle* destination_directory,
    const String& new_entry_name,
    ExceptionState& exception_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());
  ScriptPromise result = resolver->Promise();

  MoveImpl(
      destination_directory->Transfer(), new_entry_name,
      WTF::BindOnce(
          [](FileSystemHandle* handle, const String& new_name,
             ScriptPromiseResolver* resolver, FileSystemAccessErrorPtr result) {
            if (result->status == mojom::blink::FileSystemAccessStatus::kOk) {
              handle->name_ = new_name;
            }
            file_system_access_error::ResolveOrReject(resolver, *result);
          },
          WrapPersistent(this), new_entry_name, WrapPersistent(resolver)));

  return result;
}

ScriptPromise FileSystemHandle::remove(ScriptState* script_state,
                                       const FileSystemRemoveOptions* options,
                                       ExceptionState& exception_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());
  ScriptPromise result = resolver->Promise();

  RemoveImpl(options, WTF::BindOnce(
                          [](FileSystemHandle*, ScriptPromiseResolver* resolver,
                             FileSystemAccessErrorPtr result) {
                            // Keep `this` alive so the handle will not be
                            // garbage-collected before the promise is resolved.
                            file_system_access_error::ResolveOrReject(resolver,
                                                                      *result);
                          },
                          WrapPersistent(this), WrapPersistent(resolver)));

  return result;
}

ScriptPromise FileSystemHandle::isSameEntry(ScriptState* script_state,
                                            FileSystemHandle* other,
                                            ExceptionState& exception_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());
  ScriptPromise result = resolver->Promise();

  IsSameEntryImpl(
      other->Transfer(),
      WTF::BindOnce(
          [](FileSystemHandle*, ScriptPromiseResolver* resolver,
             FileSystemAccessErrorPtr result, bool same) {
            // Keep `this` alive so the handle will not be garbage-collected
            // before the promise is resolved.
            if (result->status != mojom::blink::FileSystemAccessStatus::kOk) {
              file_system_access_error::Reject(resolver, *result);
              return;
            }
            resolver->Resolve(same);
          },
          WrapPersistent(this), WrapPersistent(resolver)));
  return result;
}

ScriptPromise FileSystemHandle::getUniqueId(ScriptState* script_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise result = resolver->Promise();

  GetUniqueIdImpl(WTF::BindOnce(
      [](FileSystemHandle*, ScriptPromiseResolver* resolver,
         const WTF::String& id) {
        // Keep `this` alive so the handle will not be garbage-collected
        // before the promise is resolved.
        resolver->Resolve(std::move(id));
      },
      WrapPersistent(this), WrapPersistent(resolver)));
  return result;
}

void FileSystemHandle::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
}

}  // namespace blink
