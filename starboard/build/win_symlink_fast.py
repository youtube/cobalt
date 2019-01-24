#!/usr/bin/python
# Copyright 2019 The Cobalt Authors. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

################################################################################
#                                  API                                         #
################################################################################


def FastIsReparseLink(path):
  return _FastIsReparseLink(path)


def FastReadReparseLink(path):
  return _FastReadReparseLink(path)


# Note that FastCreateReparsePoint does not exist because the ctypes binding
# to CreateSymbolicLink can only be invoked from admin privileges.

################################################################################
#                                 IMPL                                         #
################################################################################


from ctypes import \
    POINTER, c_buffer, byref, addressof, c_ubyte, Structure, Union
from ctypes.wintypes import \
    DWORD, LPCWSTR, HANDLE, LPVOID, BOOL, USHORT, ULONG, WCHAR, WinError, WinDLL


kernel32 = WinDLL('kernel32')
LPDWORD = POINTER(DWORD)
UCHAR = c_ubyte


GetFileAttributesW = kernel32.GetFileAttributesW
GetFileAttributesW.restype = DWORD
GetFileAttributesW.argtypes = (LPCWSTR,) #lpFileName In


INVALID_FILE_ATTRIBUTES = 0xFFFFFFFF
FILE_ATTRIBUTE_REPARSE_POINT = 0x00400


CreateFileW = kernel32.CreateFileW
CreateFileW.restype = HANDLE
CreateFileW.argtypes = (LPCWSTR, # lpFileName In
                        DWORD,   # dwDesiredAccess In
                        DWORD,   # dwShareMode In
                        LPVOID,  # lpSecurityAttributes In_opt
                        DWORD,   # dwCreationDisposition In
                        DWORD,   # dwFlagsAndAttributes In
                        HANDLE)  # hTemplateFile In_opt


CloseHandle = kernel32.CloseHandle
CloseHandle.restype = BOOL
CloseHandle.argtypes = (HANDLE,) #hObject In


INVALID_HANDLE_VALUE = HANDLE(-1).value
OPEN_EXISTING = 3
FILE_FLAG_BACKUP_SEMANTICS = 0x02000000
FILE_FLAG_OPEN_REPARSE_POINT = 0x00200000


DeviceIoControl = kernel32.DeviceIoControl
DeviceIoControl.restype = BOOL
DeviceIoControl.argtypes = (HANDLE,  #hDevice In
                            DWORD,   #dwIoControlCode In
                            LPVOID,  #lpInBuffer In_opt
                            DWORD,   #nInBufferSize In
                            LPVOID,  #lpOutBuffer Out_opt
                            DWORD,   #nOutBufferSize In
                            LPDWORD, #lpBytesReturned Out_opt
                            LPVOID)  #lpOverlapped Inout_opt


FSCTL_GET_REPARSE_POINT = 0x000900A8
IO_REPARSE_TAG_MOUNT_POINT = 0xA0000003
IO_REPARSE_TAG_SYMLINK = 0xA000000C
MAXIMUM_REPARSE_DATA_BUFFER_SIZE = 0x4000


class GENERIC_REPARSE_BUFFER(Structure):
  _fields_ = (('DataBuffer', UCHAR * 1),)


class SYMBOLIC_LINK_REPARSE_BUFFER(Structure):
  _fields_ = (('SubstituteNameOffset', USHORT),
              ('SubstituteNameLength', USHORT),
              ('PrintNameOffset', USHORT),
              ('PrintNameLength', USHORT),
              ('Flags', ULONG),
              ('PathBuffer', WCHAR * 1))
  @property
  def print_name(self):
    arrayt = WCHAR * (self.PrintNameLength // 2)
    offset = type(self).PathBuffer.offset + self.PrintNameOffset
    return arrayt.from_address(addressof(self) + offset).value


class MOUNT_POINT_REPARSE_BUFFER(Structure):
  _fields_ = (('SubstituteNameOffset', USHORT),
              ('SubstituteNameLength', USHORT),
              ('PrintNameOffset', USHORT),
              ('PrintNameLength', USHORT),
              ('PathBuffer', WCHAR * 1))
  @property
  def print_name(self):
    arrayt = WCHAR * (self.PrintNameLength // 2)
    offset = type(self).PathBuffer.offset + self.PrintNameOffset
    return arrayt.from_address(addressof(self) + offset).value


class REPARSE_DATA_BUFFER(Structure):
  class REPARSE_BUFFER(Union):
      _fields_ = (('SymbolicLinkReparseBuffer',
                      SYMBOLIC_LINK_REPARSE_BUFFER),
                  ('MountPointReparseBuffer',
                      MOUNT_POINT_REPARSE_BUFFER),
                  ('GenericReparseBuffer',
                      GENERIC_REPARSE_BUFFER))
  _fields_ = (('ReparseTag', ULONG),
              ('ReparseDataLength', USHORT),
              ('Reserved', USHORT),
              ('ReparseBuffer', REPARSE_BUFFER))
  _anonymous_ = ('ReparseBuffer',)


def _ToUnicode(s):
  return s.decode('utf-8')

def _FastIsReparseLink(path):
  path = _ToUnicode(path)
  result = GetFileAttributesW(path)
  if result == INVALID_FILE_ATTRIBUTES:
    return False
  return bool(result & FILE_ATTRIBUTE_REPARSE_POINT)


def _FastReadReparseLink(path):
  path = _ToUnicode(path)
  reparse_point_handle = CreateFileW(path,
                                     0,
                                     0,
                                     None,
                                     OPEN_EXISTING,
                                     FILE_FLAG_OPEN_REPARSE_POINT |
                                     FILE_FLAG_BACKUP_SEMANTICS,
                                     None)
  if reparse_point_handle == INVALID_HANDLE_VALUE:
    return None
  target_buffer = c_buffer(MAXIMUM_REPARSE_DATA_BUFFER_SIZE)
  n_bytes_returned = DWORD()
  io_result = DeviceIoControl(reparse_point_handle,
                              FSCTL_GET_REPARSE_POINT,
                              None, 0,
                              target_buffer, len(target_buffer),
                              byref(n_bytes_returned),
                              None)
  CloseHandle(reparse_point_handle)
  if not io_result:
    return None
  rdb = REPARSE_DATA_BUFFER.from_buffer(target_buffer)
  if rdb.ReparseTag == IO_REPARSE_TAG_SYMLINK:
    return rdb.SymbolicLinkReparseBuffer.print_name
  elif rdb.ReparseTag == IO_REPARSE_TAG_MOUNT_POINT:
    return rdb.MountPointReparseBuffer.print_name
  return None
