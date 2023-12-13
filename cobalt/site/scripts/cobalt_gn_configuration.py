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
"""Module to generate GN Configuration Reference."""
import json
import environment
import os
import re
import subprocess
import sys


def print_doc_header(doc, title):
  doc.write('Project: /youtube/cobalt/_project.yaml\n')
  doc.write('Book: /youtube/cobalt/_book.yaml\n\n')
  doc.write(f'# {title}\n\n')


def print_section_property_table_header(doc, section):
  section = section.replace('_', ' ').capitalize()
  doc.write('| ' + section + ' |\n')
  doc.write('| :--- |\n')


def format_comment(raw_comment):
  """Reformats the comment to be a single line and layout lists correctly."""
  list_item_regex = re.compile(r'\s+"([^"]+)"\s*-+\s*(.*)')
  continued_list_item_regex = re.compile(r'\s+')

  comment = ''
  is_last_line_list_item = False
  for line in raw_comment.splitlines():
    list_item_match = re.search(list_item_regex, line)
    comment_text = line.strip()
    if comment_text[0:4] == 'TODO':
      continue
    elif list_item_match:
      is_last_line_list_item = True
      comment += f'<li><code>{list_item_match[1]}</code> - {list_item_match[2]}'
    elif comment_text == '':
      comment += '<br><br>'
    else:
      if is_last_line_list_item:
        continued_list_item_match = re.search(continued_list_item_regex, line)
        if not continued_list_item_match:
          comment += '</li>'
      comment += ' ' + comment_text

  # Wrap the list items in a list element.
  comment = comment.replace('<li>', '<ul><li>', 1)
  comment = last_replace(comment, '</li>', '</li></ul>')
  return comment


def print_item(doc, name, value, comment):
  comment = format_comment(comment)
  if value:
    value = value.replace('|', r'\|')
    if comment[-8:] != '<br><br>':
      comment += '<br><br>'
    comment += ('The default value is `' + value + '`.')
  if comment[0:8] != '<br><br>':
    doc.write('| **`' + name + '`**<br><br>' + comment + ' |\n')
  else:
    doc.write('| **`' + name + '`**' + comment + ' |\n')


def create_reference_doc(site_path, variables):
  reference_doc_path = os.path.join(site_path, 'docs', 'reference', 'starboard',
                                    'gn-configuration.md')
  environment.make_dirs(os.path.dirname(reference_doc_path))
  with open(reference_doc_path, 'w', encoding='utf8') as doc:
    print_doc_header(doc, 'Starboard: configuration.gni Reference Guide')

    print_section_property_table_header(doc, 'variables')
    for name, value, comment in sorted(variables, key=lambda x: x[0]):
      print_item(doc, name, value, comment)


def add_args(args, function_data):
  function_data = function_data.replace(');', '')
  if function_data == '':
    return args
  arg_list = function_data.split(',')
  for arg_index in range(0, len(arg_list)):
    arg_item_no_ws = arg_list[arg_index].strip()
    if arg_item_no_ws != '':
      arg_details = arg_item_no_ws.split(' ')
      args.append({'name': arg_details.pop(), 'type': ' '.join(arg_details)})
  return args


def last_replace(value, old, new):
  reverse_list = value.rsplit(old, 1)
  return new.join(reverse_list)


def main(source_dir, output_dir=None):
  base_config_path = '//starboard/build/config/base_configuration.gni'

  # `gn args` will resolve all variables used in the default values before
  # printing so in order to keep the docs looking reasonable the variables
  # used must be listed here.
  # Note that `root_out_dir` is not a build argument and cannot be overridden.
  args = ['clang_base_path']
  args_overrides = ' '.join(f'{x}="<{x}>"' for x in args)
  try:
    out_dir = '/project_out_dir'
    subprocess.check_call(['gn', 'gen', out_dir], cwd=source_dir)
    output = subprocess.check_output(
        ['gn', 'args', out_dir, '--list', '--json', '--args=' + args_overrides],
        cwd=source_dir)
  except subprocess.CalledProcessError as cpe:
    raise RuntimeError(f'Failed to run GN: {cpe.output}') from cpe

  # If `gn args` invokes any print statements these will be printed before the
  # json string starts. Strip all lines until we encounter the start of the json
  # array.
  output_lines = output.decode('utf-8').splitlines()
  gn_vars = []
  while output_lines:
    try:
      gn_vars = json.loads(' '.join(output_lines))
    except ValueError:
      # If the attempt to parse the rest of the lines fails we remove the first
      # line and try again.
      output_lines = output_lines[1:]
    else:
      break

  variables = []
  for variable in gn_vars:
    if variable['default'].get('file') == base_config_path:
      name = variable['name']
      value = variable['default']['value']
      comment = variable.get('comment', '').strip()
      variables.append((name, value, comment))

  if output_dir:
    site_path = environment.get_site_dir(output_dir)
  else:
    site_path = environment.get_site_dir(source_dir)
  create_reference_doc(site_path, variables)
  return 0


if __name__ == '__main__':
  options = environment.parse_arguments('', sys.argv[1:])
  sys.exit(main(options.source, options.out))
