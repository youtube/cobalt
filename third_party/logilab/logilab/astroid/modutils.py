# copyright 2003-2014 LOGILAB S.A. (Paris, FRANCE), all rights reserved.
# contact http://www.logilab.fr/ -- mailto:contact@logilab.fr
#
# This file is part of astroid.
#
# astroid is free software: you can redistribute it and/or modify it under
# the terms of the GNU Lesser General Public License as published by the Free
# Software Foundation, either version 2.1 of the License, or (at your option) any
# later version.
#
# astroid is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
# FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more
# details.
#
# You should have received a copy of the GNU Lesser General Public License along
# with astroid.  If not, see <http://www.gnu.org/licenses/>.
"""Python modules manipulation utility functions.

:type PY_SOURCE_EXTS: tuple(str)
:var PY_SOURCE_EXTS: list of possible python source file extension

:type STD_LIB_DIRS: set of str
:var STD_LIB_DIRS: directories where standard modules are located

:type BUILTIN_MODULES: dict
:var BUILTIN_MODULES: dictionary with builtin module names has key
"""
from __future__ import with_statement

__docformat__ = "restructuredtext en"

import imp
import os
import sys
from distutils.sysconfig import get_python_lib
from distutils.errors import DistutilsPlatformError
import zipimport

try:
    import pkg_resources
except ImportError:
    pkg_resources = None

from logilab.common import _handle_blacklist

PY_ZIPMODULE = object()

if sys.platform.startswith('win'):
    PY_SOURCE_EXTS = ('py', 'pyw')
    PY_COMPILED_EXTS = ('dll', 'pyd')
else:
    PY_SOURCE_EXTS = ('py',)
    PY_COMPILED_EXTS = ('so',)

# Notes about STD_LIB_DIRS
# Consider arch-specific installation for STD_LIB_DIRS definition
# :mod:`distutils.sysconfig` contains to much hardcoded values to rely on
#
# :see: `Problems with /usr/lib64 builds <http://bugs.python.org/issue1294959>`_
# :see: `FHS <http://www.pathname.com/fhs/pub/fhs-2.3.html#LIBLTQUALGTALTERNATEFORMATESSENTIAL>`_
try:
    # The explicit sys.prefix is to work around a patch in virtualenv that
    # replaces the 'real' sys.prefix (i.e. the location of the binary)
    # with the prefix from which the virtualenv was created. This throws
    # off the detection logic for standard library modules, thus the
    # workaround.
    STD_LIB_DIRS = {
        get_python_lib(standard_lib=True, prefix=sys.prefix),
        # Take care of installations where exec_prefix != prefix.
        get_python_lib(standard_lib=True, prefix=sys.exec_prefix),
        get_python_lib(standard_lib=True)}
    if os.name == 'nt':
        STD_LIB_DIRS.add(os.path.join(sys.prefix, 'dlls'))
        try:
            # real_prefix is defined when running inside virtualenv.
            STD_LIB_DIRS.add(os.path.join(sys.real_prefix, 'dlls'))
        except AttributeError:
            pass
# get_python_lib(standard_lib=1) is not available on pypy, set STD_LIB_DIR to
# non-valid path, see https://bugs.pypy.org/issue1164
except DistutilsPlatformError:
    STD_LIB_DIRS = set()

EXT_LIB_DIR = get_python_lib()

BUILTIN_MODULES = dict(zip(sys.builtin_module_names,
                           [1]*len(sys.builtin_module_names)))


class NoSourceFile(Exception):
    """exception raised when we are not able to get a python
    source file for a precompiled file
    """

def _normalize_path(path):
    return os.path.normcase(os.path.abspath(path))


_NORM_PATH_CACHE = {}

def _cache_normalize_path(path):
    """abspath with caching"""
    # _module_file calls abspath on every path in sys.path every time it's
    # called; on a larger codebase this easily adds up to half a second just
    # assembling path components. This cache alleviates that.
    try:
        return _NORM_PATH_CACHE[path]
    except KeyError:
        if not path: # don't cache result for ''
            return _normalize_path(path)
        result = _NORM_PATH_CACHE[path] = _normalize_path(path)
        return result

