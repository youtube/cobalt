// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "base/debug/proc_maps_linux.h"

#include <fcntl.h>
#include <stddef.h>

#include "base/containers/flat_map.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/format_macros.h"
#include "base/logging.h"
#include "base/memory/page_size.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
#include <inttypes.h>
#endif

#include "base/posix/eintr_wrapper.h"

namespace base::debug {

MappedMemoryRegion::MappedMemoryRegion() = default;
MappedMemoryRegion::MappedMemoryRegion(const MappedMemoryRegion&) = default;
MappedMemoryRegion::MappedMemoryRegion(MappedMemoryRegion&&) noexcept = default;
MappedMemoryRegion& MappedMemoryRegion::operator=(MappedMemoryRegion&) =
    default;
MappedMemoryRegion& MappedMemoryRegion::operator=(
    MappedMemoryRegion&&) noexcept = default;

// Scans |proc_maps| starting from |pos| returning true if the gate VMA was
// found, otherwise returns false.
static bool ContainsGateVMA(std::string* proc_maps, size_t pos) {
#if defined(ARCH_CPU_ARM_FAMILY)
  // The gate VMA on ARM kernels is the interrupt vectors page.
  return proc_maps->find(" [vectors]\n", pos) != std::string::npos;
#elif defined(ARCH_CPU_X86_64)
  // The gate VMA on x86 64-bit kernels is the virtual system call page.
  return proc_maps->find(" [vsyscall]\n", pos) != std::string::npos;
#else
  // Otherwise assume there is no gate VMA in which case we shouldn't
  // get duplicate entires.
  return false;
#endif
}

bool ReadProcMaps(std::string* proc_maps) {
  // seq_file only writes out a page-sized amount on each call. Refer to header
  // file for details.
  const size_t read_size = static_cast<size_t>(sysconf(_SC_PAGESIZE));

  base::ScopedFD fd(HANDLE_EINTR(open("/proc/self/maps", O_RDONLY)));
  if (!fd.is_valid()) {
    DPLOG(ERROR) << "Couldn't open /proc/self/maps";
    return false;
  }
  proc_maps->clear();

  while (true) {
    // To avoid a copy, resize |proc_maps| so read() can write directly into it.
    // Compute |buffer| afterwards since resize() may reallocate.
    size_t pos = proc_maps->size();
    proc_maps->resize(pos + read_size);
    void* buffer = &(*proc_maps)[pos];

    ssize_t bytes_read = HANDLE_EINTR(read(fd.get(), buffer, read_size));
    if (bytes_read < 0) {
      DPLOG(ERROR) << "Couldn't read /proc/self/maps";
      proc_maps->clear();
      return false;
    }

    // ... and don't forget to trim off excess bytes.
    proc_maps->resize(pos + static_cast<size_t>(bytes_read));

    if (bytes_read == 0) {
      break;
    }

    // The gate VMA is handled as a special case after seq_file has finished
    // iterating through all entries in the virtual memory table.
    //
    // Unfortunately, if additional entries are added at this point in time
    // seq_file gets confused and the next call to read() will return duplicate
    // entries including the gate VMA again.
    //
    // Avoid this by searching for the gate VMA and breaking early.
    if (ContainsGateVMA(proc_maps, pos)) {
      break;
    }
  }

  return true;
}

bool ParseProcMaps(const std::string& input,
                   std::vector<MappedMemoryRegion>* regions_out) {
  CHECK(regions_out);
  std::vector<MappedMemoryRegion> regions;

  // Use SplitStringPiece to avoid heap allocations for every line.
  std::vector<std::string_view> lines = base::SplitStringPiece(
      input, "\n", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

  for (size_t i = 0; i < lines.size(); ++i) {
    // Due to splitting on '\n' the last line should be empty.
    if (i == lines.size() - 1) {
      if (!lines[i].empty()) {
        DLOG(WARNING) << "Last line not empty";
        return false;
      }
      break;
    }

    std::vector<std::string_view> tokens = base::SplitStringPiece(
        lines[i], " ", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    if (tokens.size() < 5) {
      DLOG(WARNING) << "Too few tokens for line: " << lines[i];
      return false;
    }

    MappedMemoryRegion region;

    // Parse address range: "08048000-08056000"
    std::vector<std::string_view> addresses = base::SplitStringPiece(
        tokens[0], "-", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    if (addresses.size() != 2) {
      return false;
    }
    uint64_t start, end;
    if (!base::HexStringToUInt64(addresses[0], &start) ||
        !base::HexStringToUInt64(addresses[1], &end)) {
      return false;
    }
    region.start = static_cast<uintptr_t>(start);
    region.end = static_cast<uintptr_t>(end);

    // Parse permissions: "r-xp"
    std::string_view permissions = tokens[1];
    if (permissions.size() < 4) {
      return false;
    }

    region.permissions = 0;
    if (permissions[0] == 'r') {
      region.permissions |= MappedMemoryRegion::READ;
    } else if (permissions[0] != '-') {
      return false;
    }

    if (permissions[1] == 'w') {
      region.permissions |= MappedMemoryRegion::WRITE;
    } else if (permissions[1] != '-') {
      return false;
    }

    if (permissions[2] == 'x') {
      region.permissions |= MappedMemoryRegion::EXECUTE;
    } else if (permissions[2] != '-') {
      return false;
    }

    if (permissions[3] == 'p') {
      region.permissions |= MappedMemoryRegion::PRIVATE;
    } else if (permissions[3] != 's' && permissions[3] != 'S') {
      return false;
    }

    // Parse offset: "00000000"
    uint64_t offset;
    if (!base::HexStringToUInt64(tokens[2], &offset)) {
      return false;
    }
    region.offset = offset;

    // Parse dev: "03:0c"
    std::vector<std::string_view> dev = base::SplitStringPiece(
        tokens[3], ":", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    if (dev.size() != 2) {
      return false;
    }
    uint32_t dev_major, dev_minor;
    if (!base::HexStringToUInt(dev[0], &dev_major) ||
        !base::HexStringToUInt(dev[1], &dev_minor)) {
      return false;
    }
    region.dev_major = static_cast<uint8_t>(dev_major);
    region.dev_minor = static_cast<uint8_t>(dev_minor);

    // Parse inode: "64593"
    int64_t inode;
    if (!base::StringToInt64(tokens[4], &inode)) {
      return false;
    }
    region.inode = static_cast<long>(inode);

    // Parse path: "/usr/sbin/gpm" (optional)
    if (tokens.size() >= 6) {
      // The path starts after the inode. However, it might contain spaces.
      // We find the inode token in the original line, but we must search
      // after the device token to avoid matching the same string in earlier
      // fields (e.g. if the offset is the same as the inode).
      size_t dev_pos = lines[i].find(tokens[3]);
      size_t inode_pos = lines[i].find(tokens[4], dev_pos + tokens[3].size());
      if (inode_pos != std::string_view::npos) {
        size_t path_pos =
            lines[i].find_first_not_of(' ', inode_pos + tokens[4].size());
        if (path_pos != std::string_view::npos) {
          region.path.assign(lines[i].substr(path_pos));
        }
      }
    }

    regions.push_back(std::move(region));
  }

  regions_out->swap(regions);
  return true;
}

std::optional<SmapsRollup> ParseSmapsRollup(const std::string& buffer) {
  std::vector<std::string_view> lines = base::SplitStringPiece(
      buffer, "\n", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  base::flat_map<std::string_view, size_t> tmp;
  for (const auto& line : lines) {
    std::vector<std::string_view> tokens = base::SplitStringPiece(
        line, " ", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    if (tokens.size() < 3 || tokens[2] != "kB") {
      continue;
    }

    std::string_view key = tokens[0];
    if (key.ends_with(":")) {
      key.remove_suffix(1);
    }

    if (key.empty()) {
      continue;
    }

    size_t val;
    if (base::StringToSizeT(tokens[1], &val)) {
      base::CheckedNumeric<size_t> val_bytes = val;
      val_bytes *= 1024;
      tmp[key] = val_bytes.ValueOrDefault(0);
    }
  }

  if (tmp.empty()) {
    return std::nullopt;
  }

  SmapsRollup smaps_rollup;
  smaps_rollup.rss = tmp["Rss"];
  smaps_rollup.pss = tmp["Pss"];
  smaps_rollup.pss_anon = tmp["Pss_Anon"];
  smaps_rollup.pss_file = tmp["Pss_File"];
  smaps_rollup.pss_shmem = tmp["Pss_Shmem"];
  smaps_rollup.private_dirty = tmp["Private_Dirty"];
  smaps_rollup.swap = tmp["Swap"];
  smaps_rollup.swap_pss = tmp["SwapPss"];

  return smaps_rollup;
}

std::optional<SmapsRollup> ReadAndParseSmapsRollup() {
  std::string buffer;
  if (!ReadFileToString(FilePath("/proc/self/smaps_rollup"), &buffer)) {
    return std::nullopt;
  }
  return ParseSmapsRollup(buffer);
}

std::optional<SmapsRollup> ParseSmapsRollupForTesting(
    const std::string& smaps_rollup) {
  return ParseSmapsRollup(smaps_rollup);
}

}  // namespace base::debug
