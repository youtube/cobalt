# Copyright 2023 The Cobalt Authors. All Rights Reserved.
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
"""Module to generate Starboard Configuration Reference."""
import environment
import os
import re
import sys


def print_doc_header(doc, title):
  doc.write('Project: /youtube/cobalt/_project.yaml\n')
  doc.write('Book: /youtube/cobalt/_book.yaml\n\n')
  doc.write(f'# {title}\n\n')


def print_doc_section_header(doc, section, section_headers):
  doc.write('## ' + section + '\n\n')
  if section in section_headers:
    doc.write(section_headers[section] + '\n\n')


def print_section_property_table_header(doc):
  doc.write('| Properties |\n')
  doc.write('| :--- |\n')


def print_property(doc, prop):
  comment = prop['comment']
  comment = comment.replace('__', '&#95;&#95;')
  if prop['value']:
    line_break = ''
    value = prop['value'].replace('|', r'\|')
    if len(value) > 30:
      line_break = '<br>'
    if comment[-8:] != '<br><br>':
      comment += '<br><br>'
    comment += ('The default value in the Stub implementation is ' +
                line_break + '`' + prop['value'] + '`')
  elif prop['undefined']:
    if comment[-8:] != '<br><br>':
      comment += '<br><br>'
    comment += 'By default, this property is undefined.'
  if comment[0:8] != '<br><br>':
    doc.write('| **`' + prop['name'] + '`**<br><br>' + comment + ' |\n')
  else:
    doc.write('| **`' + prop['name'] + '`**' + comment + ' |\n')


def create_reference_doc(site_path, properties, section_headers):
  reference_doc_path = os.path.join(site_path, 'docs', 'reference', 'starboard',
                                    'configuration-public.md')
  environment.make_dirs(os.path.dirname(reference_doc_path))
  with open(reference_doc_path, 'w', encoding='utf8') as doc:
    print_doc_header(doc, 'Starboard Configuration Reference Guide')

    for section in sorted(properties):
      if len(properties[section]) > 0:
        print_doc_section_header(doc, section, section_headers)

        print_section_property_table_header(doc)

        for p in range(0, len(properties[section])):
          print_property(doc, properties[section][p])

        doc.write('\n\n')


def main(source_dir, output_dir=None):
  config_dir = environment.get_stub_platform_dir(source_dir)
  file_path = os.path.join(config_dir, 'configuration_public.h')
  with open(file_path, 'r', encoding='utf8') as file_contents:
    file_lines = file_contents.readlines()

  # parse .proto files
  comment = ''
  in_line_item = ''
  section = ''
  properties = {}
  section_headers = {}
  last_thing_was_a_header = False
  for original_line in file_lines:
    line = original_line.strip()
    if line[0:7] == '// --- ':
      section = line[7:].split(' -')[0]
      properties[section] = []
      last_thing_was_a_header = True
    elif section and (line[0:8] == '#define ' or line[0:7] == '#undef '):

      if in_line_item:
        if comment:
          comment += '</li></' + in_line_item + '>'
        in_line_item = ''
      last_thing_was_a_header = False
      prop_array = line.split(' ')
      prop = {'comment': '', 'name': '', 'value': '', 'undefined': False}
      if line[0:7] == '#undef ':
        prop['undefined'] = True
      if len(prop_array) > 1:
        prop['name'] = prop_array[1]
      if len(prop_array) > 2:
        prop['value'] = ' '.join(prop_array[2:])
      if comment:
        prop['comment'] = comment.strip()
      if '(' in prop['name'] and ')' not in prop['name']:
        new_string = ' '.join(prop_array[1:])
        new_prop_array = new_string.split(')')
        prop['name'] = new_prop_array[0] + ')'
        new_value = ')'.join(new_prop_array[1:])
        prop['value'] = new_value.strip()
      properties[section].append(prop)
      comment = ''
    elif section and line[0:2] == '//':
      ol_item_regex = re.compile(r'^\d\. ')
      comment_text = line[2:].strip()
      is_ol_item = re.search(ol_item_regex, comment_text)
      if (is_ol_item or comment_text.strip()[0:2] == '- ' or
          comment_text.strip()[0:2] == '* '):
        # Replace '* ' at beginning of comment with '<li>'
        # Strip whitespace before '*' and after '*" up to start of text
        if not in_line_item:
          if is_ol_item:
            comment_text = '<ol><li>' + comment_text.strip()[2:].strip()
            in_line_item = 'ol'
          else:
            comment_text = '<ul><li>' + comment_text.strip()[1:].strip()
            in_line_item = 'ul'
        else:
          if is_ol_item:
            comment_text = '</li><li>' + comment_text.strip()[2:].strip()
          else:
            comment_text = '</li><li>' + comment_text.strip()[1:].strip()
      comment += ' ' + comment_text
    elif comment and line == '':
      if last_thing_was_a_header:
        section_headers[section] = comment
      last_thing_was_a_header = False
      comment = ''
      if comment[-8:] != '<br><br>':
        comment += '<br><br>'

  if output_dir:
    site_path = environment.get_site_dir(output_dir)
  else:
    site_path = environment.get_site_dir(source_dir)
  create_reference_doc(site_path, properties, section_headers)
  return 0


if __name__ == '__main__':
  options = environment.parse_arguments(__doc__, sys.argv[1:])
  sys.exit(main(options.source, options.out))