def load_module_from_name(dotted_name, path=None, use_sys=1):
    """Load a Python module from its name.

    :type dotted_name: str
    :param dotted_name: python name of a module or package

    :type path: list or None
    :param path:
      optional list of path where the module or package should be
      searched (use sys.path if nothing or None is given)

    :type use_sys: bool
    :param use_sys:
      boolean indicating whether the sys.modules dictionary should be
      used or not


    :raise ImportError: if the module or package is not found

    :rtype: module
    :return: the loaded module
    """
    return load_module_from_modpath(dotted_name.split('.'), path, use_sys)


def load_module_from_modpath(parts, path=None, use_sys=1):
    """Load a python module from its splitted name.

    :type parts: list(str) or tuple(str)
    :param parts:
      python name of a module or package splitted on '.'

    :type path: list or None
    :param path:
      optional list of path where the module or package should be
      searched (use sys.path if nothing or None is given)

    :type use_sys: bool
    :param use_sys:
      boolean indicating whether the sys.modules dictionary should be used or not

    :raise ImportError: if the module or package is not found

    :rtype: module
    :return: the loaded module
    """
    if use_sys:
        try:
            return sys.modules['.'.join(parts)]
        except KeyError:
            pass
    modpath = []
    prevmodule = None
    for part in parts:
        modpath.append(part)
        curname = '.'.join(modpath)
        module = None
        if len(modpath) != len(parts):
            # even with use_sys=False, should try to get outer packages from sys.modules
            module = sys.modules.get(curname)
        elif use_sys:
            # because it may have been indirectly loaded through a parent
            module = sys.modules.get(curname)
        if module is None:
            mp_file, mp_filename, mp_desc = imp.find_module(part, path)
            module = imp.load_module(curname, mp_file, mp_filename, mp_desc)
            # mp_file still needs to be closed.
            if mp_file:
                mp_file.close()
        if prevmodule:
            setattr(prevmodule, part, module)
        _file = getattr(module, '__file__', '')
        if not _file and len(modpath) != len(parts):
            raise ImportError('no module in %s' % '.'.join(parts[len(modpath):]))
        path = [os.path.dirname(_file)]
        prevmodule = module
    return module


def load_module_from_file(filepath, path=None, use_sys=1, extrapath=None):
    """Load a Python module from it's path.

    :type filepath: str
    :param filepath: path to the python module or package

    :type path: list or None
    :param path:
      optional list of path where the module or package should be
      searched (use sys.path if nothing or None is given)

    :type use_sys: bool
    :param use_sys:
      boolean indicating whether the sys.modules dictionary should be
      used or not


    :raise ImportError: if the module or package is not found

    :rtype: module
    :return: the loaded module
    """
    modpath = modpath_from_file(filepath, extrapath)
    return load_module_from_modpath(modpath, path, use_sys)


def _check_init(path, mod_path):
    """check there are some __init__.py all along the way"""
    for part in mod_path:
        path = os.path.join(path, part)
        if not _has_init(path):
            return False
    return True


def modpath_from_file(filename, extrapath=None):
    """given a file path return the corresponding splitted module's name
    (i.e name of a module or package splitted on '.')

    :type filename: str
    :param filename: file's path for which we want the module's name

    :type extrapath: dict
    :param extrapath:
      optional extra search path, with path as key and package name for the path
      as value. This is usually useful to handle package splitted in multiple
      directories using __path__ trick.


    :raise ImportError:
      if the corresponding module's name has not been found

    :rtype: list(str)
    :return: the corresponding splitted module's name
    """
    base = os.path.splitext(os.path.abspath(filename))[0]
    if extrapath is not None:
        for path_ in extrapath:
            path = os.path.abspath(path_)
            if path and os.path.normcase(base[:len(path)]) == os.path.normcase(path):
                submodpath = [pkg for pkg in base[len(path):].split(os.sep)
                              if pkg]
                if _check_init(path, submodpath[:-1]):
                    return extrapath[path_].split('.') + submodpath
    for path in sys.path:
        path = _cache_normalize_path(path)
        if path and os.path.normcase(base).startswith(path):
            modpath = [pkg for pkg in base[len(path):].split(os.sep) if pkg]
            if _check_init(path, modpath[:-1]):
                return modpath
    raise ImportError('Unable to find module for %s in %s' % (
        filename, ', \n'.join(sys.path)))


