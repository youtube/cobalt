/* Copyright 2007 Google Inc. All Rights Reserved.
**/

#include <windows.h>
#include "unicode/udata.h"

/*
** This function attempts to load the ICU data tables from a DLL.
** Returns 0 on failure, nonzero on success.
** This a hack job of icu_utils.cc:Initialize().  It's Chrome-specific code.
*/

#define ICU_DATA_SYMBOL "icudt" U_ICU_VERSION_SHORT "_dat"
int sqlite_shell_init_icu() {
  HMODULE module;
  FARPROC addr;
  UErrorCode err;

  wchar_t dll_name[12];
  wsprintf(dll_name, L"icudt%2S.dll", U_ICU_VERSION_SHORT);
  dll_name[11] = L'\0';
  module = LoadLibrary(dll_name);
  if (!module)
    return 0;

  addr = GetProcAddress(module, ICU_DATA_SYMBOL);
  if (!addr)
    return 0;

  err = U_ZERO_ERROR;
  udata_setCommonData(addr, &err);

  return 1;
}
