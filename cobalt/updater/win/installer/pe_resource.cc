// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/win/installer/pe_resource.h"

namespace updater {

PEResource::PEResource(const wchar_t* name, const wchar_t* type, HMODULE module)
    : resource_(nullptr), module_(module) {
  resource_ = ::FindResource(module, name, type);
}

bool PEResource::IsValid() {
  return nullptr != resource_;
}

size_t PEResource::Size() {
  return ::SizeofResource(module_, resource_);
}

bool PEResource::WriteToDisk(const wchar_t* full_path) {
  // Resource handles are not real HGLOBALs so do not attempt to close them.
  // Windows frees them whenever there is memory pressure.
  HGLOBAL data_handle = ::LoadResource(module_, resource_);
  if (nullptr == data_handle)
    return false;

  void* data = ::LockResource(data_handle);
  if (nullptr == data)
    return false;

  size_t resource_size = Size();
  HANDLE out_file = ::CreateFile(full_path, GENERIC_WRITE, 0, nullptr,
                                 CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (INVALID_HANDLE_VALUE == out_file)
    return false;

  DWORD written = 0;
  if (!::WriteFile(out_file, data, static_cast<DWORD>(resource_size), &written,
                   nullptr)) {
    ::CloseHandle(out_file);
    return false;
  }
  return ::CloseHandle(out_file) ? true : false;
}

}  // namespace updater
