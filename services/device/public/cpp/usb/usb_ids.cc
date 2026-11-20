// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/public/cpp/usb/usb_ids.h"

#include <cstdint>
#include <optional>

#include "base/containers/span.h"
#include "base/memory/raw_span.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "base/numerics/byte_conversions.h"
#include "base/numerics/checked_math.h"
#include "services/device/public/cpp/usb/grit/usb_ids_resources.h"  // nogncheck
#include "ui/base/resource/resource_bundle.h"

namespace device {

namespace {

// See services/device/public/cpp/usb/tools/usb_ids.py for the canonical
// description of the binary layout.
constexpr uint32_t kExpectedMagic = 0x49425355;  // 'USBI'
constexpr uint32_t kExpectedVersion = 2;
constexpr size_t kHeaderSize = 16;
constexpr size_t kVendorEntrySize = 12;
constexpr size_t kProductEntrySize = 4;

// Reads a single vendor entry at index |i| from |vendors|.
struct VendorEntry {
  uint16_t vid;
  uint16_t product_count;
  uint32_t name_off;
  uint32_t products_off;
};

VendorEntry ReadVendor(base::span<const uint8_t> vendors, size_t i) {
  auto entry = vendors.subspan(i * kVendorEntrySize).first<kVendorEntrySize>();
  return {
      base::U16FromLittleEndian(entry.first<2>()),
      base::U16FromLittleEndian(entry.subspan<2, 2>()),
      base::U32FromLittleEndian(entry.subspan<4, 4>()),
      base::U32FromLittleEndian(entry.subspan<8, 4>()),
  };
}

struct ProductEntry {
  uint16_t pid;
  uint32_t name_off;
};

ProductEntry ReadProduct(base::span<const uint8_t> products, size_t i) {
  auto entry =
      products.subspan(i * kProductEntrySize).first<kProductEntrySize>();
  return {
      base::U16FromLittleEndian(entry.first<2>()),
      base::U16FromLittleEndian(entry.subspan<2, 2>()),
  };
}

// Parsed view over the binary blob. Built once, then used for read-only
// lookups.
class UsbIdsTable {
  struct FoundVendor {
    size_t index;
    VendorEntry entry;
  };

 public:
  // Builds a table over |blob|. Returns std::nullopt if the blob is malformed.
  static std::optional<UsbIdsTable> Parse(base::span<const uint8_t> blob) {
    if (blob.size() < kHeaderSize) {
      return std::nullopt;
    }
    auto header = blob.first<kHeaderSize>();
    const uint32_t magic = base::U32FromLittleEndian(header.first<4>());
    const uint32_t version = base::U32FromLittleEndian(header.subspan<4, 4>());
    const uint32_t vendor_count =
        base::U32FromLittleEndian(header.subspan<8, 4>());
    const uint32_t string_blob_size =
        base::U32FromLittleEndian(header.subspan<12, 4>());

    if (magic != kExpectedMagic || version != kExpectedVersion) {
      return std::nullopt;
    }

    size_t vendor_table_bytes;
    if (!(base::CheckedNumeric<size_t>(vendor_count) * kVendorEntrySize)
             .AssignIfValid(&vendor_table_bytes)) {
      return std::nullopt;
    }
    size_t expected_size;
    if (!(base::CheckedNumeric<size_t>(kHeaderSize) + vendor_table_bytes +
          string_blob_size)
             .AssignIfValid(&expected_size)) {
      return std::nullopt;
    }
    if (expected_size > blob.size()) {
      return std::nullopt;
    }

    UsbIdsTable table;
    table.blob_ = blob;
    table.vendors_ = blob.subspan(kHeaderSize, vendor_table_bytes);
    table.vendor_count_ = vendor_count;
    table.string_blob_ =
        blob.subspan(blob.size() - string_blob_size, string_blob_size);
    return table;
  }

  const char* GetVendorName(uint16_t vendor_id) const {
    std::optional<FoundVendor> vendor = FindVendor(vendor_id);
    if (!vendor.has_value()) {
      return nullptr;
    }
    return StringAt(vendor->entry.name_off);
  }

  UsbIdNames GetVendorAndProductName(uint16_t vendor_id,
                                     uint16_t product_id) const {
    std::optional<FoundVendor> vendor = FindVendor(vendor_id);
    if (!vendor.has_value()) {
      return {};
    }
    return {.vendor_name = StringAt(vendor->entry.name_off),
            .product_name = FindProductName(*vendor, product_id)};
  }

