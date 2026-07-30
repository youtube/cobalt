// Copyright 2026 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// This file implements an override for the standard C library function
// dl_iterate_phdr().
//
// ============================================================================
// What is dl_iterate_phdr()?
// ============================================================================
// On Linux systems, programs and shared libraries (.so files) are organized in
// the Executable and Linkable Format (ELF). When an application runs, the
// operating system's dynamic linker (glibc's ld.so) loads shared libraries into
// memory and keeps an internal list of all loaded libraries.
//
// dl_iterate_phdr() is a standard glibc API (declared in <link.h>) that allows
// an application to traverse this list. It takes a callback function and passes
// it information about each loaded library via a "struct dl_phdr_info",
// including:
//   1. dlpi_addr: The base virtual memory address where the library is mapped.
//   2. dlpi_name: The file path of the library on disk.
//   3. dlpi_phdr: A pointer to the array of ELF program headers (which describe
//                 the memory segments, such as executable code or data).
//   4. dlpi_phnum: The number of program headers in the array.
//
// ============================================================================
// What external functionality interacts with dl_iterate_phdr()?
// ============================================================================
// Debuggers, profilers, crash reporters, and stack-unwinding tools rely on
// dl_iterate_phdr() to translate raw memory addresses into human-readable code
// locations (function names, source file paths, and line numbers):
//
// 1. AddressSanitizer (ASan):
//    When a memory error (such as a use-after-free or buffer overflow) occurs,
//    ASan catches the crash signal and generates a backtrace. To determine
//    which library contains each instruction pointer in the stack, ASan calls
//    dl_iterate_phdr() to match addresses against the loaded segments.
//
// 2. Stack Unwinders & Symbolizers (e.g., llvm-symbolizer):
//    Once the library path and relative offset are known, symbolizers inspect
//    the library's ELF symbols and DWARF debug info to determine the exact
//    source file and line number.
//
// ============================================================================
// Why does Cobalt Evergreen need this override?
// ============================================================================
// Standard Linux applications load shared libraries using the system dynamic
// linker via dlopen().
//
// Cobalt Evergreen, however, uses its own custom ELF loader
// (starboard/elf_loader) to load and execute libcobalt.so directly in memory
// from package storage without calling dlopen(). Because the system dynamic
// linker is bypassed, glibc's internal library list does not contain
// libcobalt.so.
//
// Consequently, when ASan or another tool calls dl_iterate_phdr() during a
// crash inside libcobalt.so, glibc only reports the host system libraries and
// the loader executable (loader_app), omitting libcobalt.so entirely. This
// causes ASan stack traces to display "(<unknown module>)" instead of the
// library name and function offsets.
//
// ============================================================================
// How this override works
// ============================================================================
// 1. Symbol Interposition:
//    By defining dl_iterate_phdr() in the loader executable (loader_app or
//    elf_loader_sandbox), this implementation intercepts all calls made to
//    dl_iterate_phdr() within the process, including calls from ASan.
//
// 2. Chaining to Host glibc:
//    The override first calls the real glibc dl_iterate_phdr() to enumerate all
//    standard system libraries (e.g., libc.so, libm.so). If the caller's
//    callback requests early termination (by returning a non-zero value), that
//    value is returned immediately.
//
// 3. Appending the Evergreen Library:
//    If host iteration finishes and Cobalt's EvergreenInfo metadata is
//    available (registered by the ELF loader when libcobalt.so was mapped), the
//    override constructs a synthetic "struct dl_phdr_info" on the stack frame
//    and invokes the callback for libcobalt.so.
//
// 4. Async-Signal Safety & Zero Heap Allocations:
//    Crash handlers (including ASan) invoke dl_iterate_phdr() from signal
//    handlers. In a signal handler, heap memory allocation (malloc, new) and
//    mutex locking are unsafe because the crash may have interrupted an
//    allocator or lock in an inconsistent state. This override:
//      - Resolves the real dl_iterate_phdr pointer at startup via a constructor
//        attribute, avoiding lazy resolution during a crash.
//      - Uses lock-free atomic pointer reads.
//      - Constructs all synthetic metadata strictly on the local stack frame.
//      - Uses SB_NO_SANITIZE_ADDRESS to prevent recursive sanitizer calls.

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <dlfcn.h>
#include <link.h>
#include <string.h>

#include <atomic>

#include "starboard/elf_loader/evergreen_info.h"
#include "starboard/export.h"

#if defined(__clang__) || defined(__GNUC__)
#define SB_NO_SANITIZE_ADDRESS __attribute__((no_sanitize("address")))
#else
#define SB_NO_SANITIZE_ADDRESS
#endif

