#
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
#
"""A directory walker framework that filter and process files.

It filters file name against filter functions and process filtered files using
processor function.
"""

import os

# All filter functions


def CppSourceCodeFilter(filename):
  """Filter function returns true for C++ source code files."""
  return (filename[-3:] == '.cc' or filename[-4:] == '.cpp' or
          filename[-2:] == '.h' or filename[-4:] == '.hpp')


def SourceCodeFilter(filename):
  """Filter function returns true for general source code files."""
  return (filename[-3:] == '.cc' or filename[-4:] == '.cpp' or
          filename[-2:] == '.h' or filename[-4:] == '.hpp' or
          filename[-4:] == '.idl' or filename[-11:] == '.h.template' or
          filename[-12:] == '.cc.template' or filename[-2:] == '.y' or
          filename[-7:] == '.h.pump' or filename[-3:] == '.js')


# All processor functions


def AddInvalidSpace(pathname):
  """Add a space in the very beginning of the file."""
  with open(pathname, 'r') as f:
    content = f.read()
  content = ' ' + content
  with open(pathname, 'w') as f:
    f.write(content)


def ReplaceLicenseHeader(pathname):
  """Replace license headers in /* ... */ to // ...."""
  with open(pathname, 'r') as f:
    lines = f.readlines()
  # Guestimate the copyright header
  for i in range(0, len(lines)):
    if (len(lines[i]) == 3 and lines[i][:2] == '/*' and
        lines[i + 1].find('opyright') != -1 and
        lines[i + 1].find('Google') != -1):
      del lines[i]
      for j in range(i, len(lines)):
        if lines[j].find('*/') != -1:
          del lines[j]
          break

        if lines[j][0] == ' ':
          line_without_star = lines[j][2:]
        else:
          line_without_star = lines[j][1:]
        lines[j] = '//' + line_without_star
    if i >= len(lines) - 1:
      break
  with open(pathname, 'w') as f:
    f.writelines(lines)


def ReplaceMediaNamespace(pathname):
  """Move namespace media into namespace cobalt."""
  with open(pathname, 'r') as f:
    source_lines = f.readlines()
  target_lines = []
  for i in range(0, len(source_lines)):
    if source_lines[i].find('namespace media {') != -1:
      if i == 0 or source_lines[i - 1].find('namespace cobalt {') == -1:
        target_lines.append('namespace cobalt {\n')
    target_lines.append(source_lines[i])
    if source_lines[i].find('}  // namespace media') != -1:
      if i == len(source_lines) - 1 or source_lines[i + 1].find(
          '}  // namespace cobalt') == -1:
        target_lines.append('}  // namespace cobalt\n')

  with open(pathname, 'w') as f:
    f.writelines(target_lines)


C_FUNCTION_LIST = """
assert,isalnum,isalpha,iscntrl,isdigit,isgraph,islower,isprint,ispunct,isspace,
isupper,isxdigit,toupper,tolower,errno,setlocale,acos,asin,atan,atan2,ceil,cos,
cosh,exp,fabs,floor,fmod,frexp,ldexp,log,log10,modf,pow,sin,sinh,sqrt,tan,tanh,
setjmp,longjmp,signal,raise,va_start,va_arg,va_end,clearerr,fclose,feof,fflush,
fgetc,fgetpos,fgets,fopen,fprintf,fputc,fputs,fread,freopen,fscanf,fseek,
fsetpos,ftell,fwrite,getc,getchar,gets,perror,printf,putchar,puts,remove,rewind,
scanf,setbuf,setvbuf,sprintf,sscanf,tmpfile,tmpnam,ungetc,vfprintf,vprintf,
vsprintf,abort,abs,atexit,atof,atoi,atol,bsearch,calloc,div,exit,getenv,free,
labs,ldiv,malloc,mblen,mbstowcs,mbtowc,qsort,rand,realloc,strtod,strtol,strtoul,
srand,system,wctomb,wcstombs,memchr,memcmp,memcpy,memmove,memset,strcat,strchr,
strcmp,strcoll,strcpy,strcspn,strerror,strlen,strncat,strncmp,strncpy,strpbrk,
strrchr,strspn,strstr,strtok,strxfrm,asctime,clock,ctime,difftime,gmtime,
localtime,mktime,strftime,time,opendir,closedir,readdir,rewinddir,scandir,
seekdir,telldir,access,alarm,chdir,chown,close,chroot,ctermid,cuserid,dup,dup2,
execl,execle,execlp,execv,execve,execvp,fchdir,fork,fpathconf,getegid,geteuid,
gethostname,getopt,getgid,getgroups,getlogin,getpgrp,getpid,getppid,getuid,
isatty,link,lseek,mkdir,open,pathconf,pause,pipe,read,rename,rmdir,setgid,
setpgid,setsid,setuid,sleep,sysconf,tcgetpgrp,tcsetpgrp,ttyname,unlink,write,
clrscr,getch,getche,statfs,endpwent,fgetpwent,getpw,getpwent,getpwnam,getpwuid,
getuidx,putpwent,pclose,popen,putenv,setenv,setpwent,setreuid,stat,uname,
unsetenv,setuidx,setegid,setrgid,seteuid,setruid,getruid
"""

SB_CHARACTER_REPLACEMENT_DICT = {
}

SB_MEMORY_REPLACEMENT_DICT = {
    'free': 'SbMemoryFree',
    'malloc': 'SbMemoryAllocate',
    'realloc': 'SbMemoryReallocate'
}

SB_STRING_REPLACEMENT_DICT = {
}

c_function_list = []