 private:
  // Returns the product name for |product_id| within an already located
  // |vendor|, or nullptr if the product does not exist.
  const char* FindProductName(const FoundVendor& vendor,
                              uint16_t product_id) const {
    const VendorEntry& v = vendor.entry;
    if (v.product_count == 0 || v.products_off == 0) {
      return nullptr;
    }
    size_t products_bytes;
    if (!(base::CheckedNumeric<size_t>(v.product_count) * kProductEntrySize)
             .AssignIfValid(&products_bytes)) {
      return nullptr;
    }
    size_t product_string_start;
    if (!(base::CheckedNumeric<size_t>(v.products_off) + products_bytes)
             .AssignIfValid(&product_string_start)) {
      return nullptr;
    }
    if (product_string_start > blob_.size()) {
      return nullptr;
    }
    const size_t product_block_end = FindProductBlockEnd(vendor.index);
    if (product_string_start > product_block_end) {
      return nullptr;
    }
    base::span<const uint8_t> products =
        blob_.subspan(v.products_off, products_bytes);
    base::span<const uint8_t> product_strings = blob_.subspan(
        product_string_start, product_block_end - product_string_start);

    // Binary search by pid.
    size_t lo = 0;
    size_t hi = v.product_count;
    while (lo < hi) {
      const size_t mid = lo + (hi - lo) / 2;
      if (ReadProduct(products, mid).pid < product_id) {
        lo = mid + 1;
      } else {
        hi = mid;
      }
    }
    if (lo == v.product_count) {
      return nullptr;
    }
    const ProductEntry p = ReadProduct(products, lo);
    if (p.pid != product_id) {
      return nullptr;
    }
    return StringAt(product_strings, p.name_off);
  }

  // Returns the vendor whose id == |vendor_id|, or nullopt.
  std::optional<FoundVendor> FindVendor(uint16_t vendor_id) const {
    size_t lo = 0;
    size_t hi = vendor_count_;
    while (lo < hi) {
      const size_t mid = lo + (hi - lo) / 2;
      if (ReadVendor(vendors_, mid).vid < vendor_id) {
        lo = mid + 1;
      } else {
        hi = mid;
      }
    }
    if (lo == vendor_count_) {
      return std::nullopt;
    }
    VendorEntry vendor = ReadVendor(vendors_, lo);
    if (vendor.vid != vendor_id) {
      return std::nullopt;
    }
    return FoundVendor{lo, vendor};
  }

  size_t FindProductBlockEnd(size_t vendor_index) const {
    const size_t string_blob_off = blob_.size() - string_blob_.size();
    for (size_t i = vendor_index + 1; i < vendor_count_; ++i) {
      const VendorEntry vendor = ReadVendor(vendors_, i);
      if (vendor.products_off != 0) {
        return vendor.products_off;
      }
    }
    return string_blob_off;
  }

  // String pools are NUL-terminated; the data was generated from a fixed set
  // of byte sequences, so a strchr-style scan is safe and bounded by the
  // containing blob size.
  const char* StringAt(base::span<const uint8_t> strings,
                       uint32_t offset) const {
    if (offset >= strings.size()) {
      return nullptr;
    }
    return reinterpret_cast<const char*>(strings.subspan(offset).data());
  }

  const char* StringAt(uint32_t offset) const {
    return StringAt(string_blob_, offset);
  }

  base::raw_span<const uint8_t> blob_;
  base::raw_span<const uint8_t> vendors_;
  base::raw_span<const uint8_t> string_blob_;
  size_t vendor_count_ = 0;
};

// Owns the decompressed resource bytes plus a parsed view over them. The
// resource is gzip-compressed in the .pak; ui::ResourceBundle materializes
// the decompressed bytes once and we keep that allocation alive for the
// lifetime of the process so that span lookups remain valid.
struct OwnedTable {
  scoped_refptr<base::RefCountedMemory> bytes;
  std::optional<UsbIdsTable> table;
};

// Returns a pointer to the parsed table, or nullptr if the resource is
// missing or malformed. Lazily initialized on first call; thread-safe via
// C++11 function-local static initialization guarantees and held in a
// NoDestructor to avoid an exit-time destructor.
const UsbIdsTable* GetTable() {
  static base::NoDestructor<OwnedTable> owned([] {
    OwnedTable o;
    o.bytes = ui::ResourceBundle::GetSharedInstance().LoadDataResourceBytes(
        IDR_USB_IDS_DATA);
    if (o.bytes) {
      o.table = UsbIdsTable::Parse(*o.bytes);
    }
    return o;
  }());
  return owned->table.has_value() ? &owned->table.value() : nullptr;
}

}  // namespace

// static
const char* UsbIds::GetVendorName(uint16_t vendor_id) {
  const UsbIdsTable* table = GetTable();
  return table ? table->GetVendorName(vendor_id) : nullptr;
}

// static
UsbIdNames UsbIds::GetVendorAndProductName(uint16_t vendor_id,
                                           uint16_t product_id) {
  const UsbIdsTable* table = GetTable();
  return table ? table->GetVendorAndProductName(vendor_id, product_id)
               : UsbIdNames{};
}

}  // namespace device
