// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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
#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/dom/blob_property_bag.h"
#include "cobalt/dom/dom_settings.h"

namespace cobalt {
namespace dom {

namespace {

const void* DataStart(const Blob::BlobPart& part) {
  if (part.IsType<script::Handle<script::ArrayBuffer> >()) {
    return part.AsType<script::Handle<script::ArrayBuffer> >()->Data();
  } else if (part.IsType<script::Handle<script::ArrayBufferView> >()) {
    const script::Handle<script::ArrayBufferView>& view =
        part.AsType<script::Handle<script::ArrayBufferView> >();
    return view->RawData();
  } else if (part.IsType<script::Handle<script::DataView> >()) {
    const script::Handle<script::DataView>& view =
        part.AsType<script::Handle<script::DataView> >();
    return view->RawData();
  } else if (part.IsType<scoped_refptr<Blob> >()) {
    return part.AsType<scoped_refptr<Blob> >()->data();
  } else if (part.IsType<std::string>()) {
    return part.AsType<std::string>().data();
  }

  return NULL;
}

size_t DataLength(const Blob::BlobPart& part) {
  if (part.IsType<script::Handle<script::ArrayBuffer> >()) {
    return part.AsType<script::Handle<script::ArrayBuffer> >()->ByteLength();
  } else if (part.IsType<script::Handle<script::ArrayBufferView> >()) {
    return part.AsType<script::Handle<script::ArrayBufferView> >()
        ->ByteLength();
  } else if (part.IsType<script::Handle<script::DataView> >()) {
    return part.AsType<script::Handle<script::DataView> >()->ByteLength();
  } else if (part.IsType<scoped_refptr<Blob> >()) {
    return static_cast<size_t>(part.AsType<scoped_refptr<Blob> >()->size());
  } else if (part.IsType<std::string>()) {
    return static_cast<size_t>(part.AsType<std::string>().size());
  }

  return 0;
}

size_t TotalDataLength(const script::Sequence<Blob::BlobPart>& blob_parts) {
  size_t byte_length = 0;
  for (script::Sequence<Blob::BlobPart>::size_type i = 0; i < blob_parts.size();
       i++) {
    byte_length += DataLength(blob_parts.at(i));
  }

  return byte_length;
}

base::LazyInstance<BlobPropertyBag>::DestructorAtExit empty_blob_property_bag =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

Blob::Blob(script::EnvironmentSettings* settings,
           const script::Handle<script::ArrayBuffer>& buffer,
           const BlobPropertyBag& options)
    : buffer_reference_(
          this, buffer.IsEmpty()
                    ? script::ArrayBuffer::New(
                          base::polymorphic_downcast<DOMSettings*>(settings)
                              ->global_environment(),
                          0)
                    : script::ArrayBuffer::New(
                          base::polymorphic_downcast<DOMSettings*>(settings)
                              ->global_environment(),
                          buffer->Data(), buffer->ByteLength())) {
  DCHECK(settings);
  if (options.has_type()) {
    type_ = options.type();
  }
}

// TODO: Do the appropriate steps to convert the type member to lowercase ASCII
// so the media type can be exposed and used, as described in:
//    https://www.w3.org/TR/FileAPI/#constructorBlob
Blob::Blob(script::EnvironmentSettings* settings,
           const script::Sequence<BlobPart>& blob_parts,
           const BlobPropertyBag& options)
    : buffer_reference_(this,
                        script::ArrayBuffer::New(
                            base::polymorphic_downcast<DOMSettings*>(settings)
                                ->global_environment(),
                            TotalDataLength(blob_parts))),
      type_(options.type()) {
  DCHECK(settings);
  uint8* destination = static_cast<uint8*>(buffer_reference_.value().Data());
  size_t offset = 0;
  for (script::Sequence<BlobPart>::size_type i = 0; i < blob_parts.size();
       ++i) {
    const uint8* source =
        static_cast<const uint8*>(DataStart(blob_parts.at(i)));
    size_t count = DataLength(blob_parts.at(i));

    std::copy(source, source + count, destination + offset);
    offset += count;
  }
}

const BlobPropertyBag& Blob::EmptyBlobPropertyBag() {
  return empty_blob_property_bag.Get();
}

}  // namespace dom
}  // namespace cobalt
