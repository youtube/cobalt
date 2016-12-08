import os
# TODO: use util.commands
from subprocess import check_call
import logging
import tempfile
import bz2
import shutil

from util.paths import cygpath, findfiles

log = logging.getLogger(__name__)

SEVENZIP = os.environ.get('SEVENZIP', '7z')
MAR = os.environ.get('MAR', 'mar')
TAR = os.environ.get('TAR', 'tar')


def _noumask():
    # Utility function to set a umask of 000
    os.umask(0)


def unpackexe(exefile, destdir):
    """Unpack the given exefile into destdir, using 7z"""
    nullfd = open(os.devnull, "w")
    exefile = cygpath(os.path.abspath(exefile))
    try:
        check_call([SEVENZIP, 'x', exefile], cwd=destdir,
                   stdout=nullfd, preexec_fn=_noumask)
    except:
        log.exception("Error unpacking exe %s to %s", exefile, destdir)
        raise
    nullfd.close()


def packexe(exefile, srcdir):
    """Pack the files in srcdir into exefile using 7z.

    Requires that stub files are available in checkouts/stubs"""
    exefile = cygpath(os.path.abspath(exefile))
    appbundle = exefile + ".app.7z"

    # Make sure that appbundle doesn't already exist
    # We don't want to risk appending to an existing file
    if os.path.exists(appbundle):
        raise OSError("%s already exists" % appbundle)

    files = os.listdir(srcdir)

    SEVENZIP_ARGS = ['-r', '-t7z', '-mx', '-m0=BCJ2', '-m1=LZMA:d27',
                     '-m2=LZMA:d19:mf=bt2', '-m3=LZMA:d19:mf=bt2', '-mb0:1', '-mb0s1:2',
                     '-mb0s2:3', '-m1fb=128', '-m1lc=4']

    # First, compress with 7z
    stdout = tempfile.TemporaryFile()
    try:
        check_call([SEVENZIP, 'a'] + SEVENZIP_ARGS + [appbundle] + files,
                   cwd=srcdir, stdout=stdout, preexec_fn=_noumask)
    except:
        stdout.seek(0)
        data = stdout.read()
        log.error(data)
        log.exception("Error packing exe %s from %s", exefile, srcdir)
        raise
    stdout.close()

    # Then prepend our stubs onto the compressed 7z data
    o = open(exefile, "wb")
    parts = [
        'checkouts/stubs/7z/7zSD.sfx.compressed',
        'checkouts/stubs/tagfile/app.tag',
        appbundle
    ]
    for part in parts:
        i = open(part)
        while True:
            block = i.read(4096)
            if not block:
                break
            o.write(block)
        i.close()
    o.close()
    os.unlink(appbundle)


def bunzip2(filename):
    """Uncompress `filename` in place"""
    log.debug("Uncompressing %s", filename)
    tmpfile = "%s.tmp" % filename
    os.rename(filename, tmpfile)
    b = bz2.BZ2File(tmpfile)
    f = open(filename, "wb")
    while True:
        block = b.read(512 * 1024)
        if not block:
            break
        f.write(block)
    f.close()
    b.close()
    shutil.copystat(tmpfile, filename)
    shutil.copymode(tmpfile, filename)
    os.unlink(tmpfile)


def bzip2(filename):
    """Compress `filename` in place"""
    log.debug("Compressing %s", filename)
    tmpfile = "%s.tmp" % filename
    os.rename(filename, tmpfile)
    b = bz2.BZ2File(filename, "w")
    f = open(tmpfile, 'rb')
    while True:
        block = f.read(512 * 1024)
        if not block:
            break
        b.write(block)
    f.close()
    b.close()
    shutil.copystat(tmpfile, filename)
    shutil.copymode(tmpfile, filename)
    os.unlink(tmpfile)


def unpackmar(marfile, destdir):
    """Unpack marfile into destdir"""
    marfile = cygpath(os.path.abspath(marfile))
    nullfd = open(os.devnull, "w")
    try:
        check_call([MAR, '-x', marfile], cwd=destdir,
                   stdout=nullfd, preexec_fn=_noumask)
    except:
        log.exception("Error unpacking mar file %s to %s", marfile, destdir)
        raise
    nullfd.close()


def packmar(marfile, srcdir):
    """Create marfile from the contents of srcdir"""
    nullfd = open(os.devnull, "w")
    files = [f[len(srcdir) + 1:] for f in findfiles(srcdir)]
    marfile = cygpath(os.path.abspath(marfile))
    try:
        check_call(
            [MAR, '-c', marfile] + files, cwd=srcdir, preexec_fn=_noumask)
    except:
        log.exception("Error packing mar file %s from %s", marfile, srcdir)
        raise
    nullfd.close()


def unpacktar(tarfile, destdir):
    """ Unpack given tarball into the specified dir """
    nullfd = open(os.devnull, "w")
    tarfile = cygpath(os.path.abspath(tarfile))
    log.debug("unpack tar %s into %s", tarfile, destdir)
    try:
        check_call([TAR, '-xzf', tarfile], cwd=destdir,
                   stdout=nullfd, preexec_fn=_noumask)
    except:
        log.exception("Error unpacking tar file %s to %s", tarfile, destdir)
        raise
    nullfd.close()


def tar_dir(tarfile, srcdir):
    """ Pack a tar file using all the files in the given srcdir """
    files = os.listdir(srcdir)
    packtar(tarfile, files, srcdir)


def packtar(tarfile, files, srcdir):
    """ Pack the given files into a tar, setting cwd = srcdir"""
    nullfd = open(os.devnull, "w")
    tarfile = cygpath(os.path.abspath(tarfile))
    log.debug("pack tar %s from folder  %s with files ", tarfile, srcdir)
    log.debug(files)
    try:
        check_call([TAR, '-czf', tarfile] + files, cwd=srcdir,
                   stdout=nullfd, preexec_fn=_noumask)
    except:
        log.exception("Error packing tar file %s to %s", tarfile, srcdir)
        raise
    nullfd.close()


def unpackfile(filename, destdir):
    """Unpack a mar or exe into destdir"""
    if filename.endswith(".mar"):
        return unpackmar(filename, destdir)
    elif filename.endswith(".exe"):
        return unpackexe(filename, destdir)
    elif filename.endswith(".tar"):
        return unpacktar(filename, destdir)
    else:
        raise ValueError("Unknown file type: %s" % filename)


def packfile(filename, srcdir):
    """Package up srcdir into filename, archived with 7z for exes or mar for
    mar files"""
    if filename.endswith(".mar"):
        return packmar(filename, srcdir)
    elif filename.endswith(".exe"):
        return packexe(filename, srcdir)
    elif filename.endswith(".tar"):
        return tar_dir(filename, srcdir)
    else:
        raise ValueError("Unknown file type: %s" % filename)
