import os

cc_file = "third_party/blink/renderer/platform/wtf/allocator/partitions.cc"

with open(cc_file, "r") as f:
    content = f.read()

# Revert my old changes:
# 1. remove kCobaltMergeBlinkPartitions definition
content = content.replace("""#if BUILDFLAG(IS_COBALT)
BASE_FEATURE(kCobaltMergeBlinkPartitions,
             "CobaltMergeBlinkPartitions",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

""", "")

# 2. remove the old InitializeOnce changes
old_init = """#if BUILDFLAG(IS_COBALT)
  const bool merge_partitions = base::FeatureList::IsEnabled(kCobaltMergeBlinkPartitions);
  if (merge_partitions) {
#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
    fast_malloc_root_ = allocator_shim::internal::PartitionAllocMalloc::Allocator();
#else
    options.thread_cache = PartitionOptions::kEnabled;
    static base::NoDestructor<partition_alloc::PartitionAllocator>
        fast_malloc_allocator(options);
    fast_malloc_root_ = fast_malloc_allocator->root();
#endif // PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
    buffer_root_ = fast_malloc_root_;
  } else {
    static base::NoDestructor<partition_alloc::PartitionAllocator>
        buffer_allocator(options);
    buffer_root_ = buffer_allocator->root();
  }
#else
  static base::NoDestructor<partition_alloc::PartitionAllocator>
      buffer_allocator(options);
  buffer_root_ = buffer_allocator->root();
  if (base::FeatureList::IsEnabled(
          kBlinkUseLargeEmptySlotSpanRingForBufferRoot)) {
    buffer_root_->EnableLargeEmptySlotSpanRing();
  }
#endif // BUILDFLAG(IS_COBALT)"""

new_init = """#if BUILDFLAG(IS_COBALT)
  // Cobalt explicitly merges array_buffer_root_ and buffer_root_ to reduce memory footprint.
  // To avoid ARM alignment crashes and thread cache pollution, we disable BRP and thread cache.
  options.backup_ref_ptr = PartitionOptions::kDisabled;
  options.thread_cache = PartitionOptions::kDisabled;
#endif

  static base::NoDestructor<partition_alloc::PartitionAllocator>
      buffer_allocator(options);
  buffer_root_ = buffer_allocator->root();
  if (base::FeatureList::IsEnabled(
          kBlinkUseLargeEmptySlotSpanRingForBufferRoot)) {
    buffer_root_->EnableLargeEmptySlotSpanRing();
  }"""
content = content.replace(old_init, new_init)

# 3. remove the old merge_partitions blocks in InitializeOnce
old_fast_malloc = """#if BUILDFLAG(IS_COBALT)
  if (!merge_partitions) {
#if !PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
    options.thread_cache = PartitionOptions::kEnabled;
    static base::NoDestructor<partition_alloc::PartitionAllocator>
        fast_malloc_allocator(options);
    fast_malloc_root_ = fast_malloc_allocator->root();
#endif
  }
#else
#if !PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  options.thread_cache = PartitionOptions::kEnabled;
  static base::NoDestructor<partition_alloc::PartitionAllocator>
      fast_malloc_allocator(options);
  fast_malloc_root_ = fast_malloc_allocator->root();
#endif
#endif // BUILDFLAG(IS_COBALT)"""

new_fast_malloc = """#if !PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  options.thread_cache = PartitionOptions::kEnabled;
  static base::NoDestructor<partition_alloc::PartitionAllocator>
      fast_malloc_allocator(options);
  fast_malloc_root_ = fast_malloc_allocator->root();
#endif"""
content = content.replace(old_fast_malloc, new_fast_malloc)

# 4. InitializeArrayBufferPartition
old_array_buffer = """#if BUILDFLAG(IS_COBALT)
  if (base::FeatureList::IsEnabled(kCobaltMergeBlinkPartitions)) {
    array_buffer_root_ = fast_malloc_root_;
    return;
  }
#endif"""

new_array_buffer = """#if BUILDFLAG(IS_COBALT)
  // Merge array_buffer_root_ with buffer_root_
  array_buffer_root_ = buffer_root_;
  return;
#endif"""
content = content.replace(old_array_buffer, new_array_buffer)