def file_from_modpath(modpath, path=None, context_file=None):
    return file_info_from_modpath(modpath, path, context_file)[0]

def file_info_from_modpath(modpath, path=None, context_file=None):
    """given a mod path (i.e. splitted module / package name), return the
    corresponding file, giving priority to source file over precompiled
    file if it exists

    :type modpath: list or tuple
    :param modpath:
      splitted module's name (i.e name of a module or package splitted
      on '.')
      (this means explicit relative imports that start with dots have
      empty strings in this list!)

    :type path: list or None
    :param path:
      optional list of path where the module or package should be
      searched (use sys.path if nothing or None is given)

    :type context_file: str or None
    :param context_file:
      context file to consider, necessary if the identifier has been
      introduced using a relative import unresolvable in the actual
      context (i.e. modutils)

    :raise ImportError: if there is no such module in the directory

    :rtype: (str or None, import type)
    :return:
      the path to the module's file or None if it's an integrated
      builtin module such as 'sys'
    """
    if context_file is not None:
        context = os.path.dirname(context_file)
    else:
        context = context_file
    if modpath[0] == 'xml':
        # handle _xmlplus
        try:
            return _file_from_modpath(['_xmlplus'] + modpath[1:], path, context)
        except ImportError:
            return _file_from_modpath(modpath, path, context)
    elif modpath == ['os', 'path']:
        # FIXME: currently ignoring search_path...
        return os.path.__file__, imp.PY_SOURCE
    return _file_from_modpath(modpath, path, context)


def get_module_part(dotted_name, context_file=None):
    """given a dotted name return the module part of the name :

    >>> get_module_part('logilab.common.modutils.get_module_part')
    'logilab.common.modutils'

    :type dotted_name: str
    :param dotted_name: full name of the identifier we are interested in

    :type context_file: str or None
    :param context_file:
      context file to consider, necessary if the identifier has been
      introduced using a relative import unresolvable in the actual
      context (i.e. modutils)


    :raise ImportError: if there is no such module in the directory

    :rtype: str or None
    :return:
      the module part of the name or None if we have not been able at
      all to import the given name

    XXX: deprecated, since it doesn't handle package precedence over module
    (see #10066)
    """
    # os.path trick
    if dotted_name.startswith('os.path'):
        return 'os.path'
    parts = dotted_name.split('.')
    if context_file is not None:
        # first check for builtin module which won't be considered latter
        # in that case (path != None)
        if parts[0] in BUILTIN_MODULES:
            if len(parts) > 2:
                raise ImportError(dotted_name)
            return parts[0]
        # don't use += or insert, we want a new list to be created !
    path = None
    starti = 0
    if parts[0] == '':
        assert context_file is not None, \
                'explicit relative import, but no context_file?'
        path = [] # prevent resolving the import non-relatively
        starti = 1
    while parts[starti] == '': # for all further dots: change context
        starti += 1
        context_file = os.path.dirname(context_file)
    for i in range(starti, len(parts)):
        try:
            file_from_modpath(parts[starti:i+1], path=path,
                              context_file=context_file)
        except ImportError:
            if not i >= max(1, len(parts) - 2):
                raise
            return '.'.join(parts[:i])
    return dotted_name


def get_module_files(src_directory, blacklist):
    """given a package directory return a list of all available python
    module's files in the package and its subpackages

    :type src_directory: str
    :param src_directory:
      path of the directory corresponding to the package

    :type blacklist: list or tuple
    :param blacklist:
      optional list of files or directory to ignore, default to the value of
      `logilab.common.STD_BLACKLIST`

    :rtype: list
    :return:
      the list of all available python module's files in the package and
      its subpackages
    """
    files = []
    for directory, dirnames, filenames in os.walk(src_directory):
        _handle_blacklist(blacklist, dirnames, filenames)
        # check for __init__.py
        if not '__init__.py' in filenames:
            dirnames[:] = ()
            continue
        for filename in filenames:
            if _is_python_file(filename):
                src = os.path.join(directory, filename)
                files.append(src)
    return files


