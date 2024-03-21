GWP-ASan is a sampling debug allocator, similar to ElectricFence or Page Heap,
intended to detect heap memory errors in the wild. When memory errors with a
GWP-ASan allocation occur, the process crashes and the crash handler detects
that the exception happened in a GWP-ASan allocation and adds additional debug
information about the allocation/deallocation context to the crash minidump. See
the [GWP-ASan documentation](/docs/gwp_asan.md) for more information.
