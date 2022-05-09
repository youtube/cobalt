// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/file.h"

#include <windows.h>

#include "starboard/common/log.h"
#include "starboard/shared/win32/file_internal.h"
#include "starboard/shared/win32/time_utils.h"

bool SbFileGetInfo(SbFile file, SbFileInfo* out_info) {
  if (!starboard::shared::win32::HasValidHandle(file) || !out_info) {
    return false;
  }

  FILE_BASIC_INFO basic_info = {0};
  BOOL basic_info_success = GetFileInformationByHandleEx(
      file->file_handle, FileBasicInfo, &basic_info, sizeof(FILE_BASIC_INFO));
  if (!basic_info_success) {
    return false;
  }

  FILE_STANDARD_INFO standard_info = {0};
  BOOL standard_info_success =
      GetFileInformationByHandleEx(file->file_handle, FileStandardInfo,
                                   &standard_info, sizeof(FILE_STANDARD_INFO));
  if (!standard_info_success) {
    return false;
  }

  out_info->size = standard_info.EndOfFile.QuadPart;
  SB_DCHECK(out_info->size >= 0);

  using starboard::shared::win32::ConvertFileTimeTicksToSbTime;

  out_info->creation_time =
      ConvertFileTimeTicksToSbTime(basic_info.CreationTime);
  out_info->last_accessed =
      ConvertFileTimeTicksToSbTime(basic_info.LastAccessTime);
  out_info->last_modified =
      ConvertFileTimeTicksToSbTime(basic_info.LastWriteTime);

  out_info->is_symbolic_link = false;
  out_info->is_directory = standard_info.Directory;

  return true;
}