def get_source_file(filename, include_no_ext=False):
    """given a python module's file name return the matching source file
    name (the filename will be returned identically if it's a already an
    absolute path to a python source file...)

    :type filename: str
    :param filename: python module's file name


    :raise NoSourceFile: if no source file exists on the file system

    :rtype: str
    :return: the absolute path of the source file if it exists
    """
    base, orig_ext = os.path.splitext(os.path.abspath(filename))
    for ext in PY_SOURCE_EXTS:
        source_path = '%s.%s' % (base, ext)
        if os.path.exists(source_path):
            return source_path
    if include_no_ext and not orig_ext and os.path.exists(base):
        return base
    raise NoSourceFile(filename)


def is_python_source(filename):
    """
    rtype: bool
    return: True if the filename is a python source file
    """
    return os.path.splitext(filename)[1][1:] in PY_SOURCE_EXTS


def is_standard_module(modname, std_path=None):
    """try to guess if a module is a standard python module (by default,
    see `std_path` parameter's description)

    :type modname: str
    :param modname: name of the module we are interested in

    :type std_path: list(str) or tuple(str)
    :param std_path: list of path considered has standard


    :rtype: bool
    :return:
      true if the module:
      - is located on the path listed in one of the directory in `std_path`
      - is a built-in module
    """
    modname = modname.split('.')[0]
    try:
        filename = file_from_modpath([modname])
    except ImportError:
        # import failed, i'm probably not so wrong by supposing it's
        # not standard...
        return False
    # modules which are not living in a file are considered standard
    # (sys and __builtin__ for instance)
    if filename is None:
        return True
    filename = _normalize_path(filename)
    if filename.startswith(_cache_normalize_path(EXT_LIB_DIR)):
        return False
    if std_path is None:
        std_path = STD_LIB_DIRS
    for path in std_path:
        if filename.startswith(_cache_normalize_path(path)):
            return True
    return False



def is_relative(modname, from_file):
    """return true if the given module name is relative to the given
    file name

    :type modname: str
    :param modname: name of the module we are interested in

    :type from_file: str
    :param from_file:
      path of the module from which modname has been imported

    :rtype: bool
    :return:
      true if the module has been imported relatively to `from_file`
    """
    if not os.path.isdir(from_file):
        from_file = os.path.dirname(from_file)
    if from_file in sys.path:
        return False
    try:
        stream, _, _ = imp.find_module(modname.split('.')[0], [from_file])

        # Close the stream to avoid ResourceWarnings.
        if stream:
            stream.close()
        return True
    except ImportError:
        return False


# internal only functions #####################################################

def _file_from_modpath(modpath, path=None, context=None):
    """given a mod path (i.e. splitted module / package name), return the
    corresponding file

    this function is used internally, see `file_from_modpath`'s
    documentation for more information
    """
    assert len(modpath) > 0
    if context is not None:
        try:
            mtype, mp_filename = _module_file(modpath, [context])
        except ImportError:
            mtype, mp_filename = _module_file(modpath, path)
    else:
        mtype, mp_filename = _module_file(modpath, path)
    if mtype == imp.PY_COMPILED:
        try:
            return get_source_file(mp_filename), imp.PY_SOURCE
        except NoSourceFile:
            return mp_filename, imp.PY_COMPILED
    elif mtype == imp.C_BUILTIN:
        # integrated builtin module
        return None, imp.C_BUILTIN
    elif mtype == imp.PKG_DIRECTORY:
        mp_filename = _has_init(mp_filename)
        mtype = imp.PY_SOURCE
    return mp_filename, mtype

def _search_zip(modpath, pic):
    for filepath, importer in pic.items():
        if importer is not None:
            if importer.find_module(modpath[0]):
                if not importer.find_module(os.path.sep.join(modpath)):
                    raise ImportError('No module named %s in %s/%s' % (
                        '.'.join(modpath[1:]), filepath, modpath))
                return PY_ZIPMODULE, os.path.abspath(filepath) + os.path.sep + os.path.sep.join(modpath), filepath
    raise ImportError('No module named %s' % '.'.join(modpath))


