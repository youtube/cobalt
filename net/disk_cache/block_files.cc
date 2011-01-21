// Copyright (c) 2006-2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/block_files.h"

#include "base/file_util.h"
#include "base/metrics/histogram.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/threading/thread_checker.h"
#include "base/time.h"
#include "net/disk_cache/cache_util.h"
#include "net/disk_cache/file_lock.h"
#include "net/disk_cache/trace.h"

using base::TimeTicks;

namespace {

const char* kBlockName = "data_";

// This array is used to perform a fast lookup of the nibble bit pattern to the
// type of entry that can be stored there (number of consecutive blocks).
const char s_types[16] = {4, 3, 2, 2, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0};

// Returns the type of block (number of consecutive blocks that can be stored)
// for a given nibble of the bitmap.
inline int GetMapBlockType(uint8 value) {
  value &= 0xf;
  return s_types[value];
}

void FixAllocationCounters(disk_cache::BlockFileHeader* header);

// Creates a new entry on the allocation map, updating the apropriate counters.
// target is the type of block to use (number of empty blocks), and size is the
// actual number of blocks to use.
bool CreateMapBlock(int target, int size, disk_cache::BlockFileHeader* header,
                    int* index) {
  if (target <= 0 || target > disk_cache::kMaxNumBlocks ||
      size <= 0 || size > disk_cache::kMaxNumBlocks) {
    NOTREACHED();
    return false;
  }

  TimeTicks start = TimeTicks::Now();
  // We are going to process the map on 32-block chunks (32 bits), and on every
  // chunk, iterate through the 8 nibbles where the new block can be located.
  int current = header->hints[target - 1];
  for (int i = 0; i < header->max_entries / 32; i++, current++) {
    if (current == header->max_entries / 32)
      current = 0;
    uint32 map_block = header->allocation_map[current];

    for (int j = 0; j < 8; j++, map_block >>= 4) {
      if (GetMapBlockType(map_block) != target)
        continue;

      disk_cache::FileLock lock(header);
      int index_offset = j * 4 + 4 - target;
      *index = current * 32 + index_offset;
      DCHECK_EQ(*index / 4, (*index + size - 1) / 4);
      uint32 to_add = ((1 << size) - 1) << index_offset;
      header->allocation_map[current] |= to_add;

      header->hints[target - 1] = current;
      header->empty[target - 1]--;
      DCHECK(header->empty[target - 1] >= 0);
      header->num_entries++;
      if (target != size) {
        header->empty[target - size - 1]++;
      }
      HISTOGRAM_TIMES("DiskCache.CreateBlock", TimeTicks::Now() - start);
      return true;
    }
  }

  // It is possible to have an undetected corruption (for example when the OS
  // crashes), fix it here.
  LOG(ERROR) << "Failing CreateMapBlock";
  FixAllocationCounters(header);
  return false;
}

// Deletes the block pointed by index from allocation_map, and updates the
// relevant counters on the header.
void DeleteMapBlock(int index, int size, disk_cache::BlockFileHeader* header) {
  if (size < 0 || size > disk_cache::kMaxNumBlocks) {
    NOTREACHED();
    return;
  }
  TimeTicks start = TimeTicks::Now();
  int byte_index = index / 8;
  uint8* byte_map = reinterpret_cast<uint8*>(header->allocation_map);
  uint8 map_block = byte_map[byte_index];

  if (index % 8 >= 4)
    map_block >>= 4;

  // See what type of block will be availabe after we delete this one.
  int bits_at_end = 4 - size - index % 4;
  uint8 end_mask = (0xf << (4 - bits_at_end)) & 0xf;
  bool update_counters = (map_block & end_mask) == 0;
  uint8 new_value = map_block & ~(((1 << size) - 1) << (index % 4));
  int new_type = GetMapBlockType(new_value);

  disk_cache::FileLock lock(header);
  DCHECK((((1 << size) - 1) << (index % 8)) < 0x100);
  uint8  to_clear = ((1 << size) - 1) << (index % 8);
  DCHECK((byte_map[byte_index] & to_clear) == to_clear);
  byte_map[byte_index] &= ~to_clear;

  if (update_counters) {
    if (bits_at_end)
      header->empty[bits_at_end - 1]--;
    header->empty[new_type - 1]++;
    DCHECK(header->empty[bits_at_end - 1] >= 0);
  }
  header->num_entries--;
  DCHECK(header->num_entries >= 0);
  HISTOGRAM_TIMES("DiskCache.DeleteBlock", TimeTicks::Now() - start);
}

#ifndef NDEBUG
// Returns true if the specified block is used. Note that this is a simplified
// version of DeleteMapBlock().
bool UsedMapBlock(int index, int size, disk_cache::BlockFileHeader* header) {
  if (size < 0 || size > disk_cache::kMaxNumBlocks) {
    NOTREACHED();
    return false;
  }
  int byte_index = index / 8;
  uint8* byte_map = reinterpret_cast<uint8*>(header->allocation_map);
  uint8 map_block = byte_map[byte_index];

  if (index % 8 >= 4)
    map_block >>= 4;

  DCHECK((((1 << size) - 1) << (index % 8)) < 0x100);
  uint8  to_clear = ((1 << size) - 1) << (index % 8);
  return ((byte_map[byte_index] & to_clear) == to_clear);
}
#endif  // NDEBUG

// Restores the "empty counters" and allocation hints.
void FixAllocationCounters(disk_cache::BlockFileHeader* header) {
  for (int i = 0; i < disk_cache::kMaxNumBlocks; i++) {
    header->hints[i] = 0;
    header->empty[i] = 0;
  }

  for (int i = 0; i < header->max_entries / 32; i++) {
    uint32 map_block = header->allocation_map[i];

    for (int j = 0; j < 8; j++, map_block >>= 4) {
      int type = GetMapBlockType(map_block);
      if (type)
        header->empty[type -1]++;
    }
  }
}

// Returns true if the current block file should not be used as-is to store more
// records. |block_count| is the number of blocks to allocate.
bool NeedToGrowBlockFile(const disk_cache::BlockFileHeader* header,
                         int block_count) {
  bool have_space = false;
  int empty_blocks = 0;
  for (int i = 0; i < disk_cache::kMaxNumBlocks; i++) {
    empty_blocks += header->empty[i] * (i + 1);
    if (i >= block_count - 1 && header->empty[i])
      have_space = true;
  }

  if (header->next_file && (empty_blocks < disk_cache::kMaxBlocks / 10)) {
    // This file is almost full but we already created another one, don't use
    // this file yet so that it is easier to find empty blocks when we start
    // using this file again.
    return true;
  }
  return !have_space;
}

}  // namespace

