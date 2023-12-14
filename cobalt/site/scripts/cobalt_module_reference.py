# Copyright 2017 The Cobalt Authors. All Rights Reserved.
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
"""Generates reference markdown documentation for Starboard modules.

The full process:
1. Preprocess the files to be Doxygen friendly (Doxygenation).
2. Run Doxygen to produce XML that describes each module.
3. Parse the XML and generate Markdown for each module.
"""

import contextlib
from io import StringIO
import logging
import os
import re
import sys
from xml.etree import ElementTree as ET

import doxygen
import environment
from markdown_writer import MarkdownWriter

_HEADER_PATTERN = r'.*\.h$'
_HEADER_RE = re.compile(_HEADER_PATTERN)

_HEADER_XML_PATTERN = r'.*h\.xml$'
_HEADER_XML_RE = re.compile(_HEADER_XML_PATTERN)

_SCRIPT_FILE = os.path.basename(__file__)
_SCRIPT_NAME, _ = os.path.splitext(_SCRIPT_FILE)

_OSS_STARBOARD_VERSIONS = [13, 14, 15]


def _strip(string_or_none):
  if string_or_none:
    return string_or_none.strip()
  return ''


def _get_files(parent_path, regexp):
  return sorted([
      os.path.realpath(os.path.join(parent_path, x))
      for x in os.listdir(parent_path)
      if regexp.match(x)
  ])


def _find_filename(compounddef_element):
  return _strip(compounddef_element.findtext('./compoundname'))


def _find_name(member_def):
  return _strip(member_def.findtext('./name'))


def _has_innertext(element, query=''):
  return bool(_find_innertext(element, query))


def _has_description(memberdef_element):
  return (_has_innertext(memberdef_element, './briefdescription') or
          _has_innertext(memberdef_element, './detaileddescription'))


def _find_innertext(element, query=''):
  if element is None:
    return ''
  if query:
    node = element.find(query)
    if node is None:
      return ''
  else:
    node = element
  return _strip(''.join(x.strip() for x in node.itertext()))


def _find_memberdefs(compounddef_element, kind):
  return compounddef_element.findall(f'./sectiondef/memberdef[@kind="{kind}"]')


def _find_documented_memberdefs(compounddef_element, kind):
  memberdef_elements = _find_memberdefs(compounddef_element, kind)
  if not memberdef_elements:
    return {}

  all_memberdefs = {_find_name(x): x for x in memberdef_elements}

  def essential(k, v):
    return _has_description(v) and not k.endswith('Private')

  return {k: v for k, v in all_memberdefs.items() if essential(k, v)}


def _find_structs(compounddef_element, xml_path):
  innerclass_elements = compounddef_element.findall('./innerclass')
  if not innerclass_elements:
    return {}

  all_struct_refids = [x.get('refid') for x in innerclass_elements]
  all_struct_elements = {}
  for refid in all_struct_refids:
    struct_xml_path = os.path.join(xml_path, refid + '.xml')
    struct_xml = ET.parse(struct_xml_path)
    struct_elements = struct_xml.findall(
        './/compounddef[@kind="struct"][compoundname]')
    for struct_element in struct_elements:
      struct_name = _find_filename(struct_element)
      all_struct_elements[struct_name] = struct_element

  def essential(k):
    return not k.endswith('Private')

  return {k: v for k, v in all_struct_elements.items() if essential(k)}


def _find_struct_names(compounddef_element, xml_path):
  return set(_find_structs(compounddef_element, xml_path).keys())


def _find_documented_memberdef_names(compounddef_element, *args):
  found = set([])
  for kind in args:
    found.update(_find_documented_memberdefs(compounddef_element, kind).keys())
  return found


