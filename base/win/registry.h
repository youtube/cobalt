// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_WIN_REGISTRY_H_
#define BASE_WIN_REGISTRY_H_
#pragma once

#include <windows.h>
#include <string>

#include "base/basictypes.h"

// Please ignore this part. This temporary hack exists
// to detect if the return value is used as 'bool'.
// Todo(amit): remove this before (or soon after) checkin.
struct CatchBoolChecks {
  CatchBoolChecks(LONG l) : l_(l) {}
  LONG l_;
  operator LONG() { return l_; }
  LONG value() const { return l_; }
  bool operator == (LONG l) const { return l == l_; }
  bool operator != (LONG l) const { return l != l_; }
 private:
  // If you hit a compile error here, you most likely attempting to use the
  // return value of a RegKey helper as a bool. Please note that RegKey
  // methods return LONG now instead of bool.
  operator bool () { return false; }
};

inline bool operator == (const LONG& l, const CatchBoolChecks& g) {
  return g.value() == l;
}

inline bool operator != (const LONG& l, const CatchBoolChecks& g) {
  return g.value() != l;
}

using std::ostream;
inline ostream& operator <<(ostream &os, const CatchBoolChecks& g) {
  os << g.value();
  return os;
}

typedef CatchBoolChecks GONG;

namespace base {
namespace win {

// Utility class to read, write and manipulate the Windows Registry.
// Registry vocabulary primer: a "key" is like a folder, in which there
// are "values", which are <name, data> pairs, with an associated data type.
//
// Note:
// ReadValue family of functions guarantee that the return arguments
// are not touched in case of failure.
class RegKey {
 public:
  RegKey();
  RegKey(HKEY rootkey, const wchar_t* subkey, REGSAM access);
  ~RegKey();

  GONG Create(HKEY rootkey, const wchar_t* subkey, REGSAM access);

  GONG CreateWithDisposition(HKEY rootkey, const wchar_t* subkey,
                             DWORD* disposition, REGSAM access);

  GONG Open(HKEY rootkey, const wchar_t* subkey, REGSAM access);

  // Creates a subkey or open it if it already exists.
  GONG CreateKey(const wchar_t* name, REGSAM access);

  // Opens a subkey
  GONG OpenKey(const wchar_t* name, REGSAM access);

  void Close();

  DWORD ValueCount() const;

  // Determine the nth value's name.
  GONG ReadName(int index, std::wstring* name) const;

  // True while the key is valid.
  bool Valid() const { return key_ != NULL; }

  // Kill a key and everything that live below it; please be careful when using
  // it.
  GONG DeleteKey(const wchar_t* name);

  // Deletes a single value within the key.
  GONG DeleteValue(const wchar_t* name);

  bool ValueExists(const wchar_t* name) const;

  GONG ReadValue(const wchar_t* name, void* data, DWORD* dsize,
                 DWORD* dtype) const;
  GONG ReadValue(const wchar_t* name, std::wstring* value) const;
  GONG ReadValueDW(const wchar_t* name, DWORD* value) const;
  GONG ReadInt64(const wchar_t* name, int64* value) const;

  GONG WriteValue(const wchar_t* name, const void* data, DWORD dsize,
                  DWORD dtype);
  GONG WriteValue(const wchar_t* name, const wchar_t* value);
  GONG WriteValue(const wchar_t* name, DWORD value);

  // Starts watching the key to see if any of its values have changed.
  // The key must have been opened with the KEY_NOTIFY access privilege.
  GONG StartWatching();

  // If StartWatching hasn't been called, always returns false.
  // Otherwise, returns true if anything under the key has changed.
  // This can't be const because the |watch_event_| may be refreshed.
  bool HasChanged();

  // Will automatically be called by destructor if not manually called
  // beforehand.  Returns true if it was watching, false otherwise.
  GONG StopWatching();

  inline bool IsWatching() const { return watch_event_ != 0; }
  HANDLE watch_event() const { return watch_event_; }
  HKEY Handle() const { return key_; }

 private:
  HKEY key_;  // The registry key being iterated.
  HANDLE watch_event_;

  DISALLOW_COPY_AND_ASSIGN(RegKey);
};

// Iterates the entries found in a particular folder on the registry.
// For this application I happen to know I wont need data size larger
// than MAX_PATH, but in real life this wouldn't neccessarily be
// adequate.
class RegistryValueIterator {
 public:
  RegistryValueIterator(HKEY root_key, const wchar_t* folder_key);

  ~RegistryValueIterator();

  DWORD ValueCount() const;

  // True while the iterator is valid.
  bool Valid() const;

  // Advances to the next registry entry.
  void operator++();

  const wchar_t* Name() const { return name_; }
  const wchar_t* Value() const { return value_; }
  DWORD ValueSize() const { return value_size_; }
  DWORD Type() const { return type_; }

  int Index() const { return index_; }

 private:
  // Read in the current values.
  bool Read();

  // The registry key being iterated.
  HKEY key_;

  // Current index of the iteration.
  int index_;

  // Current values.
  wchar_t name_[MAX_PATH];
  wchar_t value_[MAX_PATH];
  DWORD value_size_;
  DWORD type_;

  DISALLOW_COPY_AND_ASSIGN(RegistryValueIterator);
};

class RegistryKeyIterator {
 public:
  RegistryKeyIterator(HKEY root_key, const wchar_t* folder_key);

  ~RegistryKeyIterator();

  DWORD SubkeyCount() const;

  // True while the iterator is valid.
  bool Valid() const;

  // Advances to the next entry in the folder.
  void operator++();

  const wchar_t* Name() const { return name_; }

  int Index() const { return index_; }

 private:
  // Read in the current values.
  bool Read();

  // The registry key being iterated.
  HKEY key_;

  // Current index of the iteration.
  int index_;

  wchar_t name_[MAX_PATH];

  DISALLOW_COPY_AND_ASSIGN(RegistryKeyIterator);
};

}  // namespace win
}  // namespace base

#endif  // BASE_WIN_REGISTRY_H_
