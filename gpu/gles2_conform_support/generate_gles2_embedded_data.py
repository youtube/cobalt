#!/usr/bin/env python3
# Copyright 2013 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""generates files to embed the gles2 conformance test data in executable."""

from __future__ import print_function

import os
import sys
import typing

class GenerateEmbeddedFiles():
  """generates files to embed the gles2 conform test data in executable"""

  paths_to_ignore = set([
    ".",
    "..",
    ".svn",
    ".git",
    ".hg",
  ])

  extensions_to_include = set([
      ".vert",
      ".frag",
      ".test",
      ".run",
  ])

  def __init__(self, scan_dir: str, base_dir: typing.Optional[str]):
    self.scan_dir = scan_dir
    self.base_dir = base_dir
    self.count = 0;
    if self.base_dir != None:
      self.files_data_h = open(os.path.join(base_dir, "FilesDATA.h"), "wb")
      self.files_data_c = open(os.path.join(base_dir, "FilesDATA.c"), "wb")
      self.files_toc_c = open(os.path.join(base_dir, "FilesTOC.c"), "wb")

      self.files_data_h.write(b"#ifndef FilesDATA_h\n\n")
      self.files_data_h.write(b"#define FilesDATA_h\n\n");

      self.files_data_c.write(b"#include \"FilesDATA.h\"\n\n")

      self.files_toc_c.write(b"#include \"FilesTOC.h\"\n\n");
      self.files_toc_c.write(b"struct GTFVectorFileEntry tempFiles;\n\n");
      self.files_toc_c.write(b"struct FileEntry files[] = {\n");

    self.AddFiles(scan_dir)

    if self.base_dir != None:
      self.files_toc_c.write(b"\n};\n\n");
      self.files_toc_c.write(
        b"int numFileEntrys = sizeof(files) / sizeof(struct FileEntry);\n");

      self.files_data_h.write(b"\n\n#endif  // FilesDATA_h\n");

      self.files_data_c.close()
      self.files_data_h.close()
      self.files_toc_c.close()

  def AddFiles(self, scan_dir: str) -> None:
    """Scan a folder and embed the contents of files."""
    files_to_embed = os.listdir(scan_dir)
    sub_dirs = []
    for file_to_embed in files_to_embed:
      full_path = os.path.join(scan_dir, file_to_embed)
      ext = os.path.splitext(file_to_embed)[1]
      base_path = full_path[len(self.scan_dir) + 1:]
      if os.path.isdir(full_path):
        if not file_to_embed in GenerateEmbeddedFiles.paths_to_ignore:
          sub_dirs.append(full_path)
      elif ext in GenerateEmbeddedFiles.extensions_to_include:
        if self.base_dir == None:
          print(full_path.replace("\\", "/"))
        else:
          self.count += 1
          name = "_FILE_%s_%d" % (ext.upper(), self.count)
          name = name.replace(".", "_")

          self.files_data_h.write(
              ("extern const char %s[];\n" % name).encode("utf8"))
          self.files_data_c.write(
              ("const char %s[] = \n" % name).encode("utf8"))

          data = open(full_path, "r")
          lines = data.readlines();
          data.close()

          for line in lines:
            line = line.replace("\n", "")
            line = line.replace("\r", "")
            line = line.replace("\\", "\\\\")
            line = line.replace("\"", "\\\"")

            self.files_data_c.write(('"%s\\n"\n' % line).encode("utf8"))

          self.files_data_c.write(b";\n")
          self.files_toc_c.write(("\t{ \"%s\", %s, 0 },\n" % (
              base_path.replace("\\", "/"), name)).encode("utf8"))

    for sub_dir in sub_dirs:
      self.AddFiles(sub_dir)


def main(argv: typing.List[str]) -> int:
  """This is the main function."""

  if len(argv) >= 1:
    scan_dir = argv[0]
  else:
    scan_dir = '.'

  if len(argv) >= 2:
    base_dir = argv[1]
  else:
    base_dir = None

  GenerateEmbeddedFiles(scan_dir, base_dir)
  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
