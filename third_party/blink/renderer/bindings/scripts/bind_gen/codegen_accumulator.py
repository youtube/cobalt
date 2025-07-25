# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


class CodeGenAccumulator(object):
    """
    Accumulates a variety of information and helps generate code based on the
    information.
    """

    def __init__(self):
        # Headers of non-standard library to be included
        self._include_headers = set()
        # Headers of C++ standard library to be included
        self._stdcpp_include_headers = set()
        # Forward declarations of C++ class
        self._class_decls = set()
        # Forward declarations of C++ struct
        self._struct_decls = set()

    def total_size(self):
        return (len(self.include_headers) + len(self.class_decls) +
                len(self.struct_decls) + len(self.stdcpp_include_headers))

    @property
    def include_headers(self):
        return self._include_headers

    def add_include_headers(self, headers):
        self._include_headers.update(filter(None, headers))

    @staticmethod
    def require_include_headers(headers):
        return lambda accumulator: accumulator.add_include_headers(headers)

    @property
    def stdcpp_include_headers(self):
        return self._stdcpp_include_headers

    def add_stdcpp_include_headers(self, headers):
        self._stdcpp_include_headers.update(filter(None, headers))

    @staticmethod
    def require_stdcpp_include_headers(headers):
        return lambda accumulator: accumulator.add_stdcpp_include_headers(
            headers)

    @property
    def class_decls(self):
        return self._class_decls

    def add_class_decls(self, class_names):
        self._class_decls.update(filter(None, class_names))

    @staticmethod
    def require_class_decls(class_names):
        return lambda accumulator: accumulator.add_class_decls(class_names)

    @property
    def struct_decls(self):
        return self._struct_decls

    def add_struct_decls(self, struct_names):
        self._struct_decls.update(filter(None, struct_names))

    @staticmethod
    def require_struct_decls(struct_names):
        return lambda accumulator: accumulator.add_struct_decls(struct_names)