def _module_file(modpath, path=None):
    """get a module type / file path

    :type modpath: list or tuple
    :param modpath:
      splitted module's name (i.e name of a module or package splitted
      on '.'), with leading empty strings for explicit relative import

    :type path: list or None
    :param path:
      optional list of path where the module or package should be
      searched (use sys.path if nothing or None is given)


    :rtype: tuple(int, str)
    :return: the module type flag and the file path for a module
    """
    # egg support compat
    try:
        pic = sys.path_importer_cache
        _path = (path is None and sys.path or path)
        for __path in _path:
            if not __path in pic:
                try:
                    pic[__path] = zipimport.zipimporter(__path)
                except zipimport.ZipImportError:
                    pic[__path] = None
        checkeggs = True
    except AttributeError:
        checkeggs = False
    # pkg_resources support (aka setuptools namespace packages)
    if (pkg_resources is not None
            and modpath[0] in pkg_resources._namespace_packages
            and modpath[0] in sys.modules
            and len(modpath) > 1):
        # setuptools has added into sys.modules a module object with proper
        # __path__, get back information from there
        module = sys.modules[modpath.pop(0)]
        path = module.__path__
    imported = []
    while modpath:
        modname = modpath[0]
        # take care to changes in find_module implementation wrt builtin modules
        #
        # Python 2.6.6 (r266:84292, Sep 11 2012, 08:34:23)
        # >>> imp.find_module('posix')
        # (None, 'posix', ('', '', 6))
        #
        # Python 3.3.1 (default, Apr 26 2013, 12:08:46)
        # >>> imp.find_module('posix')
        # (None, None, ('', '', 6))
        try:
            stream, mp_filename, mp_desc = imp.find_module(modname, path)
        except ImportError:
            if checkeggs:
                return _search_zip(modpath, pic)[:2]
            raise
        else:
            # Don't forget to close the stream to avoid
            # spurious ResourceWarnings.
            if stream:
               stream.close()

            if checkeggs and mp_filename:
                fullabspath = [_cache_normalize_path(x) for x in _path]
                try:
                    pathindex = fullabspath.index(os.path.dirname(_normalize_path(mp_filename)))
                    emtype, emp_filename, zippath = _search_zip(modpath, pic)
                    if pathindex > _path.index(zippath):
                        # an egg takes priority
                        return emtype, emp_filename
                except ValueError:
                    # XXX not in _path
                    pass
                except ImportError:
                    pass
                checkeggs = False
        imported.append(modpath.pop(0))
        mtype = mp_desc[2]
        if modpath:
            if mtype != imp.PKG_DIRECTORY:
                raise ImportError('No module %s in %s' % ('.'.join(modpath),
                                                          '.'.join(imported)))
            # XXX guess if package is using pkgutil.extend_path by looking for
            # those keywords in the first four Kbytes
            try:
                with open(os.path.join(mp_filename, '__init__.py'), 'rb') as stream:
                    data = stream.read(4096)
            except IOError:
                path = [mp_filename]
            else:
                if b'pkgutil' in data and b'extend_path' in data:
                    # extend_path is called, search sys.path for module/packages
                    # of this name see pkgutil.extend_path documentation
                    path = [os.path.join(p, *imported) for p in sys.path
                            if os.path.isdir(os.path.join(p, *imported))]
                else:
                    path = [mp_filename]
    return mtype, mp_filename

def _is_python_file(filename):
    """return true if the given filename should be considered as a python file

    .pyc and .pyo are ignored
    """
    for ext in ('.py', '.so', '.pyd', '.pyw'):
        if filename.endswith(ext):
            return True
    return False


def _has_init(directory):
    """if the given directory has a valid __init__ file, return its path,
    else return None
    """
    mod_or_pack = os.path.join(directory, '__init__')
    for ext in PY_SOURCE_EXTS + ('pyc', 'pyo'):
        if os.path.exists(mod_or_pack + '.' + ext):
            return mod_or_pack + '.' + ext
    return None
