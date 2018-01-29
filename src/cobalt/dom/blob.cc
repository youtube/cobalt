// Copyright 2017 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "cobalt/dom/blob.h"

#include "base/lazy_instance.h"
#include "cobalt/dom/blob_property_bag.h"

namespace cobalt {
namespace dom {

namespace {

const uint8* DataStart(const Blob::BlobPart& part) {
  if (part.IsType<scoped_refptr<ArrayBuffer> >()) {
    return part.AsType<scoped_refptr<ArrayBuffer> >()->data();
  } else if (part.IsType<scoped_refptr<ArrayBufferView> >()) {
    const scoped_refptr<ArrayBufferView>& view =
        part.AsType<scoped_refptr<ArrayBufferView> >();
    return view->buffer()->data() + view->byte_offset();
  } else if (part.IsType<scoped_refptr<DataView> >()) {
    const scoped_refptr<DataView>& view =
        part.AsType<scoped_refptr<DataView> >();
    return view->buffer()->data() + view->byte_offset();
  } else if (part.IsType<scoped_refptr<Blob> >()) {
    return part.AsType<scoped_refptr<Blob> >()->data();
  }

  return NULL;
}

uint64 DataLength(const Blob::BlobPart& part) {
  if (part.IsType<scoped_refptr<ArrayBuffer> >()) {
    return part.AsType<scoped_refptr<ArrayBuffer> >()->byte_length();
  } else if (part.IsType<scoped_refptr<ArrayBufferView> >()) {
    return part.AsType<scoped_refptr<ArrayBufferView> >()->byte_length();
  } else if (part.IsType<scoped_refptr<DataView> >()) {
    return part.AsType<scoped_refptr<DataView> >()->byte_length();
  } else if (part.IsType<scoped_refptr<Blob> >()) {
    return part.AsType<scoped_refptr<Blob> >()->size();
  }

  return 0;
}

base::LazyInstance<BlobPropertyBag> empty_blob_property_bag =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

Blob::Blob(script::EnvironmentSettings* settings,
           const scoped_refptr<ArrayBuffer>& buffer) {
  if (buffer) {
    buffer_ = buffer->Slice(settings, 0);
  } else {
    buffer_ = new ArrayBuffer(settings, 0);
  }
}

// TODO: Do the appropriate steps to convert the type member to lowercase ASCII
// so the media type can be exposed and used, as described in:
//    https://www.w3.org/TR/FileAPI/#constructorBlob
Blob::Blob(script::EnvironmentSettings* settings,
           script::Sequence<BlobPart> blobParts, const BlobPropertyBag& options)
    : buffer_(new ArrayBuffer(settings, 0)), type_(options.type()) {
  size_t byte_length = 0;
  for (script::Sequence<BlobPart>::size_type i = 0; i < blobParts.size(); i++) {
    byte_length += DataLength(blobParts.at(i));
  }
  buffer_ = new ArrayBuffer(settings, static_cast<uint32>(byte_length));

  uint8* destination = buffer_->data();
  size_t offset = 0;
  for (script::Sequence<BlobPart>::size_type i = 0; i < blobParts.size(); i++) {
    const uint8* source = DataStart(blobParts.at(i));
    uint64 count = DataLength(blobParts.at(i));

    std::copy(source, source + count, destination + offset);
    offset += count;
  }
}

void Blob::TraceMembers(script::Tracer* tracer) { tracer->Trace(buffer_); }

const BlobPropertyBag& Blob::EmptyBlobPropertyBag() {
  return empty_blob_property_bag.Get();
}

}  // namespace dom
}  // namespace cobalt