# 5. DumpMemoryStats
old_dump = """#if BUILDFLAG(IS_COBALT)
  if (!base::FeatureList::IsEnabled(kCobaltMergeBlinkPartitions)) {
    if (ArrayBufferPartitionInitialized()) {
      ArrayBufferPartition()->DumpStats("array_buffer", is_light_dump,
                                        partition_stats_dumper);
    }
    BufferPartition()->DumpStats("buffer", is_light_dump, partition_stats_dumper);
  }
#else
  if (ArrayBufferPartitionInitialized()) {
    ArrayBufferPartition()->DumpStats("array_buffer", is_light_dump,
                                      partition_stats_dumper);
  }
  BufferPartition()->DumpStats("buffer", is_light_dump, partition_stats_dumper);
#endif"""

new_dump = """  if (ArrayBufferPartitionInitialized()) {
    ArrayBufferPartition()->DumpStats("array_buffer", is_light_dump,
                                      partition_stats_dumper);
  }
#if BUILDFLAG(IS_COBALT)
  // buffer_root_ is the same as array_buffer_root_, don't dump twice
#else
  BufferPartition()->DumpStats("buffer", is_light_dump, partition_stats_dumper);
#endif"""
content = content.replace(old_dump, new_dump)

# 6. TotalSizeOfCommittedPages
old_total_size = """#if BUILDFLAG(IS_COBALT)
  if (!base::FeatureList::IsEnabled(kCobaltMergeBlinkPartitions)) {
    if (ArrayBufferPartitionInitialized()) {
      total_size += TS_UNCHECKED_READ(
          ArrayBufferPartition()->total_size_of_committed_pages);
    }
    total_size +=
        TS_UNCHECKED_READ(BufferPartition()->total_size_of_committed_pages);
  }
#else
  if (ArrayBufferPartitionInitialized()) {
    total_size += TS_UNCHECKED_READ(
        ArrayBufferPartition()->total_size_of_committed_pages);
  }
  total_size +=
      TS_UNCHECKED_READ(BufferPartition()->total_size_of_committed_pages);
#endif"""

new_total_size = """  if (ArrayBufferPartitionInitialized()) {
    total_size += TS_UNCHECKED_READ(
        ArrayBufferPartition()->total_size_of_committed_pages);
  }
#if BUILDFLAG(IS_COBALT)
  // buffer_root_ is the same as array_buffer_root_, don't double count
#else
  total_size +=
      TS_UNCHECKED_READ(BufferPartition()->total_size_of_committed_pages);
#endif"""
content = content.replace(old_total_size, new_total_size)

# 7. AdjustPartitionsForForeground
old_adjust = """#if BUILDFLAG(IS_COBALT)
    if (base::FeatureList::IsEnabled(kCobaltMergeBlinkPartitions)) {
      if (fast_malloc_root_) {
        fast_malloc_root_->AdjustForForeground();
      }
    } else {
      array_buffer_root_->AdjustForForeground();
      buffer_root_->AdjustForForeground();
      if (fast_malloc_root_) {
        fast_malloc_root_->AdjustForForeground();
      }
    }
#else
    array_buffer_root_->AdjustForForeground();
    buffer_root_->AdjustForForeground();
    if (fast_malloc_root_) {
      fast_malloc_root_->AdjustForForeground();
    }
#endif"""

new_adjust = """    array_buffer_root_->AdjustForForeground();
#if !BUILDFLAG(IS_COBALT)
    buffer_root_->AdjustForForeground();
#endif
    if (fast_malloc_root_) {
      fast_malloc_root_->AdjustForForeground();
    }"""
content = content.replace(old_adjust, new_adjust)

# 8. AdjustPartitionsForBackground
old_bg_adjust = """#if BUILDFLAG(IS_COBALT)
    if (base::FeatureList::IsEnabled(kCobaltMergeBlinkPartitions)) {
      if (fast_malloc_root_) {
        fast_malloc_root_->AdjustForBackground();
      }
    } else {
      array_buffer_root_->AdjustForBackground();
      buffer_root_->AdjustForBackground();
      if (fast_malloc_root_) {
        fast_malloc_root_->AdjustForBackground();
      }
    }
#else
    array_buffer_root_->AdjustForBackground();
    buffer_root_->AdjustForBackground();
    if (fast_malloc_root_) {
      fast_malloc_root_->AdjustForBackground();
    }
#endif"""

new_bg_adjust = """    array_buffer_root_->AdjustForBackground();
#if !BUILDFLAG(IS_COBALT)
    buffer_root_->AdjustForBackground();
#endif
    if (fast_malloc_root_) {
      fast_malloc_root_->AdjustForBackground();
    }"""
content = content.replace(old_bg_adjust, new_bg_adjust)

with open(cc_file, "w") as f:
    f.write(content)