def _find_member_definition(memberdef_element):
  type_element = memberdef_element.find('./type')
  type_name = _find_innertext(type_element)
  args_name = _strip(memberdef_element.findtext('./argsstring'))
  member_name = _strip(memberdef_element.findtext('./name'))
  # Doxygen does not handle structs of non-typedef'd function pointers
  # gracefully. The 'type" and "argsstring' elements are used to temporarily
  # store the information needed to be able to rebuild the full signature, e.g.:
  #
  #  void (*glEnable)(SbGlEnum cap)
  #
  #    type = 'void (*'
  #    name = 'glEnable'
  #    argsstring = ')(SbGlEnum cap)'
  #
  # When we identify these members using the end of 'type' and the beginning of
  # 'argsstring' we return the full member signature instead.
  if type_name.endswith('(*') and args_name.startswith(')('):
    return type_name + member_name + args_name
  return type_name + ' ' + member_name


def _node_to_markdown(out, node):
  text = node.text.replace('|', '`') if node.text else ''
  tail = node.tail.replace('|', '`') if node.tail else ''

  if node.tag == 'ndash':
    assert not _strip(text)
    out.text('\u2013')
  elif node.tag == 'mdash':
    assert not _strip(text)
    out.text('\u2013')
  elif node.tag == 'para':
    # Block tags should never be nested inside other blocks.
    assert not _strip(tail)
    out.paragraph()
  elif node.tag == 'bold':
    assert len(node) == 0
    out.bold(text)
  elif node.tag == 'computeroutput':
    assert len(node) == 0
    out.code(text)
  elif node.tag == 'ulink':
    url = node.get('url')
    assert url
    out.link(url)
  elif node.tag == 'orderedlist':
    # List tags should never have any text of their own.
    assert not _strip(text)

    # Block tags should never be nested inside other blocks.
    assert not _strip(tail)
    out.ordered_list()
  elif node.tag == 'itemizedlist':
    # List tags should never have any text of their own.
    assert not _strip(text)

    # Block tags should never be nested inside other blocks.
    assert not _strip(tail)
    out.unordered_list()
  elif node.tag == 'listitem':
    out.item()
  elif node.tag == 'heading':
    # Block tags should never be nested inside other blocks.
    assert not _strip(tail)
    try:
      levels = int(node.get('level'))
    except ValueError:
      levels = 1
    out.heading(levels=levels)
  elif node.tag == 'verbatim':
    # Verbatim tags can appear inside paragraphs.
    assert len(node) == 0
    # Don't replace pipes in verbatim text.
    text = node.text if node.text else ''
    out.code_block(text)
    text = ''
  else:
    logging.warning('UNHANDLED: %s: %s', node.tag, text.replace('\n', '\\n'))

  if text:
    out.text(text)

  for child in node:
    _node_to_markdown(out, child)

  if node.tag == 'para':
    out.end_paragraph()
  elif node.tag == 'ulink':
    out.end_link()
  elif node.tag in ['orderedlist', 'itemizedlist']:
    out.end_list()
  elif node.tag == 'heading':
    out.end_heading()
    out.pop_heading_level()
  elif node.tag == 'listitem':
    out.end_item()

  if tail:
    out.text(tail)


def _description_to_markdown(out, description_element):
  if description_element is None:
    return

  for child in description_element:
    _node_to_markdown(out, child)


def _emit_doc_header(out_io):
  out_io.write('Project: /youtube/cobalt/_project.yaml\n')
  out_io.write('Book: /youtube/cobalt/_book.yaml\n\n')


def _emit_description(out, memberdef_element):
  _description_to_markdown(out, memberdef_element.find('./briefdescription'))
  _description_to_markdown(out, memberdef_element.find('./detaileddescription'))


def _emit_macro(out, memberdef_element):
  name = _find_name(memberdef_element)
  assert name or _has_description(memberdef_element)

  params = ''
  param_defs = memberdef_element.findall('./param/defname')
  if param_defs:
    param_names = [_strip(x.text) for x in param_defs]
    params = f'({", ".join(param_names)})'

  logging.info('Macro: %s%s', name, params)
  with out.auto_scoped_heading(name + params):
    _emit_description(out, memberdef_element)