namespace {

// Function pointer signature matching the standard glibc dl_iterate_phdr().
typedef int (*DlIteratePhdrFn)(int (*callback)(struct dl_phdr_info* info,
                                               size_t size,
                                               void* data),
                               void* data);

// No-op function whose address is used as a sentinel when dlsym() returns
// nullptr. Storing a valid function pointer distinguishes a completed lookup
// that found no symbol from an uninitialized state (nullptr) without casting
// an integer to a function pointer.
SB_NO_SANITIZE_ADDRESS int LookupFailedSentinel(
    int (*callback)(struct dl_phdr_info* info, size_t size, void* data),
    void* data) {
  return 0;
}

// Stores the pointer to glibc's original dl_iterate_phdr() function, or
// &LookupFailedSentinel if lookup returned nullptr. Stored as an atomic pointer
// so it can be safely read from any thread without requiring mutex locks.
std::atomic<DlIteratePhdrFn> g_real_dl_iterate_phdr{nullptr};

// Resolves glibc's original dl_iterate_phdr() implementation using dlsym()
// with RTLD_NEXT. RTLD_NEXT instructs the dynamic linker to search for the
// next occurrence of the symbol in libraries loaded after the current one,
// bypassing this override and returning the real system function.
//
// Automatically runs during process startup before main() is entered.
// This guarantees that g_real_dl_iterate_phdr is initialized early and safely,
// so that if a crash occurs later, dl_iterate_phdr() will never need to call
// dlsym() from inside a crash signal handler.
__attribute__((constructor)) void InitDlIteratePhdrOverride() {
  if (g_real_dl_iterate_phdr.load(std::memory_order_relaxed) == nullptr) {
    DlIteratePhdrFn real_fn =
        reinterpret_cast<DlIteratePhdrFn>(dlsym(RTLD_NEXT, "dl_iterate_phdr"));
    g_real_dl_iterate_phdr.store(
        real_fn != nullptr ? real_fn : &LookupFailedSentinel,
        std::memory_order_release);
  }
}

}  // namespace

extern "C" {

// Overrides the standard dl_iterate_phdr() function.
//
// Parameters:
//   callback: A function pointer provided by the caller (such as ASan). It is
//             called once for each loaded library, receiving information about
//             that library in a "struct dl_phdr_info". If the callback returns
//             a non-zero integer, iteration stops immediately.
//   data:     An arbitrary pointer provided by the caller, passed through to
//             the callback function on each invocation.
//
// Returns:
//   0 if all libraries were visited, or the non-zero integer returned by the
//   callback that caused early termination.
SB_EXPORT_PLATFORM SB_NO_SANITIZE_ADDRESS int dl_iterate_phdr(
    int (*callback)(struct dl_phdr_info* info, size_t size, void* data),
    void* data) {
  DlIteratePhdrFn real_fn =
      g_real_dl_iterate_phdr.load(std::memory_order_acquire);
  // AddressSanitizer's __asan_init() runs in .preinit_array—before any
  // .init_array (__attribute__((constructor))) functions execute—and calls
  // dl_iterate_phdr() during process startup. Resolve the real function here
  // on that startup call so that g_real_dl_iterate_phdr is guaranteed to be
  // populated before main() and never resolved inside a crash signal handler.
  if (!real_fn) {
    InitDlIteratePhdrOverride();
    real_fn = g_real_dl_iterate_phdr.load(std::memory_order_acquire);
  }

  // Step 1: Delegate to glibc's real dl_iterate_phdr() to enumerate all
  // standard system libraries (e.g. libc, libpthread).
  int status = 0;
  if (real_fn && real_fn != &LookupFailedSentinel) {
    status = real_fn(callback, data);
    // If the callback returned a non-zero value, honor early termination.
    if (status != 0) {
      return status;
    }
  }

  // Step 2: Append the custom-loaded Evergreen library (libcobalt.so) if it
  // has been mapped into memory.
  //
  // Note: All metadata structures here are allocated strictly on the stack
  // frame (no malloc / new) to ensure async-signal safety if this function
  // was invoked by ASan from a crash handler.
  EvergreenInfo evergreen_info = {};
  if (GetEvergreenInfo(&evergreen_info) && evergreen_info.base_address != 0 &&
      evergreen_info.phdr_table != 0 && evergreen_info.phdr_table_num > 0) {
    struct dl_phdr_info info;
    memset(&info, 0, sizeof(info));
    info.dlpi_addr = static_cast<ElfW(Addr)>(evergreen_info.base_address);
    info.dlpi_name = evergreen_info.file_path_buf;
    info.dlpi_phdr =
        reinterpret_cast<const ElfW(Phdr)*>(evergreen_info.phdr_table);
    info.dlpi_phnum = static_cast<ElfW(Half)>(evergreen_info.phdr_table_num);

    status = callback(&info, sizeof(info), data);
  }

  return status;
}

}  // extern "C"
