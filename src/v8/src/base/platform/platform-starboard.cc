// Copyright 2018 Google Inc. All Rights Reserved.
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

#include "src/base/lazy-instance.h"
#include "src/base/macros.h"
#include "src/base/platform/platform.h"
#include "src/base/platform/time.h"
#include "src/base/utils/random-number-generator.h"

#include "src/base/timezone-cache.h"

#include "starboard/condition_variable.h"
#include "starboard/configuration.h"
#include "starboard/memory.h"
#include "starboard/string.h"
#include "starboard/time.h"

namespace v8 {
namespace base {

#ifdef __arm__
bool OS::ArmUsingHardFloat() {
  // GCC versions 4.6 and above define __ARM_PCS or __ARM_PCS_VFP to specify
  // the Floating Point ABI used (PCS stands for Procedure Call Standard).
  // We use these as well as a couple of other defines to statically determine
  // what FP ABI used.
  // GCC versions 4.4 and below don't support hard-fp.
  // GCC versions 4.5 may support hard-fp without defining __ARM_PCS or
  // __ARM_PCS_VFP.

#define GCC_VERSION \
  (__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__)
#if GCC_VERSION >= 40600 && !defined(__clang__)
#if defined(__ARM_PCS_VFP)
  return true;
#else
  return false;
#endif

#elif GCC_VERSION < 40500 && !defined(__clang__)
  return false;

#else
#if defined(__ARM_PCS_VFP)
  return true;
#elif defined(__ARM_PCS) || defined(__SOFTFP__) || defined(__SOFTFP) || \
    !defined(__VFP_FP__)
  return false;
#else
#error \
    "Your version of compiler does not report the FP ABI compiled for."     \
       "Please report it on this issue"                                        \
       "http://code.google.com/p/v8/issues/detail?id=2140"

#endif
#endif
#undef GCC_VERSION
}
#endif  // def __arm__

namespace {

static LazyInstance<RandomNumberGenerator>::type
    platform_random_number_generator = LAZY_INSTANCE_INITIALIZER;
static LazyMutex rng_mutex = LAZY_MUTEX_INITIALIZER;

bool g_hard_abort = false;

}  // namespace

void OS::Initialize(bool hard_abort, const char* const gc_fake_mmap) {
  g_hard_abort = hard_abort;
  // This is only used on Posix, we don't need to use it for anything.
  SB_UNREFERENCED_PARAMETER(gc_fake_mmap);
}

int OS::GetUserTime(uint32_t* secs, uint32_t* usecs) {
#if SB_HAS(TIME_THREAD_NOW)
  SbTimeMonotonic thread_now = SbTimeGetMonotonicThreadNow();
  *secs = thread_now / kSbTimeSecond;
  *usecs = thread_now % kSbTimeSecond;
  return 0;
#else
  return -1;
#endif
}

double OS::TimeCurrentMillis() {
  return Time::Now().ToJsTime();
}

int OS::ActivationFrameAlignment() {
#if V8_TARGET_ARCH_ARM
  // On EABI ARM targets this is required for fp correctness in the
  // runtime system.
  return 8;
#elif V8_TARGET_ARCH_MIPS
  return 8;
#elif V8_TARGET_ARCH_S390
  return 8;
#else
  // Otherwise we just assume 16 byte alignment, i.e.:
  // - With gcc 4.4 the tree vectorization optimizer can generate code
  //   that requires 16 byte alignment such as movdqa on x86.
  // - Mac OS X, PPC and Solaris (64-bit) activation frames must
  //   be 16 byte-aligned;  see "Mac OS X ABI Function Call Guide"
  return 16;
#endif
}

// static
size_t OS::AllocatePageSize() { return SB_MEMORY_PAGE_SIZE; }

// static
size_t OS::CommitPageSize() { return SB_MEMORY_PAGE_SIZE; }

// static
void OS::SetRandomMmapSeed(int64_t seed) { SB_NOTIMPLEMENTED(); }

// static
void* OS::GetRandomMmapAddr() { return nullptr; }

void* Allocate(void* address, size_t size, OS::MemoryPermission access) {
  // Starboard has no concept of changing permissions after allocating memory,
  // so we have to allocate everything with rwx permissions and then no-op on
  // changes.  TODO: Actually use |access| once Starboard supports changing
  // permissions after allocation.
  SB_UNREFERENCED_PARAMETER(access);
  SbMemoryMapFlags sb_flags =
      SbMemoryMapFlags(kSbMemoryMapProtectReadWrite | kSbMemoryMapProtectExec);
  void* result = SbMemoryMap(size, sb_flags, "v8::Base::Allocate");
  if (result == SB_MEMORY_MAP_FAILED) {
    return nullptr;
  }
  return result;
}

// static
void* OS::Allocate(void* address, size_t size, size_t alignment,
                   MemoryPermission access) {
  size_t page_size = AllocatePageSize();
  DCHECK_EQ(0, size % page_size);
  DCHECK_EQ(0, alignment % page_size);
  address = AlignedAddress(address, alignment);
  // Add the maximum misalignment so we are guaranteed an aligned base address.
  size_t request_size = size + (alignment - page_size);
  request_size = RoundUp(request_size, OS::AllocatePageSize());
  void* result = base::Allocate(address, request_size, access);
  if (result == nullptr) return nullptr;

  // Unmap memory allocated before the aligned base address.
  uint8_t* base = static_cast<uint8_t*>(result);
  uint8_t* aligned_base = RoundUp(base, alignment);
  if (aligned_base != base) {
    DCHECK_LT(base, aligned_base);
    size_t prefix_size = static_cast<size_t>(aligned_base - base);
    CHECK(Free(base, prefix_size));
    request_size -= prefix_size;
  }
  // Unmap memory allocated after the potentially unaligned end.
  if (size != request_size) {
    DCHECK_LT(size, request_size);
    size_t suffix_size = request_size - size;
    CHECK(Free(aligned_base + size, suffix_size));
    request_size -= suffix_size;
  }

  DCHECK_EQ(size, request_size);
  return static_cast<void*>(aligned_base);
}

// static
bool OS::Free(void* address, const size_t size) {
  return SbMemoryUnmap(address, size);
}

// static
bool OS::Release(void* address, size_t size) {
  return SbMemoryUnmap(address, size);
}

// static
bool OS::SetPermissions(void* address, size_t size, MemoryPermission access) {
  return true;
}

// static
bool OS::HasLazyCommits() {
  SB_NOTIMPLEMENTED();
  return false;
}

void OS::Sleep(TimeDelta interval) { SbThreadSleep(interval.InMicroseconds()); }

void OS::Abort() { SbSystemBreakIntoDebugger(); }

void OS::DebugBreak() { SbSystemBreakIntoDebugger(); }

class StarboardMemoryMappedFile final : public OS::MemoryMappedFile {
 public:
  ~StarboardMemoryMappedFile() final;
  void* memory() const final {
    SB_NOTIMPLEMENTED();
    return nullptr;
  }
  size_t size() const final {
    SB_NOTIMPLEMENTED();
    return 0u;
  }
};

// static
OS::MemoryMappedFile* OS::MemoryMappedFile::open(const char* name) {
  SB_NOTIMPLEMENTED();
  return nullptr;
}

// static
OS::MemoryMappedFile* OS::MemoryMappedFile::create(const char* name,
                                                   size_t size, void* initial) {
  SB_NOTIMPLEMENTED();
  return nullptr;
}

StarboardMemoryMappedFile::~StarboardMemoryMappedFile() { SB_NOTIMPLEMENTED(); }

int OS::GetCurrentProcessId() {
  SB_NOTIMPLEMENTED();
  return 0;
}

int OS::GetCurrentThreadId() { return SbThreadGetId(); }

int OS::GetLastError() {
  SB_NOTIMPLEMENTED();
  return 0;
}

// ----------------------------------------------------------------------------
// POSIX stdio support.
//

FILE* OS::FOpen(const char* path, const char* mode) {
  SB_NOTIMPLEMENTED();
  return nullptr;
}

bool OS::Remove(const char* path) {
  SB_NOTIMPLEMENTED();
  return false;
}

char OS::DirectorySeparator() { return SB_FILE_SEP_CHAR; }

bool OS::isDirectorySeparator(const char ch) {
  return ch == DirectorySeparator();
}

FILE* OS::OpenTemporaryFile() {
  SB_NOTIMPLEMENTED();
  return nullptr;
}

const char* const OS::LogFileOpenMode = "\0";

void OS::Print(const char* format, ...) {
  va_list args;
  va_start(args, format);
  VPrint(format, args);
  va_end(args);
}

void OS::VPrint(const char* format, va_list args) {
  SbLogRawFormat(format, args);
}

void OS::FPrint(FILE* out, const char* format, ...) { SB_NOTIMPLEMENTED(); }

void OS::VFPrint(FILE* out, const char* format, va_list args) {
  SB_NOTIMPLEMENTED();
}

void OS::PrintError(const char* format, ...) {
  va_list args;
  va_start(args, format);
  VPrintError(format, args);
  va_end(args);
}

void OS::VPrintError(const char* format, va_list args) {
  // Starboard has no concept of stderr vs stdout.
  SbLogRawFormat(format, args);
}

int OS::SNPrintF(char* str, int length, const char* format, ...) {
  va_list args;
  va_start(args, format);
  int result = VSNPrintF(str, length, format, args);
  va_end(args);
  return result;
}

int OS::VSNPrintF(char* str, int length, const char* format, va_list args) {
  int n = vsnprintf(str, length, format, args);
  if (n < 0 || n >= length) {
    // If the length is zero, the assignment fails.
    if (length > 0) str[length - 1] = '\0';
    return -1;
  } else {
    return n;
  }
}

// ----------------------------------------------------------------------------
// POSIX string support.
//

char* OS::StrChr(char* str, int c) {
  return const_cast<char*>(SbStringFindCharacter(str, c));
}

void OS::StrNCpy(char* dest, int length, const char* src, size_t n) {
  SB_UNREFERENCED_PARAMETER(length);
  SbStringCopy(dest, src, n);
}

// ----------------------------------------------------------------------------
// POSIX thread support.
//

class Thread::PlatformData {
 public:
  PlatformData() : thread_(kSbThreadInvalid) {}
  SbThread thread_;  // Thread handle for pthread.
  // Synchronizes thread creation
  Mutex thread_creation_mutex_;
};

Thread::Thread(const Options& options)
    : data_(new PlatformData),
      stack_size_(options.stack_size()),
      start_semaphore_(nullptr) {
  set_name(options.name());
}

Thread::~Thread() {
  SB_NOTIMPLEMENTED();
  delete data_;
}

static void SetThreadName(const char* name) { SbThreadSetName(name); }

static void* ThreadEntry(void* arg) {
  Thread* thread = reinterpret_cast<Thread*>(arg);
  // We take the lock here to make sure that pthread_create finished first since
  // we don't know which thread will run first (the original thread or the new
  // one).
  { LockGuard<Mutex> lock_guard(&thread->data()->thread_creation_mutex_); }
  SetThreadName(thread->name());
  // DCHECK_NE(thread->data()->thread_, kNoThread);
  thread->NotifyStartedAndRun();

  return nullptr;
}

void Thread::set_name(const char* name) {
  strncpy(name_, name, sizeof(name_));
  name_[sizeof(name_) - 1] = '\0';
}

void Thread::Start() {
  data_->thread_ =
      SbThreadCreate(stack_size_, kSbThreadNoPriority, kSbInvalidInt, true,
                     name_, ThreadEntry, this);
}

void Thread::Join() { SbThreadJoin(data_->thread_, nullptr); }

Thread::LocalStorageKey Thread::CreateThreadLocalKey() {
  return SbThreadCreateLocalKey(nullptr);
}

void Thread::DeleteThreadLocalKey(LocalStorageKey key) {
  SbThreadDestroyLocalKey(key);
}

void* Thread::GetThreadLocal(LocalStorageKey key) {
  return SbThreadGetLocalValue(key);
}

void Thread::SetThreadLocal(LocalStorageKey key, void* value) {
  bool result = SbThreadSetLocalValue(key, value);
  DCHECK(result);
}

class StarboardTimezoneCache : public TimezoneCache {
 public:
  double DaylightSavingsOffset(double time_ms) override { return 0.0; }
  void Clear() override {}
  ~StarboardTimezoneCache() override {}

 protected:
  static const int msPerSecond = 1000;
};

class StarboardDefaultTimezoneCache : public StarboardTimezoneCache {
 public:
  const char* LocalTimezone(double time_ms) override { return nullptr; }
  double LocalTimeOffset() override { return 0.0; }

  ~StarboardDefaultTimezoneCache() override {}
};

TimezoneCache* OS::CreateTimezoneCache() {
  return new StarboardDefaultTimezoneCache();
}

std::vector<OS::SharedLibraryAddress> OS::GetSharedLibraryAddresses() {
  SB_NOTIMPLEMENTED();
  return {};
}

void OS::SignalCodeMovingGC() { SB_NOTIMPLEMENTED(); }

}  // namespace base
}  // namespace v8