def _emit_enum(out, memberdef_element):
  name = _find_name(memberdef_element)
  assert name or _has_description(memberdef_element)

  logging.info('Enum: %s', name)
  with out.auto_scoped_heading(name):
    _emit_description(out, memberdef_element)
    with out.auto_scoped_heading('Values'):
      with out.auto_unordered_list():
        for enumvalue_element in memberdef_element.findall('./enumvalue'):
          with out.auto_item():
            out.code(_find_name(enumvalue_element))
            _emit_description(out, enumvalue_element)


def _emit_typedef(out, memberdef_element):
  name = _find_name(memberdef_element)
  assert name or _has_description(memberdef_element)

  with out.auto_scoped_heading(name):
    _emit_description(out, memberdef_element)
    definition = _strip(memberdef_element.findtext('./definition'))
    if definition:
      with out.auto_scoped_heading('Definition'):
        out.code_block(definition)


def _emit_struct(out, compounddef_element):
  name = _find_filename(compounddef_element)
  assert name or _has_description(compounddef_element)

  logging.info('Struct: %s', name)
  with out.auto_scoped_heading(name):
    _emit_description(out, compounddef_element)
    memberdef_elements = _find_memberdefs(compounddef_element, 'variable')
    if memberdef_elements:
      with out.auto_scoped_heading('Members'):
        with out.auto_unordered_list():
          for memberdef_element in memberdef_elements:
            with out.auto_item():
              out.code(_find_member_definition(memberdef_element))
              _emit_description(out, memberdef_element)


def _emit_variable(out, memberdef_element):
  name = _find_name(memberdef_element)
  assert name or _has_description(memberdef_element)

  logging.info('Variable: %s', name)
  with out.auto_scoped_heading(name):
    _emit_description(out, memberdef_element)


def _emit_function(out, memberdef_element):
  name = _find_name(memberdef_element)
  assert name or _has_description(memberdef_element)

  logging.info('Function: %s', name)
  with out.auto_scoped_heading(name):
    _emit_description(out, memberdef_element)
    prototype = memberdef_element.findtext('./definition') + \
                memberdef_element.findtext('./argsstring')
    if prototype:
      with out.auto_scoped_heading('Declaration'):
        out.code_block(prototype)


def _emit_macros(out, compounddef_element):
  member_dict = _find_documented_memberdefs(compounddef_element, 'define')
  if not member_dict:
    return False

  with out.auto_scoped_heading('Macros'):
    for name in sorted(member_dict.keys()):
      _emit_macro(out, member_dict[name])
  return True


def _emit_enums(out, compounddef_element):
  member_dict = _find_documented_memberdefs(compounddef_element, 'enum')
  if not member_dict:
    return False

  with out.auto_scoped_heading('Enums'):
    for name in sorted(member_dict.keys()):
      _emit_enum(out, member_dict[name])
  return True


def _emit_typedefs(out, compounddef_element, xml_path):
  member_dict = _find_documented_memberdefs(compounddef_element, 'typedef')
  redundant_set = _find_documented_memberdef_names(compounddef_element,
                                                   'define', 'enum', 'function')
  redundant_set |= _find_struct_names(compounddef_element, xml_path)
  essential_set = set(member_dict.keys()) - redundant_set
  if not essential_set:
    return False

  with out.auto_scoped_heading('Typedefs'):
    for name in sorted(essential_set):
      _emit_typedef(out, member_dict[name])
  return True


def _emit_structs(out, compounddef_element, xml_path):
  struct_dict = _find_structs(compounddef_element, xml_path)
  if not struct_dict:
    return False

  with out.auto_scoped_heading('Structs'):
    for name in sorted(struct_dict.keys()):
      _emit_struct(out, struct_dict[name])
  return True


def _emit_variables(out, compounddef_element):
  member_dict = _find_documented_memberdefs(compounddef_element, 'variable')
  if not member_dict:
    return False

  with out.auto_scoped_heading('Variables'):
    for name in sorted(member_dict.keys()):
      _emit_variable(out, member_dict[name])
  return True


