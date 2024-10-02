// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_return_value.h"

#include <stdint.h>
#include <vector>

#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom.h"

namespace content {

// static
blink::mojom::IDBReturnValuePtr IndexedDBReturnValue::ConvertReturnValue(
    IndexedDBReturnValue* value) {
  auto mojo_value = blink::mojom::IDBReturnValue::New();
  mojo_value->value = blink::mojom::IDBValue::New();
  if (value->primary_key.IsValid()) {
    mojo_value->primary_key = value->primary_key;
    mojo_value->key_path = value->key_path;
  }
  if (!value->empty()) {
    // TODO(crbug.com/902498): Use mojom traits to map directly from
    //                         std::string.
    const char* value_data = value->bits.data();
    mojo_value->value->bits =
        std::vector<uint8_t>(value_data, value_data + value->bits.length());
    // Release value->bits std::string.
    value->bits.clear();
  }
  IndexedDBExternalObject::ConvertToMojo(value->external_objects,
                                         &mojo_value->value->external_objects);
  return mojo_value;
}

}  // namespace content
