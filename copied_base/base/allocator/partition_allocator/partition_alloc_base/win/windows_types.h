// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains defines and typedefs that allow popular Windows types to
// be used without the overhead of including windows.h.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_ALLOC_BASE_WIN_WINDOWS_TYPES_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_ALLOC_BASE_WIN_WINDOWS_TYPES_H_

// Needed for function prototypes.
#include <specstrings.h>

#ifdef __cplusplus
extern "C" {
#endif

// typedef and define the most commonly used Windows integer types.

typedef unsigned long DWORD;
typedef long LONG;
typedef __int64 LONGLONG;
typedef unsigned __int64 ULONGLONG;

#define VOID void
typedef char CHAR;
typedef short SHORT;
typedef long LONG;
typedef int INT;
typedef unsigned int UINT;
typedef unsigned int* PUINT;
typedef unsigned __int64 UINT64;
typedef void* LPVOID;
typedef void* PVOID;
typedef void* HANDLE;
typedef int BOOL;
typedef unsigned char BYTE;
typedef BYTE BOOLEAN;
typedef DWORD ULONG;
typedef unsigned short WORD;
typedef WORD UWORD;
typedef WORD ATOM;

// Forward declare some Windows struct/typedef sets.

typedef struct _RTL_SRWLOCK RTL_SRWLOCK;
typedef RTL_SRWLOCK SRWLOCK, *PSRWLOCK;

typedef struct _FILETIME FILETIME;

struct PA_CHROME_SRWLOCK {
  PVOID Ptr;
};

// The trailing white-spaces after this macro are required, for compatibility
// with the definition in winnt.h.
#define RTL_SRWLOCK_INIT {0}                            // NOLINT
#define SRWLOCK_INIT RTL_SRWLOCK_INIT

// clang-format on

// Define some macros needed when prototyping Windows functions.

#define DECLSPEC_IMPORT __declspec(dllimport)
#define WINBASEAPI DECLSPEC_IMPORT
#define WINAPI __stdcall

// Needed for LockImpl.
WINBASEAPI _Releases_exclusive_lock_(*SRWLock) VOID WINAPI
    ReleaseSRWLockExclusive(_Inout_ PSRWLOCK SRWLock);
WINBASEAPI BOOLEAN WINAPI TryAcquireSRWLockExclusive(_Inout_ PSRWLOCK SRWLock);

// Needed for thread_local_storage.h
WINBASEAPI LPVOID WINAPI TlsGetValue(_In_ DWORD dwTlsIndex);

WINBASEAPI BOOL WINAPI TlsSetValue(_In_ DWORD dwTlsIndex,
                                   _In_opt_ LPVOID lpTlsValue);

WINBASEAPI _Check_return_ _Post_equals_last_error_ DWORD WINAPI
    GetLastError(VOID);

WINBASEAPI VOID WINAPI SetLastError(_In_ DWORD dwErrCode);

#ifdef __cplusplus
}
#endif

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_ALLOC_BASE_WIN_WINDOWS_TYPES_H_