def _emit_functions(out, compounddef_element):
  member_dict = _find_documented_memberdefs(compounddef_element, 'function')
  if not member_dict:
    return False

  with out.auto_scoped_heading('Functions'):
    for name in sorted(member_dict.keys()):
      _emit_function(out, member_dict[name])
  return True


def _emit_file(out_io, compounddef_element, xml_path):
  header_filename = _find_filename(compounddef_element)
  logging.info('File: %s', header_filename)
  _emit_doc_header(out_io)
  mdwriter = MarkdownWriter(out_io)
  with mdwriter.auto_scoped_heading(
      f'Starboard Module Reference: `{header_filename}`'):
    _emit_description(mdwriter, compounddef_element)
    # When an API is deprecated it will be removed via #ifdef. When this is the
    # case, we will no longer have macros, enums, typedefs, structs, or
    # functions and thus the API should not be included in the site.
    has_content = _emit_macros(mdwriter, compounddef_element)
    has_content = _emit_enums(mdwriter, compounddef_element) or has_content
    has_content = _emit_typedefs(mdwriter, compounddef_element,
                                 xml_path) or has_content
    has_content = _emit_structs(mdwriter, compounddef_element,
                                xml_path) or has_content
    has_content = _emit_variables(mdwriter, compounddef_element) or has_content
    has_content = _emit_functions(mdwriter, compounddef_element) or has_content
  return has_content


def generate(source_dir, output_dir):
  if output_dir:
    site_path = environment.get_site_dir(output_dir)
  else:
    site_path = environment.get_site_dir(source_dir)
  doc_dir_path = os.path.join(site_path, 'docs', 'reference', 'starboard',
                              'modules')
  environment.make_clean_dirs(doc_dir_path)
  starboard_directory_path = environment.get_starboard_dir(source_dir)
  starboard_files = _get_files(starboard_directory_path, _HEADER_RE)
  with environment.mkdtemp(suffix='.' + _SCRIPT_NAME) as temp_directory_path:
    logging.debug('Working directory: %s', temp_directory_path)
    doxygenated_directory_path = os.path.join(temp_directory_path,
                                              'doxygenated')
    doxygenated_files = doxygen.doxygenate(starboard_files,
                                           doxygenated_directory_path)
    doxygen_directory_path = os.path.join(temp_directory_path, 'doxygen')
    for sb_version in _OSS_STARBOARD_VERSIONS:
      version_path = os.path.join(doxygen_directory_path, str(sb_version))
      version_doc_dir_path = os.path.join(doc_dir_path, str(sb_version))
      doxygen.doxygen(sb_version, doxygenated_files, [], version_path)
      doxygen_xml_path = os.path.join(version_path, 'xml')
      for header_xml_path in _get_files(doxygen_xml_path, _HEADER_XML_RE):
        header_xml = ET.parse(header_xml_path)
        for compounddef_element in header_xml.findall(
            './/compounddef[@kind="file"][compoundname]'):
          environment.make_dirs(version_doc_dir_path)
          header_filename = _find_filename(compounddef_element)
          doc_filename = (
              os.path.splitext(os.path.basename(header_filename))[0] + '.md')
          with contextlib.closing(StringIO()) as doc_file:
            if not _emit_file(doc_file, compounddef_element, doxygen_xml_path):
              continue
            doc_contents = doc_file.getvalue()
          doc_file_path = os.path.join(version_doc_dir_path, doc_filename)
          environment.write_file(doc_file_path, doc_contents)

          # Make the latest Starboard documentation version the default version.
          if sb_version == _OSS_STARBOARD_VERSIONS[-1]:
            doc_file_path = os.path.join(doc_dir_path, doc_filename)
            environment.write_file(doc_file_path, doc_contents)

  return 0


def main(argv):
  environment.setup_logging()
  options = environment.parse_arguments(__doc__, argv)
  environment.set_log_level(options.log_delta)
  return generate(options.source, options.out)


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
