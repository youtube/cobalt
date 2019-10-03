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


"""Provides functions for symlinking on Windows."""


import ctypes
from ctypes import wintypes
import os


DWORD = wintypes.DWORD
LPCWSTR = wintypes.LPCWSTR
HANDLE = wintypes.HANDLE
LPVOID = wintypes.LPVOID
BOOL = wintypes.BOOL
USHORT = wintypes.USHORT
ULONG = wintypes.ULONG
WCHAR = wintypes.WCHAR


kernel32 = wintypes.WinDLL('kernel32')
LPDWORD = ctypes.POINTER(DWORD)
UCHAR = ctypes.c_ubyte


GetFileAttributesW = kernel32.GetFileAttributesW
GetFileAttributesW.restype = DWORD
GetFileAttributesW.argtypes = (LPCWSTR,)  # lpFileName In


INVALID_FILE_ATTRIBUTES = 0xFFFFFFFF
FILE_ATTRIBUTE_REPARSE_POINT = 0x00400


CreateFileW = kernel32.CreateFileW
CreateFileW.restype = HANDLE
CreateFileW.argtypes = (LPCWSTR,  # lpFileName In
                        DWORD,    # dwDesiredAccess In
                        DWORD,    # dwShareMode In
                        LPVOID,   # lpSecurityAttributes In_opt
                        DWORD,    # dwCreationDisposition In
                        DWORD,    # dwFlagsAndAttributes In
                        HANDLE)   # hTemplateFile In_opt


CloseHandle = kernel32.CloseHandle
CloseHandle.restype = BOOL
CloseHandle.argtypes = (HANDLE,)  # hObject In


INVALID_HANDLE_VALUE = HANDLE(-1).value
OPEN_EXISTING = 3
FILE_FLAG_BACKUP_SEMANTICS = 0x02000000
FILE_FLAG_OPEN_REPARSE_POINT = 0x00200000


DeviceIoControl = kernel32.DeviceIoControl
DeviceIoControl.restype = BOOL
DeviceIoControl.argtypes = (HANDLE,   # hDevice In
                            DWORD,    # dwIoControlCode In
                            LPVOID,   # lpInBuffer In_opt
                            DWORD,    # nInBufferSize In
                            LPVOID,   # lpOutBuffer Out_opt
                            DWORD,    # nOutBufferSize In
                            LPDWORD,  # lpBytesReturned Out_opt
                            LPVOID)   # lpOverlapped Inout_opt


FSCTL_GET_REPARSE_POINT = 0x000900A8
IO_REPARSE_TAG_MOUNT_POINT = 0xA0000003
IO_REPARSE_TAG_SYMLINK = 0xA000000C
MAXIMUM_REPARSE_DATA_BUFFER_SIZE = 0x4000
SYMBOLIC_LINK_FLAG_DIRECTORY = 0x1
# Developer Mode must be enabled in order to use the following flag.
SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE = 0x2
SYMLINK_FLAG_RELATIVE = 0x1


class GenericReparseBuffer(ctypes.Structure):
  """Win32 api data structure."""
  _fields_ = (('DataBuffer', UCHAR * 1),)


class SymbolicLinkReparseBuffer(ctypes.Structure):
  """Win32 api data structure."""

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
    return arrayt.from_address(ctypes.addressof(self) + offset).value

  @property
  def substitute_name(self):
    arrayt = WCHAR * (self.SubstituteNameLength // 2)
    offset = type(self).PathBuffer.offset + self.SubstituteNameOffset
    return arrayt.from_address(ctypes.addressof(self) + offset).value

  @property
  def is_relative_path(self):
    return bool(self.Flags & SYMLINK_FLAG_RELATIVE)


class MountPointReparseBuffer(ctypes.Structure):
  """Win32 api data structure."""
  _fields_ = (('SubstituteNameOffset', USHORT),
              ('SubstituteNameLength', USHORT),
              ('PrintNameOffset', USHORT),
              ('PrintNameLength', USHORT),
              ('PathBuffer', WCHAR * 1))

  @property
  def print_name(self):
    arrayt = WCHAR * (self.PrintNameLength // 2)
    offset = type(self).PathBuffer.offset + self.PrintNameOffset
    return arrayt.from_address(ctypes.addressof(self) + offset).value

  @property
  def substitute_name(self):
    arrayt = WCHAR * (self.SubstituteNameLength // 2)
    offset = type(self).PathBuffer.offset + self.SubstituteNameOffset
    return arrayt.from_address(ctypes.addressof(self) + offset).value


class ReparseDataBuffer(ctypes.Structure):
  """Win32 api data structure."""

  class ReparseBuffer(ctypes.Union):
    """Win32 api data structure."""
    _fields_ = (('SymbolicLinkReparseBuffer', SymbolicLinkReparseBuffer),
                ('MountPointReparseBuffer', MountPointReparseBuffer),
                ('GenericReparseBuffer', GenericReparseBuffer))
  _fields_ = (('ReparseTag', ULONG),
              ('ReparseDataLength', USHORT),
              ('Reserved', USHORT),
              ('ReparseBuffer', ReparseBuffer))
  _anonymous_ = ('ReparseBuffer',)


def _ToUnicode(s):
  return s.decode('utf-8')


_kdll = None


def _GetKernel32Dll():
  global _kdll
  if _kdll:
    return _kdll
  _kdll = ctypes.windll.LoadLibrary('kernel32.dll')
  return _kdll


def FastCreateReparseLink(from_folder, link_folder):
  """Creates a reparse link.

  Args:
    from_folder: The folder that the link will point to.
    link_folder: The path of the link to be created.

  Returns:
    None

  Raises:
    OSError: if link cannot be created
  """
  from_folder = _ToUnicode(from_folder)
  link_folder = _ToUnicode(link_folder)
  par_dir = os.path.dirname(link_folder)
  if not os.path.isdir(par_dir):
    os.makedirs(par_dir)
  kdll = _GetKernel32Dll()
  # Only supported from Windows 10 Insiders build 14972
  flags = SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE | \
          SYMBOLIC_LINK_FLAG_DIRECTORY
  ok = kdll.CreateSymbolicLinkW(link_folder, from_folder, flags)
  if not ok or not FastIsReparseLink(link_folder):
    raise OSError('Could not create sym link ' + link_folder + ' to ' +
                  from_folder)


def FastIsReparseLink(path):
  path = _ToUnicode(path)
  result = GetFileAttributesW(path)
  if result == INVALID_FILE_ATTRIBUTES:
    return False
  return bool(result & FILE_ATTRIBUTE_REPARSE_POINT)


def FastReadReparseLink(path):
  """See api docstring, above."""
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
  # Remove false positive below.
  # pylint: disable=deprecated-method
  target_buffer = ctypes.c_buffer(MAXIMUM_REPARSE_DATA_BUFFER_SIZE)
  n_bytes_returned = DWORD()
  io_result = DeviceIoControl(reparse_point_handle,
                              FSCTL_GET_REPARSE_POINT,
                              None, 0,
                              target_buffer, len(target_buffer),
                              ctypes.byref(n_bytes_returned),
                              None)
  CloseHandle(reparse_point_handle)
  if not io_result:
    return None
  rdb = ReparseDataBuffer.from_buffer(target_buffer)
  if rdb.ReparseTag == IO_REPARSE_TAG_SYMLINK:
    return rdb.SymbolicLinkReparseBuffer.print_name
  elif rdb.ReparseTag == IO_REPARSE_TAG_MOUNT_POINT:
    return rdb.MountPointReparseBuffer.print_name
  return None
