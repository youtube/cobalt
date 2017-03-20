"""Helper functions to handle file operations"""
import logging
import os
import shutil
import hashlib
import tempfile
from ConfigParser import RawConfigParser
log = logging.getLogger(__name__)


def compare(file1, file2):
    """compares the contents of two files, passed in either as
       open file handles or accessible file paths. Does a simple
       naive string comparison, so do not use on larger files"""
    if isinstance(file1, basestring):
        file1 = open(file1, 'r', True)
    if isinstance(file2, basestring):
        file2 = open(file2, 'r', True)
    file1_contents = file1.read()
    file2_contents = file2.read()
    return file1_contents == file2_contents


def directoryContains(directory, suffix):
    """ Return true if the given directory contains the provided wildcard
    suffix, similar to `ls foo/*bar` """
    hit = any([f.endswith(suffix) for f in os.listdir(directory)])
    if not hit:
        log.error("Could not find *%s in %s" % (suffix, directory))
    return hit


def copyfile(src, dst, copymode=True):
    """Copy src to dst, preserving permissions and times if copymode is True"""
    shutil.copyfile(src, dst)
    if copymode:
        shutil.copymode(src, dst)
        shutil.copystat(src, dst)


def sha1sum(f):
    """Return the SHA-1 hash of the contents of file `f`, in hex format"""
    h = hashlib.sha1()
    fp = open(f, 'rb')
    while True:
        block = fp.read(512 * 1024)
        if not block:
            break
        h.update(block)
    return h.hexdigest()


def safe_unlink(filename):
    """unlink filename ignorning errors if the file doesn't exist"""
    try:
        if os.path.isdir(filename):
            for root, dirs, files in os.walk(filename, topdown=True):
                for f in files:
                    fp = os.path.join(root, f)
                    safe_unlink(fp)
                os.rmdir(root)
        else:
            if os.path.exists(filename):
                os.unlink(filename)
    except OSError, e:
        # Ignore "No such file or directory"
        if e.errno == 2:
            return
        else:
            raise


def safe_copyfile(src, dest):
    """safely copy src to dest using a temporary intermediate and then renaming
    to dest"""
    fd, tmpname = tempfile.mkstemp(dir=os.path.dirname(dest))
    shutil.copyfileobj(open(src, 'rb'), os.fdopen(fd, 'wb'))
    shutil.copystat(src, tmpname)
    os.rename(tmpname, dest)


def load_config(filename):
    config = RawConfigParser()
    if config.read([filename]) != [filename]:
        return None
    return config


def get_config(config, section, option, default):
    if config.has_section(section) and config.has_option(section, option):
        return config.get(section, option)
    return default


def get_config_int(config, section, option, default):
    if config.has_section(section) and config.has_option(section, option):
        return config.getint(section, option)
    return default


def get_config_bool(config, section, option, default):
    if config.has_section(section) and config.has_option(section, option):
        return config.getboolean(section, option)
    return default


def touch(filename, timestamp=None):
    """a command line touch, replacement"""
    with open(filename, 'a'):
        os.utime(filename, timestamp)
