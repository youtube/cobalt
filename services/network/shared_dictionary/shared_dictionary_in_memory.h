// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_SHARED_DICTIONARY_SHARED_DICTIONARY_IN_MEMORY_H_
#define SERVICES_NETWORK_SHARED_DICTIONARY_SHARED_DICTIONARY_IN_MEMORY_H_

#include "services/network/shared_dictionary/shared_dictionary.h"

#include "net/base/hash_value.h"

namespace network {

// A SharedDictionary which can be obtained using
// SharedDictionaryStorageInMemory::GetDictionary(). All binary data is in the
// memory. So ReadAll() synchronously returns OK.
class SharedDictionaryInMemory : public SharedDictionary {
 public:
  SharedDictionaryInMemory(scoped_refptr<net::IOBuffer> data,
                           size_t size,
                           const net::SHA256HashValue& sha256);
  ~SharedDictionaryInMemory() override;

  SharedDictionaryInMemory(const SharedDictionaryInMemory&) = delete;
  SharedDictionaryInMemory& operator=(const SharedDictionaryInMemory&) = delete;

  // SharedDictionary
  int ReadAll(base::OnceCallback<void(int)> callback) override;
  scoped_refptr<net::IOBuffer> data() const override;
  size_t size() const override;
  const net::SHA256HashValue& hash() const override;

 private:
  const scoped_refptr<net::IOBuffer> data_;
  const size_t size_;
  const net::SHA256HashValue sha256_;
#if DCHECK_IS_ON()
  bool read_all_called_ = false;
#endif  // DCHECK_IS_ON()
};

}  // namespace network

#endif  // SERVICES_NETWORK_SHARED_DICTIONARY_SHARED_DICTIONARY_IN_MEMORY_H_
