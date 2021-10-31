# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'os_include': 'starboard',
  },
  'targets': [
    {
      'target_name': 'libxml',
      'type': 'static_library',
      'sources': [
        # 'chromium/libxml_utils.h',
        # 'chromium/libxml_utils.cc',
        'linux/config.h',
        'linux/include/libxml/xmlversion.h',
        'mac/config.h',
        'mac/include/libxml/xmlversion.h',

        #'src/DOCBparser.c',
        'src/HTMLparser.c',
        'src/HTMLtree.c',

        #'src/SAX.c',
        'src/SAX2.c',
        'src/buf.c',
        'src/buf.h',

        #'src/c14n.c',
        #'src/catalog.c',
        'src/chvalid.c',

        #'src/debugXML.c',
        'src/dict.c',
        'src/elfgcchack.h',
        'src/enc.h',
        'src/encoding.c',
        'src/entities.c',
        'src/error.c',
        'src/globals.c',
        'src/hash.c',
        'src/include/libxml/DOCBparser.h',
        'src/include/libxml/HTMLparser.h',
        'src/include/libxml/HTMLtree.h',
        'src/include/libxml/SAX.h',
        'src/include/libxml/SAX2.h',
        'src/include/libxml/c14n.h',
        'src/include/libxml/catalog.h',
        'src/include/libxml/chvalid.h',
        'src/include/libxml/debugXML.h',
        'src/include/libxml/dict.h',
        'src/include/libxml/encoding.h',
        'src/include/libxml/entities.h',
        'src/include/libxml/globals.h',
        'src/include/libxml/hash.h',
        'src/include/libxml/list.h',
        'src/include/libxml/nanoftp.h',
        'src/include/libxml/nanohttp.h',
        'src/include/libxml/parser.h',
        'src/include/libxml/parserInternals.h',
        'src/include/libxml/pattern.h',
        'src/include/libxml/relaxng.h',
        'src/include/libxml/schemasInternals.h',
        'src/include/libxml/schematron.h',
        'src/include/libxml/threads.h',
        'src/include/libxml/tree.h',
        'src/include/libxml/uri.h',
        'src/include/libxml/valid.h',
        'src/include/libxml/xinclude.h',
        'src/include/libxml/xlink.h',
        'src/include/libxml/xmlIO.h',
        'src/include/libxml/xmlautomata.h',
        'src/include/libxml/xmlerror.h',
        'src/include/libxml/xmlexports.h',
        'src/include/libxml/xmlmemory.h',
        'src/include/libxml/xmlmodule.h',
        'src/include/libxml/xmlreader.h',
        'src/include/libxml/xmlregexp.h',
        'src/include/libxml/xmlsave.h',
        'src/include/libxml/xmlschemas.h',
        'src/include/libxml/xmlschemastypes.h',
        'src/include/libxml/xmlstring.h',
        'src/include/libxml/xmlunicode.h',
        'src/include/libxml/xmlwriter.h',
        'src/include/libxml/xpath.h',
        'src/include/libxml/xpathInternals.h',
        'src/include/libxml/xpointer.h',
        'src/include/win32config.h',
        'src/include/wsockcompat.h',

        #'src/legacy.c',
        'src/libxml.h',
        'src/list.c',
        'src/parser.c',
        'src/parserInternals.c',
        'src/pattern.c',

        'src/relaxng.c',
        'src/save.h',

        #'src/schematron.c',
        'src/threads.c',
        'src/timsort.h',
        'src/tree.c',
        'src/triodef.h',
        'src/trionan.h',

        #'src/trio.c',
        #'src/trio.h',
        #'src/triodef.h',
        # Note: xpath.c #includes trionan.c
        #'src/trionan.c',
        #'src/triop.h',
        #'src/triostr.c',
        #'src/triostr.h',
        'src/uri.c',
        'src/valid.c',

        #'src/xinclude.c',
        #'src/xlink.c',
        'src/xmlIO.c',
        'src/xmlmemory.c',

        #'src/xmlmodule.c',
        'src/xmlreader.c',

        'src/xmlregexp.c',
        'src/xmlsave.c',

        'src/xmlschemas.c',
        'src/xmlschemastypes.c',
        'src/xmlstring.c',
        'src/xmlunicode.c',
        'src/xmlwriter.c',
        'src/xpath.c',

        'src/xpointer.c',
        #'src/xzlib.c',
        'src/xzlib.h',
        'win32/config.h',
        'win32/include/libxml/xmlversion.h',
      ],
      'defines': [
        # Define LIBXML_STATIC as nothing to match how libxml.h
        # (an internal header) defines LIBXML_STATIC, otherwise
        # we get the macro redefined warning from GCC.  (-DFOO
        # defines the macro FOO as 1.)
        'LIBXML_STATIC=',
      ],
      'include_dirs': [
        '<(os_include)',
        '<(os_include)/include',
        'src/include',
      ],
      'dependencies': [
        '../icu/icu.gyp:icuuc',
        '../zlib/zlib.gyp:zlib',
      ],
      'export_dependent_settings': [
        '../icu/icu.gyp:icuuc',
      ],
      'direct_dependent_settings': {
        'defines': [
          'LIBXML_STATIC',
        ],
        'include_dirs': [
          '<(os_include)/include',
          'src/include',
        ],
      },
      'msvs_disabled_warnings': [
        '4018', # Signed / Unsigned mismatch.
        '4244', # Conversion from int64 to int.
        '4267', # Conversion from size_t to int.
        '4005', # Macro re-definition.
        '4047',
        '4067',
        '4101', # Unreferenced local variable.
        '4996', # Posix name for this item is deprecated.
        '4311', # Pointer truncation from 'void *' to 'long'
        '4312', # Conversion from 'long' to 'void *' of greater size
        '4477', # Format string '%lu' requires an argument of type
                # 'unsigned long', but variadic argument 1 has type 'size_t'
      ],
      'conditions': [
        ['OS=="starboard" or OS=="lb_shell"', {
          'dependencies!': [
            '../zlib/zlib.gyp:zlib',
          ],
        }],
        ['target_arch=="win"', {
          'product_name': 'libxml2',
        }, {  # else: OS!="win"
          'product_name': 'xml2',
        }],
        ['clang == 1', {
          'cflags': [
            # libxml passes `const unsigned char*` through `const char*`.
            '-Wno-pointer-sign',
            # pattern.c and uri.c both have an intentional
            # `for (...);` / `while(...);` loop. I submitted a patch to
            # move the `'` to its own line, but until that's landed
            # suppress the warning:
            '-Wno-empty-body',
            # debugXML.c compares array 'arg' to NULL.
            '-Wno-tautological-pointer-compare',
            # See http://crbug.com/138571#c8
            '-Wno-ignored-attributes',
            # libxml casts from int to long to void*.
            '-Wno-int-to-void-pointer-cast',
            # libxml passes a volatile LPCRITICAL_SECTION* to a function
            # expecting a void* volatile*.
            '-Wno-incompatible-pointer-types',
            # trio_is_special_quantity and trio_is_negative are only
            # used with certain preprocessor defines set.
            '-Wno-unused-function',
            # xpath.c xmlXPathNodeCollectAndTest compares 'xmlElementType'
            # and 'xmlXPathTypeVal'.
            '-Wno-enum-compare',
          ],
        }],
      ],
    },
  ],
}

