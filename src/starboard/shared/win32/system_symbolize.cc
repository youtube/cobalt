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

#include "starboard/system.h"

#ifndef COBALT_BUILD_TYPE_GOLD

#include <functional>
#include <string>

#include <Windows.h>  // Must go before dbhelp.h
#include <dbghelp.h>

#include "starboard/log.h"
#include "starboard/memory.h"
#include "starboard/mutex.h"
#include "starboard/once.h"
#include "starboard/string.h"

namespace {

typedef std::function<bool(const void*, std::string*)> SymbolResolverFunction;

// Handles dynamically loading functions from dbghelp.dll used
// for symbol resolution.
SymbolResolverFunction CreateWin32SymbolResolver() {
  SymbolResolverFunction null_function =
    [] (const void*, std::string*) {
      return false;
    };

  if (!::SymInitialize(GetCurrentProcess(), NULL, TRUE)) {
    auto error = GetLastError();
    SB_LOG(WARNING) << "SymInitialize returned error : " << error << "\n";
    return null_function;
  }

  ::SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS);
  return [=] (const void* address, std::string* destination) {
    char sym_buf[sizeof(SYMBOL_INFO) + MAX_SYM_NAME * sizeof(TCHAR)];
    SYMBOL_INFO* symbol = reinterpret_cast<SYMBOL_INFO*>(sym_buf);
    symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
    symbol->MaxNameLen = MAX_SYM_NAME;

    DWORD64 offset = 0;
    bool ok = ::SymFromAddr(::GetCurrentProcess(),
                            reinterpret_cast<DWORD64>(address),
                            &offset, symbol);

    const char* name = symbol->Name;
    if (name[0] == '_') {
      name++;
    }
    *destination = name;
    return ok;
  };
}

class SymbolResolver {
 public:
  static SymbolResolver* Instance();

  bool Resolve(const void* address, char* out_buffer, int buffer_size) {
    std::string symbol;

    starboard::ScopedLock lock(mutex_);
    if (resolver_(address, &symbol)) {
      SbStringCopy(out_buffer, symbol.c_str(), buffer_size);
      return true;
    } else {
      return false;
    }
  }

 private:
  SymbolResolver() {
    resolver_ = CreateWin32SymbolResolver();
  }
  starboard::Mutex mutex_;
  SymbolResolverFunction resolver_;
};

SB_ONCE_INITIALIZE_FUNCTION(SymbolResolver, SymbolResolver::Instance);

}  // namespace

bool SbSystemSymbolize(const void* address, char* out_buffer,
                       int buffer_size) {
  SymbolResolver* db = SymbolResolver::Instance();
  bool ok = db->Resolve(address, out_buffer, buffer_size);
  return ok;
}

#else  // COBALT_BUILD_TYPE_GOLD

bool SbSystemSymbolize(const void*, char*,int) {
  return false;
}

#endif
