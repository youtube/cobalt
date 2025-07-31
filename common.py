import re

def get_field_def_pos(deps_path, module):
    """Returns the start and end (exclusive) line numbers of a module definition.

    This function reads a DEPS-like file and finds the line range for a given
    module's definition. It handles both single-line and multi-line (dictionary)
    definitions by counting curly braces.
    """
    with open(deps_path, 'r', encoding='utf-8') as f:
        lines = f.readlines()

    start_index = -1
    module_pattern = re.compile(r"^\s*['\"]" + re.escape(module) + r"['\"]\s*:")

    for i, line in enumerate(lines):
        if module_pattern.match(line) and not line.lstrip().startswith('#'):
            start_index = i
            break

    if start_index == -1:
        raise ValueError(f'Could not find module "{module}" in {deps_path}')

    line_content = lines[start_index]
    colon_index = line_content.find(':')
    line_after_colon = line_content[colon_index + 1:]

    # If the value doesn't start a dictionary, it's a single-line value.
    if '{' not in line_after_colon and line_after_colon.rstrip().endswith(','):
        return start_index, start_index + 1

    open_braces = line_after_colon.count('{') - line_after_colon.count('}')

    # If braces are balanced on the same line, it's a single-line dict.
    if open_braces == 0 and line_after_colon.rstrip().endswith(','):
        return start_index, start_index + 1

    # It's a multi-line definition. Scan subsequent lines.
    for i in range(start_index + 1, len(lines)):
        line = lines[i]
        if line.isspace():
            continue
        if line.strip().startswith('#'):
            continue
        open_braces += line.count('{')
        open_braces -= line.count('}')
        if open_braces == 0:
            return start_index, i + 1
        # Edge case: End of encapsulating dict without trailing comma
        if open_braces < 0:
            return start_index, i

    raise ValueError(
        f'Could not find end of definition for module "{module}" in {deps_path}')
