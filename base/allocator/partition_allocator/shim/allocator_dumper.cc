// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#include "base/allocator/partition_allocator/shim/allocator_dumper.h"

#define USE_BACKTRACE 0

#if USE_BACKTRACE
#include <execinfo.h>
#else
#include <unwind.h>
#define GLOG_EXPORT
#endif

#include <fcntl.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <fstream>
#include <set>
#include <string>

#include "base/logging.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread.h"
#include "base/third_party/symbolize/symbolize.h"  // nogncheck
#include "base/time/time.h"

#pragma clang diagnostic ignored "-Wexit-time-destructors"
#pragma clang diagnostic ignored "-Wshorten-64-to-32"
#pragma clang diagnostic ignored "-Wsign-conversion"
#pragma clang diagnostic ignored "-Wsign-compare"
#pragma clang diagnostic ignored "-Wunused-function"

namespace base {
namespace {

std::atomic<int> dump_enabled = -1;

#if USE_BACKTRACE
int SystemGetStack(void** out_stack, int stack_size) {
  int count = std::max(backtrace(out_stack, stack_size), 0);

  if (count < 1) {
    // No stack, for some reason.
    return count;
  }

  // We have an extra stack frame (for this very function), so let's remove it.
  for (int i = 1; i < count; ++i) {
    out_stack[i - 1] = out_stack[i];
  }

  return count - 1;
}
#else
namespace {
class CallbackContext {
 public:
  void** out_stack;
  int stack_size;
  int count;
  CallbackContext(void** out_stack, int stack_size)
      : out_stack(out_stack), stack_size(stack_size), count(0) {}
};

_Unwind_Reason_Code UnwindCallback(struct _Unwind_Context* uwc,
                                   void* user_context) {
  CallbackContext* callback_context =
      static_cast<CallbackContext*>(user_context);
  _Unwind_Ptr ip = _Unwind_GetIP(uwc);

  if (ip == 0) {
    return _URC_END_OF_STACK;
  }

  if (callback_context->count == callback_context->stack_size) {
    return _URC_END_OF_STACK;
  }

  callback_context->out_stack[callback_context->count] =
      reinterpret_cast<void*>(ip);
  callback_context->count++;
  return _URC_NO_REASON;
}
}  // namespace

int SystemGetStack(void** out_stack, int stack_size) {
  CallbackContext callback_context(out_stack, stack_size);

  _Unwind_Backtrace(UnwindCallback, &callback_context);
  return callback_context.count;
}
#endif

[[clang::optnone]]
bool SystemSymbolize(const void* address, char* out_buffer, unsigned int buffer_size) {
//  if (address == nullptr || *reinterpret_cast<const uintptr_t*>(address) == 0) {
//    return false;
//  }
  // I believe this casting-away const in the implementation is better than the
  // alternative of removing const-ness from the address parameter.
  return google::Symbolize(const_cast<void*>(address), out_buffer, buffer_size);
}

void AppendSpaceToString(char* buffer, unsigned int* offset) {
  buffer[*offset] = ' ';
  ++*offset;
}

void AppendHexValueToString(uintptr_t value, char* buffer, unsigned int* offset) {
  buffer[*offset] = '0';
  ++*offset;
  buffer[*offset] = 'x';
  ++*offset;

  for (unsigned int i = 0; i < sizeof(value) * 2; ++i) {
    static constexpr char hex[] = "0123456789abcdef";
    int bits_to_shift = static_cast<int>((sizeof(value) * 2 - 1 - i) * 4);
    buffer[*offset] = hex[(value >> bits_to_shift) & 0xf];
    ++*offset;
  }
}

void AppendDecimalValueToString(uintptr_t value, char* buffer, unsigned int* offset) {
  if (value == 0) {
    buffer[*offset] = value % 10 + '0';
    ++*offset;
    return;
  }

  char* start = buffer + *offset;

  while (value > 0) {
    buffer[*offset] = value % 10 + '0';
    ++*offset;
    value /= 10;
  }

  std::reverse(start, buffer + *offset);
}

void TryToWriteSymbol(void* address, int fd) {
  static void* addresses[1024 * 1024];  // sorted ascend
  static int number_of_addresses = 0;

  auto lower_bound = std::lower_bound(addresses, addresses + number_of_addresses, address);

  if (lower_bound == addresses + number_of_addresses || *lower_bound != address) {
    auto iter = addresses + number_of_addresses;
    while (iter != lower_bound) {
      *iter = *(iter - 1);
      --iter;
    }
    *lower_bound = address;
    ++number_of_addresses;
    CHECK_LE(number_of_addresses, 1024 * 1024);

    static char buf[16384] = "";
    unsigned int buf_offset = 0;

    AppendHexValueToString(reinterpret_cast<uintptr_t>(address), buf, &buf_offset);
    buf[buf_offset] = ' ';
    ++buf_offset;
    CHECK_EQ(write(fd, buf, buf_offset), static_cast<ssize_t>(buf_offset));

    if (SystemSymbolize(address, buf, sizeof(buf))) {
      buf_offset = static_cast<unsigned int>(strlen(buf));

      buf[buf_offset] = '\n';
      ++buf_offset;

      CHECK_EQ(write(fd, buf, buf_offset), static_cast<int>(buf_offset));
    } else {
      CHECK_EQ(write(fd, "??:0\n", 5), 5);
    }
  }
}

}  // namespace

struct ThreadParam {
  std::set<void*>* addresses;
  int* fd;
  base::Lock* lock;
  int* active_threads;
};

// This function isn't used by any shims, so we can use any features that
// allocate memory.
void TryToDumpSymbolsAndExit() {
  std::ifstream ifs("alloc.log");

  if (!ifs) {
    LOG(INFO) << "\"alloc.log\" not found, skip generating symbols.";
    return;
  }

  dump_enabled = 0;

  LOG(INFO) << "\"alloc.log\" found, loading addresses ...";

  std::set<void*> addresses;
  std::string line;
  int line_count = 0, token_count = 0;

  while (std::getline(ifs, line)) {
    TrimString(line, " \n\r", &line);
    ++line_count;
    auto tokens = SplitString(line, " ", TRIM_WHITESPACE, SPLIT_WANT_NONEMPTY);

    bool first_address = true;
    for (auto&& token : tokens) {
      // Skip if it's not an address
      if (token.find("0x") != 0) {
        continue;
      }

      // The first address is the pointer allocated or to be freed, skip
      if (first_address) {
        first_address = false;
        continue;
      }

      token.erase(token.begin(), token.begin() + 2);  // Remove "0x".
      intptr_t p = std::stoll(token, nullptr, 16);
      addresses.insert(reinterpret_cast<void*>(p));
      ++token_count;
    }
  }

  LOG(INFO) << "Processed " << line_count << " lines with " << token_count
            << " addresses, found " << addresses.size() << " unique addresses.";

  int fd = open("alloc_symbols.log", O_WRONLY|O_CREAT|O_TRUNC, 0644);
  CHECK_GE(fd, 0);

  base::Lock lock;

  int active_threads = 64;
  ThreadParam param = {&addresses, &fd, &lock, &active_threads};
  size_t total = addresses.size();

  for (int i = 0; i < 64; ++i) {
    base::Thread* thread = new base::Thread("worker");  // it leaks
    thread->Start();
    thread->task_runner()->PostTask(FROM_HERE, base::BindOnce(
      [](ThreadParam* param) {
        for (;;) {
          void* address = nullptr;

          {
            base::AutoLock auto_lock(*param->lock);
            if (param->addresses->empty()) {
              --param->active_threads;
              return;
            }
            address = *param->addresses->begin();
            param->addresses->erase(param->addresses->begin());
          }

          char symbol[16384] = "";

          if (SystemSymbolize(address, symbol, sizeof(symbol))) {
            base::AutoLock auto_lock(*param->lock);
            char buf[128];
            unsigned int buf_offset = 0;

            AppendHexValueToString(reinterpret_cast<uintptr_t>(address), buf, &buf_offset);
            buf[buf_offset] = ' ';
            ++buf_offset;

            CHECK_EQ(write(*param->fd, buf, buf_offset), static_cast<ssize_t>(buf_offset));

            unsigned int symbol_offset = static_cast<unsigned int>(strlen(symbol));
            symbol[symbol_offset] = '\n';
            ++symbol_offset;

            CHECK_EQ(write(*param->fd, symbol, symbol_offset), static_cast<ssize_t>(symbol_offset));
          } else {
            base::AutoLock auto_lock(*param->lock);
            CHECK_EQ(write(*param->fd, "??:0\n", 5), 5);
          }
        }
      }, base::Unretained(&param)
    ));
  }

  /*auto start = Time::Now();
  int written = 0;
  for (auto address : addresses) {
    ++written;
    TryToWriteSymbol(address, fd);

    LOG_IF(INFO, written % 100 == 0)
        << "Written " << written << "/" << addresses.size() << " symbols, took "
        << (Time::Now() - start).InSecondsF() << " seconds.";
  }*/

  auto start = Time::Now();
  size_t last_checked = total;
  for (;;) {
    base::AutoLock auto_lock(lock);
    if (active_threads == 0) {
      break;
    }
    if (last_checked - addresses.size() > 10000) {
      last_checked = addresses.size();
      LOG(INFO) << "Written " << total - last_checked << "/" << total << " symbols, took "
                << (Time::Now() - start).InSecondsF() << " seconds.";
    }
  }

  close(fd);

  LOG(INFO) << "Finished generating symbols, terminating ...";
  CHECK(0);
}

void DumpOperation(char type, void* pointer, ssize_t size) {
  if (dump_enabled == -1) {
    // The handling here isn't thread safe.  It's ok as the assignment happens
    // when the app only runs on one thread.
    int fd = open("alloc.log", O_RDONLY, 0644);
    if (fd >= 0) {
      close(fd);
      dump_enabled = 0;
    } else {
      dump_enabled = 1;
    }

    return;
  }

  if (dump_enabled == 0) {
    return;
  }

  if (type == 'f' && pointer == nullptr) {
    return;
  }

  static Lock lock;

  static __thread bool calling = false;

  if (calling) {
    return;
  }

  calling = true;

  AutoLock auto_lock(lock);

  static int fd = -1;

  if (fd == -1) {
    fd = open("alloc_tmp.log", O_WRONLY|O_CREAT|O_TRUNC, 0644);
    CHECK_GE(fd, 0);
  }

  static char buffer[1024 * 1024 * 16];
  static unsigned int offset = 0;

  static uintptr_t index = 0;
  ++index;

  AppendDecimalValueToString(index, buffer, &offset);
  AppendSpaceToString(buffer, &offset);

  auto now = Time::Now().ToInternalValue();
  AppendDecimalValueToString(static_cast<uintptr_t>(now), buffer, &offset);
  AppendSpaceToString(buffer, &offset);

  buffer[offset] = type;
  ++offset;
  AppendSpaceToString(buffer, &offset);

  AppendHexValueToString(reinterpret_cast<uintptr_t>(pointer), buffer, &offset);
  AppendSpaceToString(buffer, &offset);

  if (size >= 0) {
    AppendDecimalValueToString(size, buffer, &offset);
    AppendSpaceToString(buffer, &offset);
  }

  static void* stack[128];
  int num_of_entries = SystemGetStack(stack, 128);

  CHECK_LE(num_of_entries, 128);

  // Skip the first one (it's this function).
  for (int i = 1; i < num_of_entries; ++i) {
    // It's possible to dump symbols here, but it's extremely slow.
    // TryToWriteSymbol(stack[i], fd);

    AppendHexValueToString(reinterpret_cast<uintptr_t>(stack[i]), buffer, &offset);
    AppendSpaceToString(buffer, &offset);
  }

  buffer[offset] = '\n';
  ++offset;

  static bool first_write = true;

  bool write_now = first_write ? (offset >= sizeof(buffer) / 8 * 7) :
                                offset >= 256 * 1024;
  if (write_now) {
    CHECK_EQ(write(fd, buffer, offset), static_cast<ssize_t>(offset));
    offset = 0;
  }

  calling = false;
}

}  // namespace base