namespace disk_cache {

BlockFiles::BlockFiles(const FilePath& path)
    : init_(false), zero_buffer_(NULL), path_(path) {
}

BlockFiles::~BlockFiles() {
  if (zero_buffer_)
    delete[] zero_buffer_;
  CloseFiles();
}

bool BlockFiles::Init(bool create_files) {
  DCHECK(!init_);
  if (init_)
    return false;

  thread_checker_.reset(new base::ThreadChecker);

  block_files_.resize(kFirstAdditionalBlockFile);
  for (int i = 0; i < kFirstAdditionalBlockFile; i++) {
    if (create_files)
      if (!CreateBlockFile(i, static_cast<FileType>(i + 1), true))
        return false;

    if (!OpenBlockFile(i))
      return false;

    // Walk this chain of files removing empty ones.
    RemoveEmptyFile(static_cast<FileType>(i + 1));
  }

  init_ = true;
  return true;
}

MappedFile* BlockFiles::GetFile(Addr address) {
  DCHECK(thread_checker_->CalledOnValidThread());
  DCHECK(block_files_.size() >= 4);
  DCHECK(address.is_block_file() || !address.is_initialized());
  if (!address.is_initialized())
    return NULL;

  int file_index = address.FileNumber();
  if (static_cast<unsigned int>(file_index) >= block_files_.size() ||
      !block_files_[file_index]) {
    // We need to open the file
    if (!OpenBlockFile(file_index))
      return NULL;
  }
  DCHECK(block_files_.size() >= static_cast<unsigned int>(file_index));
  return block_files_[file_index];
}

bool BlockFiles::CreateBlock(FileType block_type, int block_count,
                             Addr* block_address) {
  DCHECK(thread_checker_->CalledOnValidThread());
  if (block_type < RANKINGS || block_type > BLOCK_4K ||
      block_count < 1 || block_count > 4)
    return false;
  if (!init_)
    return false;

  MappedFile* file = FileForNewBlock(block_type, block_count);
  if (!file)
    return false;

  BlockFileHeader* header = reinterpret_cast<BlockFileHeader*>(file->buffer());

  int target_size = 0;
  for (int i = block_count; i <= 4; i++) {
    if (header->empty[i - 1]) {
      target_size = i;
      break;
    }
  }

  DCHECK(target_size);
  int index;
  if (!CreateMapBlock(target_size, block_count, header, &index))
    return false;

  Addr address(block_type, block_count, header->this_file, index);
  block_address->set_value(address.value());
  Trace("CreateBlock 0x%x", address.value());
  return true;
}

void BlockFiles::DeleteBlock(Addr address, bool deep) {
  DCHECK(thread_checker_->CalledOnValidThread());
  if (!address.is_initialized() || address.is_separate_file())
    return;

  if (!zero_buffer_) {
    zero_buffer_ = new char[Addr::BlockSizeForFileType(BLOCK_4K) * 4];
    memset(zero_buffer_, 0, Addr::BlockSizeForFileType(BLOCK_4K) * 4);
  }
  MappedFile* file = GetFile(address);
  if (!file)
    return;

  Trace("DeleteBlock 0x%x", address.value());

  BlockFileHeader* header = reinterpret_cast<BlockFileHeader*>(file->buffer());
  DeleteMapBlock(address.start_block(), address.num_blocks(), header);

  size_t size = address.BlockSize() * address.num_blocks();
  size_t offset = address.start_block() * address.BlockSize() +
                  kBlockHeaderSize;
  if (deep)
    file->Write(zero_buffer_, size, offset);

  if (!header->num_entries) {
    // This file is now empty. Let's try to delete it.
    FileType type = Addr::RequiredFileType(header->entry_size);
    if (Addr::BlockSizeForFileType(RANKINGS) == header->entry_size)
      type = RANKINGS;
    RemoveEmptyFile(type);
  }
}

void BlockFiles::CloseFiles() {
  if (init_) {
    DCHECK(thread_checker_->CalledOnValidThread());
  }
  init_ = false;
  for (unsigned int i = 0; i < block_files_.size(); i++) {
    if (block_files_[i]) {
      block_files_[i]->Release();
      block_files_[i] = NULL;
    }
  }
  block_files_.clear();
}

void BlockFiles::ReportStats() {
  DCHECK(thread_checker_->CalledOnValidThread());
  int used_blocks[kFirstAdditionalBlockFile];
  int load[kFirstAdditionalBlockFile];
  for (int i = 0; i < kFirstAdditionalBlockFile; i++) {
    GetFileStats(i, &used_blocks[i], &load[i]);
  }
  UMA_HISTOGRAM_COUNTS("DiskCache.Blocks_0", used_blocks[0]);
  UMA_HISTOGRAM_COUNTS("DiskCache.Blocks_1", used_blocks[1]);
  UMA_HISTOGRAM_COUNTS("DiskCache.Blocks_2", used_blocks[2]);
  UMA_HISTOGRAM_COUNTS("DiskCache.Blocks_3", used_blocks[3]);

  UMA_HISTOGRAM_ENUMERATION("DiskCache.BlockLoad_0", load[0], 101);
  UMA_HISTOGRAM_ENUMERATION("DiskCache.BlockLoad_1", load[1], 101);
  UMA_HISTOGRAM_ENUMERATION("DiskCache.BlockLoad_2", load[2], 101);
  UMA_HISTOGRAM_ENUMERATION("DiskCache.BlockLoad_3", load[3], 101);
}

bool BlockFiles::IsValid(Addr address) {
#ifdef NDEBUG
  return true;
#else
  if (!address.is_initialized() || address.is_separate_file())
    return false;

  MappedFile* file = GetFile(address);
  if (!file)
    return false;

  BlockFileHeader* header = reinterpret_cast<BlockFileHeader*>(file->buffer());
  bool rv = UsedMapBlock(address.start_block(), address.num_blocks(), header);
  DCHECK(rv);

  static bool read_contents = false;
  if (read_contents) {
    scoped_array<char> buffer;
    buffer.reset(new char[Addr::BlockSizeForFileType(BLOCK_4K) * 4]);
    size_t size = address.BlockSize() * address.num_blocks();
    size_t offset = address.start_block() * address.BlockSize() +
                    kBlockHeaderSize;
    bool ok = file->Read(buffer.get(), size, offset);
    DCHECK(ok);
  }

  return rv;
#endif
}

bool BlockFiles::CreateBlockFile(int index, FileType file_type, bool force) {
  FilePath name = Name(index);
  int flags =
      force ? base::PLATFORM_FILE_CREATE_ALWAYS : base::PLATFORM_FILE_CREATE;
  flags |= base::PLATFORM_FILE_WRITE | base::PLATFORM_FILE_EXCLUSIVE_WRITE;

  scoped_refptr<File> file(new File(
      base::CreatePlatformFile(name, flags, NULL, NULL)));
  if (!file->IsValid())
    return false;

  BlockFileHeader header;
  header.entry_size = Addr::BlockSizeForFileType(file_type);
  header.this_file = static_cast<int16>(index);
  DCHECK(index <= kint16max && index >= 0);

  return file->Write(&header, sizeof(header), 0);
}

bool BlockFiles::OpenBlockFile(int index) {
  if (block_files_.size() - 1 < static_cast<unsigned int>(index)) {
    DCHECK(index > 0);
    int to_add = index - static_cast<int>(block_files_.size()) + 1;
    block_files_.resize(block_files_.size() + to_add);
  }

  FilePath name = Name(index);
  scoped_refptr<MappedFile> file(new MappedFile());

  if (!file->Init(name, kBlockHeaderSize)) {
    LOG(ERROR) << "Failed to open " << name.value();
    return false;
  }

  size_t file_len = file->GetLength();
  if (file_len < static_cast<size_t>(kBlockHeaderSize)) {
    LOG(ERROR) << "File too small " << name.value();
    return false;
  }

  BlockFileHeader* header = reinterpret_cast<BlockFileHeader*>(file->buffer());
  if (kBlockMagic != header->magic || kCurrentVersion != header->version) {
    LOG(ERROR) << "Invalid file version or magic";
    return false;
  }

  if (header->updating) {
    // Last instance was not properly shutdown.
    if (!FixBlockFileHeader(file))
      return false;
  }

  if (index == 0) {
    // Load the links file into memory with a single read.
    scoped_array<char> buf(new char[file_len]);
    if (!file->Read(buf.get(), file_len, 0))
      return false;
  }

  DCHECK(!block_files_[index]);
  file.swap(&block_files_[index]);
  return true;
}

bool BlockFiles::GrowBlockFile(MappedFile* file, BlockFileHeader* header) {
  if (kMaxBlocks == header->max_entries)
    return false;

  DCHECK(!header->empty[3]);
  int new_size = header->max_entries + 1024;
  if (new_size > kMaxBlocks)
    new_size = kMaxBlocks;

  int new_size_bytes = new_size * header->entry_size + sizeof(*header);

  FileLock lock(header);
  if (!file->SetLength(new_size_bytes)) {
    // Most likely we are trying to truncate the file, so the header is wrong.
    if (header->updating < 10 && !FixBlockFileHeader(file)) {
      // If we can't fix the file increase the lock guard so we'll pick it on
      // the next start and replace it.
      header->updating = 100;
      return false;
    }
    return (header->max_entries >= new_size);
  }

  header->empty[3] = (new_size - header->max_entries) / 4;  // 4 blocks entries
  header->max_entries = new_size;

  return true;
}

MappedFile* BlockFiles::FileForNewBlock(FileType block_type, int block_count) {
  COMPILE_ASSERT(RANKINGS == 1, invalid_fily_type);
  MappedFile* file = block_files_[block_type - 1];
  BlockFileHeader* header = reinterpret_cast<BlockFileHeader*>(file->buffer());

  TimeTicks start = TimeTicks::Now();
  while (NeedToGrowBlockFile(header, block_count)) {
    if (kMaxBlocks == header->max_entries) {
      file = NextFile(file);
      if (!file)
        return NULL;
      header = reinterpret_cast<BlockFileHeader*>(file->buffer());
      continue;
    }

    if (!GrowBlockFile(file, header))
      return NULL;
    break;
  }
  HISTOGRAM_TIMES("DiskCache.GetFileForNewBlock", TimeTicks::Now() - start);
  return file;
}

MappedFile* BlockFiles::NextFile(const MappedFile* file) {
  BlockFileHeader* header = reinterpret_cast<BlockFileHeader*>(file->buffer());
  int new_file = header->next_file;
  if (!new_file) {
    // RANKINGS is not reported as a type for small entries, but we may be
    // extending the rankings block file.
    FileType type = Addr::RequiredFileType(header->entry_size);
    if (header->entry_size == Addr::BlockSizeForFileType(RANKINGS))
      type = RANKINGS;

    new_file = CreateNextBlockFile(type);
    if (!new_file)
      return NULL;

    FileLock lock(header);
    header->next_file = new_file;
  }

  // Only the block_file argument is relevant for what we want.
  Addr address(BLOCK_256, 1, new_file, 0);
  return GetFile(address);
}

int BlockFiles::CreateNextBlockFile(FileType block_type) {
  for (int i = kFirstAdditionalBlockFile; i <= kMaxBlockFile; i++) {
    if (CreateBlockFile(i, block_type, false))
      return i;
  }
  return 0;
}

// We walk the list of files for this particular block type, deleting the ones
// that are empty.
void BlockFiles::RemoveEmptyFile(FileType block_type) {
  MappedFile* file = block_files_[block_type - 1];
  BlockFileHeader* header = reinterpret_cast<BlockFileHeader*>(file->buffer());

  while (header->next_file) {
    // Only the block_file argument is relevant for what we want.
    Addr address(BLOCK_256, 1, header->next_file, 0);
    MappedFile* next_file = GetFile(address);
    if (!next_file)
      return;

    BlockFileHeader* next_header =
        reinterpret_cast<BlockFileHeader*>(next_file->buffer());
    if (!next_header->num_entries) {
      DCHECK_EQ(next_header->entry_size, header->entry_size);
      // Delete next_file and remove it from the chain.
      int file_index = header->next_file;
      header->next_file = next_header->next_file;
      DCHECK(block_files_.size() >= static_cast<unsigned int>(file_index));

      // We get a new handle to the file and release the old one so that the
      // file gets unmmaped... so we can delete it.
      FilePath name = Name(file_index);
      scoped_refptr<File> this_file(new File(false));
      this_file->Init(name);
      block_files_[file_index]->Release();
      block_files_[file_index] = NULL;

      int failure = DeleteCacheFile(name) ? 0 : 1;
      UMA_HISTOGRAM_COUNTS("DiskCache.DeleteFailed2", failure);
      if (failure)
        LOG(ERROR) << "Failed to delete " << name.value() << " from the cache.";
      continue;
    }

    header = next_header;
    file = next_file;
  }
}

bool BlockFiles::FixBlockFileHeader(MappedFile* file) {
  BlockFileHeader* header = reinterpret_cast<BlockFileHeader*>(file->buffer());
  int file_size = static_cast<int>(file->GetLength());
  if (file_size < static_cast<int>(sizeof(*header)))
    return false;  // file_size > 2GB is also an error.

  int expected = header->entry_size * header->max_entries + sizeof(*header);
  if (file_size != expected) {
    int max_expected = header->entry_size * kMaxBlocks + sizeof(*header);
    if (file_size < expected || header->empty[3] || file_size > max_expected) {
      NOTREACHED();
      LOG(ERROR) << "Unexpected file size";
      return false;
    }
    // We were in the middle of growing the file.
    int num_entries = (file_size - sizeof(*header)) / header->entry_size;
    header->max_entries = num_entries;
  }

  FixAllocationCounters(header);
  header->updating = 0;
  return true;
}

// We are interested in the total number of blocks used by this file type, and
// the max number of blocks that we can store (reported as the percentage of
// used blocks). In order to find out the number of used blocks, we have to
// substract the empty blocks from the total blocks for each file in the chain.
void BlockFiles::GetFileStats(int index, int* used_count, int* load) {
  int max_blocks = 0;
  *used_count = 0;
  *load = 0;
  for (;;) {
    if (!block_files_[index] && !OpenBlockFile(index))
      return;

    BlockFileHeader* header =
        reinterpret_cast<BlockFileHeader*>(block_files_[index]->buffer());

    max_blocks += header->max_entries;
    int used = header->max_entries;
    for (int i = 0; i < 4; i++) {
      used -= header->empty[i] * (i + 1);
      DCHECK_GE(used, 0);
    }
    *used_count += used;

    if (!header->next_file)
      break;
    index = header->next_file;
  }
  if (max_blocks)
    *load = *used_count * 100 / max_blocks;
}

FilePath BlockFiles::Name(int index) {
  // The file format allows for 256 files.
  DCHECK(index < 256 || index >= 0);
  std::string tmp = base::StringPrintf("%s%d", kBlockName, index);
  return path_.AppendASCII(tmp);
}

}  // namespace disk_cache