def AddProjectHeader(lines, header):
  """Add a new project header into lines which takes order into account."""
  assert header.find('.h') != -1 and header.find('/') != -1

  include_statement = '#include "' + header + '"\n'

  last_c_header = -1
  last_cpp_header = -1
  last_project_header = -1

  for i in range(0, len(lines)):
    line = lines[i]
    if line.find('#include <') != -1:
      if line.find('.h') != -1:
        last_c_header = i
      else:
        last_cpp_header = i
    elif line.find('#include "') != -1:
      if line.find(header) != -1:
        return
      last_project_header = i

  if (last_project_header < last_cpp_header or
      last_project_header < last_c_header):
    # There is no project header at all, add after last cpp or c header
    if last_cpp_header != -1:
      lines.insert(last_cpp_header + 1, include_statement)
      lines.insert(last_cpp_header + 1, '\n')
    else:
      # In the case that there is no C header as well, this will be added to the
      # first line, maybe before the copyrights, and hopefully will be caught
      # during code review.
      lines.insert(last_c_header + 1, include_statement)
      lines.insert(last_c_header + 1, '\n')
    return

  # Now we have to add into the project include section with proper ordering
  while last_project_header > 0:
    if include_statement > lines[last_project_header]:
      lines.insert(last_project_header + 1, include_statement)
      return
    last_project_header -= 1

  lines.insert(0, include_statement)


def RemoveHeader(lines, header):
  for i in range(0, len(lines)):
    if lines[i].find('#include') != -1 and lines[i].find(header) != -1:
      del lines[i]
      if (lines[i] == '\n' and lines[i + 1] == '\n' or
          lines[i - 1] == '\n' and lines[i] == '\n'):
        del lines[i]
      return


def DumpCHeadersAndFunctions(pathname):
  """Print out C headers included and C functions used."""
  global c_function_list
  if not c_function_list:
    c_function_list = [
        x for x in C_FUNCTION_LIST.replace('\n', '').split(',') if x
    ]
  with open(pathname, 'r') as f:
    source_lines = f.readlines()
  first = True

  add_starboard_character_h = False
  add_starboard_memory_h = False
  add_starboard_string_h = False
  add_starboard_types_h = False

  for i in range(0, len(source_lines)):
    if source_lines[i].find('#include <') != -1 and source_lines[i].find(
        '.h>') != -1:
      if source_lines[i].find('stddef.h') or source_lines[i].find('stdint.h'):
        add_starboard_types_h = True
        continue  # We can fix this, no need to dump

      if first:
        print pathname
        first = False
      print '    => line ', i + 1, '\t', source_lines[i][:-1]
    for j in range(0, len(c_function_list)):
      index = source_lines[i].find(c_function_list[j] + '(')
      if index == -1:
        continue
      if (source_lines[i].find('//') != -1 and
          source_lines[i].find('//') < index):
        continue
      if index == 0 or (not source_lines[i][index - 1].isalpha() and
                        source_lines[i][index - 1] != '_' and
                        source_lines[i][index - 1] != ':' and
                        source_lines[i][index - 1] != '>' and
                        source_lines[i][index - 1] != '.'):
        if c_function_list[j] in SB_CHARACTER_REPLACEMENT_DICT:
          source_lines[i] = source_lines[i].replace(
              c_function_list[j],
              SB_CHARACTER_REPLACEMENT_DICT[c_function_list[j]])
          add_starboard_character_h = True
          continue  # We fixed this, no need to dump
        if c_function_list[j] in SB_MEMORY_REPLACEMENT_DICT:
          source_lines[i] = source_lines[i].replace(
              c_function_list[j],
              SB_MEMORY_REPLACEMENT_DICT[c_function_list[j]])
          add_starboard_memory_h = True
          continue  # We fixed this, no need to dump
        if c_function_list[j] in SB_STRING_REPLACEMENT_DICT:
          source_lines[i] = source_lines[i].replace(
              c_function_list[j],
              SB_STRING_REPLACEMENT_DICT[c_function_list[j]])
          add_starboard_string_h = True
          continue  # We fixed this, no need to dump
        if first:
          print pathname
          first = False
        print '    => line ', i + 1, '\t', source_lines[
            i][:-1], 'contains', c_function_list[j]

  if add_starboard_character_h:
    AddProjectHeader(source_lines, 'starboard/character.h')

  if add_starboard_memory_h:
    AddProjectHeader(source_lines, 'starboard/memory.h')

  if add_starboard_string_h:
    AddProjectHeader(source_lines, 'starboard/string.h')

  if add_starboard_types_h:
    RemoveHeader(source_lines, 'stddef.h')
    RemoveHeader(source_lines, 'stdint.h')
    AddProjectHeader(source_lines, 'starboard/types.h')

  with open(pathname, 'w') as f:
    f.writelines(source_lines)


def CollectFilesInDirectory(root, file_filter=None):
  """Traverse the folder and call filters."""
  result = []
  for current_dir, _, files in os.walk(root):
    for f in files:
      pathname = current_dir + '/' + f
      if file_filter and not file_filter(pathname):
        continue
      result.append(pathname)
  return result


def ProcessFiles(pathnames, processor):
  for pathname in pathnames:
    processor(pathname)


# ProcessFiles(CollectFilesInDirectory('.', CppSourceCodeFilter),
#                                      AddInvalidSpace)
# ProcessFiles(CollectFilesInDirectory('.', SourceCodeFilter),
#                                      ReplaceLicenseHeader)
# ProcessFiles(CollectFilesInDirectory('.', SourceCodeFilter),
#                                      ReplaceMediaNamespace)
ProcessFiles(
    CollectFilesInDirectory('.', CppSourceCodeFilter), DumpCHeadersAndFunctions)
