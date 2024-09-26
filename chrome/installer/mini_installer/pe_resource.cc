// Copyright 2006-2009 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/mini_installer/pe_resource.h"

#include <algorithm>

#include "chrome/installer/mini_installer/mini_file.h"

PEResource::PEResource(HRSRC resource, HMODULE module)
    : resource_(resource), module_(module) {}

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
  // Resources are freed when the containing module is unloaded.
  HGLOBAL data_handle = ::LoadResource(module_, resource_);
  if (nullptr == data_handle)
    return false;

  const char* data = reinterpret_cast<const char*>(::LockResource(data_handle));
  if (nullptr == data)
    return false;

  mini_installer::MiniFile file;
  if (!file.Create(full_path))
    return false;

  // Don't write all of the data at once because this can lead to kernel
  // address-space exhaustion on 32-bit Windows (see https://crbug.com/1001022
  // for details).
  constexpr size_t kMaxWriteAmount = 8 * 1024 * 1024;
  const size_t resource_size = Size();
  for (size_t total_written = 0; total_written < resource_size; /**/) {
    const size_t write_amount =
        std::min(kMaxWriteAmount, resource_size - total_written);
    DWORD written = 0;
    if (!::WriteFile(file.GetHandleUnsafe(), data + total_written,
                     static_cast<DWORD>(write_amount), &written, nullptr)) {
      const auto write_error = ::GetLastError();

      // Delete the file since the write failed.
      file.DeleteOnClose();
      file.Close();

      ::SetLastError(write_error);
      return false;
    }
    total_written += write_amount;
  }
  return true;
}
