import os
import time
import tempfile
import shlex
import shutil
import fnmatch
import re
# TODO: Use util.command
from subprocess import Popen, PIPE, STDOUT, check_call, call

from util.archives import unpacktar, tar_dir, MAR, SEVENZIP
from release.info import fileInfo

import logging
log = logging.getLogger(__name__)

MAC_DESIGNATED_REQUIREMENTS = """\
=designated => ( (anchor apple generic and certificate leaf[field.1.2.840.113635.100.6.1.9] ) or (anchor apple generic and certificate 1[field.1.2.840.113635.100.6.2.6] and certificate leaf[field.1.2.840.113635.100.6.1.13] and certificate leaf[subject.OU] = "%(subject_ou)s"))
"""


def signfile(filename, keydir, fake=False, passphrase=None, timestamp=True):
    """Perform authenticode signing on the given file with keys in keydir.

    If passphrase is set, it will be sent as stdin to the process.

    If fake is True, then don't actually sign anything, just sleep for a
    second to simulate signing time.

    If timestamp is True, then a signed timestamp will be included with the
    signature."""
    if fake:
        time.sleep(1)
        return
    basename = os.path.basename(filename)
    dirname = os.path.dirname(filename)
    stdout = tempfile.TemporaryFile()
    command = ['signcode',
               '-spc', '%s/MozAuthenticode.spc' % keydir,
               '-v', '%s/MozAuthenticode.pvk' % keydir,
               ]
    if timestamp:
        command.extend(
            ['-t', 'http://timestamp.verisign.com/scripts/timestamp.dll'])
    command.extend([
        '-i', 'http://www.mozilla.com',
        '-a', 'sha1',
        # Try 5 times, and wait 60 seconds between tries
        '-tr', '5',
        '-tw', '60',
        basename])
    try:
        log.debug("Running %s", command)
        proc = Popen(
            command, cwd=dirname, stdout=stdout, stderr=STDOUT, stdin=PIPE)
        if passphrase:
            proc.stdin.write(passphrase)
        proc.stdin.close()
        if proc.wait() != 0:
            raise ValueError("signcode didn't return with 0")
        stdout.seek(0)
        data = stdout.read()
        # Make sure that the command output "Succeeded".  Sometimes signcode
        # returns with 0, but doesn't output "Succeeded", which in the past has
        # meant that the file has been signed, but is missing a timestmap.
        if data.strip() != "Succeeded" and "Success" not in data:
            raise ValueError("signcode didn't report success")
    except:
        stdout.seek(0)
        data = stdout.read()
        log.exception(data)
        raise


def osslsigncode_signfile(inputfile, outputfile, keydir, fake=False, passphrase=None, timestamp=None):
    """Perform Authenticode signing on "inputfile", writing a signed version
    to "outputfile". See signcode_signfile for a description of other
    arguments.

    See https://bugzilla.mozilla.org/show_bug.cgi?id=711210#c15 for background
    on why we want both methods.
    """
    if fake:
        time.sleep(1)
        return

    stdout = tempfile.TemporaryFile()
    args = [
        '-certs', '%s/MozAuthenticode.spc' % keydir,
        '-key', '%s/MozAuthenticode.pvk' % keydir,
        '-i', 'http://www.mozilla.com',
        '-h', 'sha1',
        '-in', inputfile,
        '-out', outputfile,
    ]
    if timestamp:
        args.extend(['-t', 'http://timestamp.verisign.com/scripts/timestamp.dll'])

    try:
        import pexpect
        proc = pexpect.spawn('osslsigncode', args)
        # We use logfile_read because we only want stdout/stderr, _not_ stdin.
        proc.logfile_read = stdout
        proc.expect('Enter PEM pass phrase')
        proc.sendline(passphrase)
        proc.wait()
    except:
        stdout.seek(0)
        data = stdout.read()
        log.exception(data)
        raise


def gpg_signfile(filename, sigfile, gpgdir, fake=False, passphrase=None):
    """Sign the given file with the default key from gpgdir. The signature is
    written to sigfile.

    If fake is True, generate a fake signature and sleep for a bit.

    If passphrase is set, it will be passed to gpg on stdin
    """
    if fake:
        open(sigfile, "wb").write("""
-----BEGIN FAKE SIGNATURE-----
Version: 1.2.3.4

I am ur signature!
-----END FAKE SIGNATURE-----""")
        time.sleep(1)
        return

    command = ['gpg', '--homedir', gpgdir, '-bsa', '-o', sigfile, '-q',
               '--batch']
    if passphrase:
        command.extend(['--passphrase-fd', '0'])
    command.append(filename)
    log.info('Running %s', command)
    stdout = tempfile.TemporaryFile()
    try:
        proc = Popen(command, stdout=stdout, stderr=STDOUT, stdin=PIPE)
        if passphrase:
            proc.stdin.write(passphrase)
        proc.stdin.close()
        if proc.wait() != 0:
            raise ValueError("gpg didn't return 0")
        stdout.seek(0)
        data = stdout.read()
    except:
        stdout.seek(0)
        data = stdout.read()
        log.exception(data)
        raise


def emevoucher_signfile(inputfile, outputfile, key, chain, fake=False, passphrase=None):
    """Perform SMIME signing on "inputfile", writing a signed version
    to "outputfile", using passed in "key". This is necessary for the EME voucher.

    If fake is True, generate a fake signature and sleep for a bit.

    If passphrase is set, it will be passed to gpg on stdin
    """
    if fake:
        time.sleep(1)
        return

    stdout = tempfile.TemporaryFile()
    args = ['smime', '-sign', '-in', inputfile,
            '-out', outputfile, '-signer', key,
            '-certfile', chain,
            '-md', 'sha256', '-binary', '-nodetach',
            '-outform', 'DER']

    try:
        import pexpect
        proc = pexpect.spawn("openssl", args)
        # We use logfile_read because we only want stdout/stderr, _not_ stdin.
        proc.logfile_read = stdout
        proc.expect('Enter pass phrase')
        proc.sendline(passphrase)
        if proc.wait() != 0:
            raise ValueError("openssl didn't return 0")
    except:
        stdout.seek(0)
        data = stdout.read()
        log.exception(data)
        raise


def mar_signfile(inputfile, outputfile, mar_cmd, fake=False, passphrase=None):
    # Now sign it
    if isinstance(mar_cmd, basestring):
        mar_cmd = shlex.split(mar_cmd)
    else:
        mar_cmd = mar_cmd[:]
    command = mar_cmd + [inputfile, outputfile]
    log.info('Running %s', command)
    stdout = tempfile.TemporaryFile()
    try:
        proc = Popen(command, stdout=stdout, stderr=STDOUT, stdin=PIPE)
        if passphrase:
            proc.stdin.write(passphrase)
            proc.stdin.write("\n")
        proc.stdin.close()
        if proc.wait() != 0:
            raise ValueError("mar didn't return 0")
        stdout.seek(0)
        data = stdout.read()
    except:
        stdout.seek(0)
        data = stdout.read()
        log.exception(data)
        raise


def jar_unsignfile(filename):
    # Find all the files in META-INF
    command = ['unzip', '-l', filename]
    stdout = tempfile.TemporaryFile()
    log.debug("running %s", command)
    proc = Popen(command, stdout=stdout, stderr=STDOUT, stdin=PIPE)
    if proc.wait() != 0:
        stdout.seek(0)
        data = stdout.read()
        log.error("unzip output: %s", data)
        raise ValueError("Couldn't list zip contents")

    stdout.seek(0)
    data = stdout.read()
    meta_files = re.findall(r'\b(META-INF/.*)$', data, re.I + re.M)
    if not meta_files:
        # Nothing to do
        return

    # Now delete them
    command = ['zip', filename, '-d'] + meta_files
    stdout = tempfile.TemporaryFile()
    log.debug("running %s", command)
    proc = Popen(command, stdout=stdout, stderr=STDOUT, stdin=PIPE)
    # Zip returns with 0 in normal operatoin
    # it returns with 12 if it has nothing to do
    if proc.wait() not in (0, 12):
        stdout.seek(0)
        data = stdout.read()
        log.error("zip output: %s", data)
        raise ValueError("Couldn't remove previous signature")


def jar_signfile(filename, keystore, keyname, fake=False, passphrase=None):
    """Sign a jar file
    """
    # unsign first
    jar_unsignfile(filename)
    command = ["jarsigner", "-keystore", keystore, filename]
    if keyname:
        command.append(keyname)
    stdout = tempfile.TemporaryFile()
    try:
        log.debug("running %s", command)
        proc = Popen(command, stdout=stdout, stderr=STDOUT, stdin=PIPE)
        if passphrase:
            passphrases = passphrase.split(' ')
            for p in passphrases:
                proc.stdin.write(p)
                proc.stdin.write("\n")
        proc.stdin.close()
        if proc.wait() != 0:
            stdout.seek(0)
            data = stdout.read()
            log.error("jarsigner output: %s", data)
            raise ValueError("jarsigner didn't return 0")
    except:
        stdout.seek(0)
        data = stdout.read()
        log.exception(data)
        raise


def get_bundle_executable(appdir):
    """Return the CFBundleIdentifier from a Mac application."""
    import plistlib
    return plistlib.readPlist(os.path.join(appdir, 'Contents', 'Info.plist'))['CFBundleExecutable']


def dmg_signfile(filename, keychain, signing_identity, subject_ou, fake=False):
    """Sign a mac .app folder, including handling subapps within.
    """
    # Only some directories contain files that get explicitly signed by
    # codesign. Others (such as .app/Contents/Resources) have their checksums
    # embedded into the CodeResources file that codesign creates when the .app
    # is signed at the end.
    SIGN_DIRS = ("MacOS", "Library")
    log.debug("Signing %s", filename)
    app = os.path.basename(filename)
    appdir = os.path.dirname(filename)
    app_executable = get_bundle_executable(filename)
    stdout = tempfile.TemporaryFile()

    sign_command = ['codesign',
                    '-s', signing_identity, '-fv',
                    '--keychain', keychain,
                    '--requirement', MAC_DESIGNATED_REQUIREMENTS % locals()]

    # We scan "Contents" here to make it easier to find inner apps
    # (by looking for things that end with ".app")
    contents_d = os.path.join(filename, "Contents")
    for top, dirs, files in os.walk(contents_d):
        for d in dirs:
            abs_d = os.path.join(top, d)
            # We only want to sign individual files in certain directories.
            # Removing items from "dirs" prevents walk() from descending into
            # those directories.
            # See SIGN_DIRS above for further explanation.
            if top == contents_d and d not in SIGN_DIRS:
                log.debug("Skipping %s because it's not in SIGN_DIRS.", abs_d)
                dirs.remove(d)
                continue
            # Make sure to sign any inner apps, too
            if d.endswith(".app"):
                dmg_signfile(abs_d, keychain, signing_identity, subject_ou, fake)

        # None of the individual files in Contents (eg, Info.plist) need
        # explicit signing. They get taken care of during the overall .app
        # signing, so there's no point in iterating over files if we're still
        # in the root directory.
        if top == contents_d:
            log.debug("Skipping file iteration in %s because it's the root directory.", top)
            continue

        for f in files:
            abs_f = os.path.join(top, f)
            # If we find any ".app" after the app we're currently signing
            # this means it's an inner app, which was handled above.
            # We shouldn't do any further processing here.
            if top[len(filename):].count(".app") > 0:
                log.debug("Skipping %s because it's part an inner app.", abs_f)
                continue
            # We also need to skip the main executable, because this gets
            # signed with the outer package, which happens below.
            if f == app_executable:
                log.debug("Skipping %s because it's the main executable.", abs_f)
                continue
            try:
                dir_ = os.path.dirname(abs_f)
                log.debug("Signing with command: %s %s", " ".join(sign_command), f)
                check_call(sign_command + [f], cwd=dir_, stdout=stdout, stderr=STDOUT)
            except:
                stdout.seek(0)
                data = stdout.read()
                log.exception(data)
                raise

    try:
        log.debug("Signing with command: %s %s", " ".join(sign_command), app)
        check_call(sign_command + [app], cwd=appdir, stdout=stdout, stderr=STDOUT)
    except:
        stdout.seek(0)
        data = stdout.read()
        log.exception(data)
        raise


def dmg_signpackage(pkgfile, dstfile, keychain, mac_id, subject_ou, fake=False, passphrase=None):
    """ Sign a mac build, putting results into `dstfile`.
        pkgfile must be a tar, which gets unpacked, signed, and repacked.
    """
    # Keep track of our output in a list here, and we can output everything
    # when we're done This is to avoid interleaving the output from
    # multiple processes.
    from flufl.lock import Lock, TimeOutError, NotLockedError
    from datetime import timedelta
    import pexpect

    # TODO: Is it even possible to do 'fake' signing?
    logs = []
    logs.append("Repacking %s to %s" % (pkgfile, dstfile))

    # pexpect requires a string as input
    unlock_command = 'security unlock-keychain ' + keychain
    lock_command = ['security', 'lock-keychain', keychain]
    lockfile = os.path.join(os.path.dirname(keychain), '.lock')

    tmpdir = tempfile.mkdtemp()
    try:
        # Unpack it
        logs.append("Unpacking %s to %s" % (pkgfile, tmpdir))
        unpacktar(pkgfile, tmpdir)


        for macdir in os.listdir(tmpdir):
            macdir = os.path.join(tmpdir, macdir)
            log.debug('Checking if we should sign %s', macdir)
            if shouldSign(macdir, 'mac'):
                log.debug('Need to sign %s', macdir)

                try:
                    sign_lock = None
                    # Acquire a lock for the signing command, to ensure we don't have a
                    # race condition where one process locks the keychain immediately after another
                    # unlocks it.
                    log.debug("Try to acquire %s", lockfile)
                    sign_lock = Lock(lockfile)
                    # Put a 30 second timeout on waiting for the lock.
                    sign_lock.lock(timedelta(0, 30))

                    # Unlock the keychain so that we do not get a user-interaction prompt to use
                    # the keychain for signing. This operation requires a password.
                    child = pexpect.spawn(unlock_command)
                    child.expect('password to unlock .*')
                    child.sendline(passphrase)
                    # read output until child exits
                    child.read()
                    child.close()
                    if child.exitstatus != 0:
                        raise ValueError("keychain unlock failed")

                    # Sign the thing!
                    dmg_signfile(macdir, keychain, mac_id, subject_ou, fake)

                except TimeOutError:
                    # timed out acquiring lock, give an error
                    log.exception("Timeout acquiring lock  %s for codesign, is something broken? ", lockfile)
                    raise
                except:
                    # catch any other locking error
                    log.exception("Error acquiring  %s for codesign, is something broken?", lockfile)
                    raise
                finally:
                    # Lock the keychain again, no matter what happens
                    # This command does not require a password
                    check_call(lock_command)

                    # Release the lock, if it was acquired
                    if sign_lock:
                        try:
                            sign_lock.unlock()
                            log.debug("Release %s", lockfile)
                        except NotLockedError:
                            log.debug("%s was already unlocked", lockfile)


        # Repack it
        logs.append("Packing %s" % dstfile)
        tar_dir(dstfile, tmpdir)
    except:
        log.exception("Error signing %s", pkgfile)
        raise
    finally:
        # Clean up after ourselves, and output our logs
        shutil.rmtree(tmpdir)
        log.info("\n  ".join(logs))


def filterFiles(files, product):
    """ Filter out files that we can't sign. Right now this means that
    anything that isn't a win32 .exe or .mar file gets filtered out"""
    for f in files[:]:
        skip = False
        try:
            info = fileInfo(f, product)
            if info['platform'] != 'win32':
                skip = True
            if info['contents'] not in ('complete', 'installer'):
                skip = True
        except ValueError:
            skip = True

        if skip:
            files.remove(f)
            if 'win32' in f:
                if 'xpi' not in f:
                    log.info("Skipping %s", f)

    return files


def sortFiles(files, product, firstLocale):
    """sort files into a specific ordering using the key function defined
    within"""
    # Utility function for sorting files
    # Makes sure that .mar files follow their corresponding .exe files
    def fileKey(f):
        info = fileInfo(f, product)
        locale = info['locale']
        leading_path = info['leading_path']
        if locale == firstLocale and not leading_path:
            localeVal = 0
        else:
            localeVal = 1
        if info['format'] == 'exe':
            exeVal = 0
        else:
            exeVal = 1
        return (localeVal, leading_path, locale, exeVal, f)

    return sorted(files, key=fileKey)


def sums_are_equal(base_package, packages):
    """ Check to make sure that the dictionaries of files:checksums are
    exactly equal across a list of packages """
    success = True
    for signed_file in base_package.keys():
        log.debug("comparing file %s", signed_file)
        if not len([p for p in packages if p[signed_file] ==
                    packages[0][signed_file]]) == len(packages):
            log.error("%s differs!", signed_file)
            success = False
    return success


def shouldSign(filename, platform='win32'):
    """Returns True if filename should be signed."""
    # These should already be signed by Microsoft.
    _dont_sign = [
        'D3DCompiler_42.dll', 'd3dx9_42.dll',
        'D3DCompiler_43.dll', 'd3dx9_43.dll',
        'msvc*.dll',
    ]
    ext = os.path.splitext(filename)[1]
    b = os.path.basename(filename)
    if platform == 'mac':
        if b.endswith('.app'):
            return True
    elif platform in ('win32', 'win64'):
        if ext in ('.dll', '.exe') and not any(fnmatch.fnmatch(b, p) for p in _dont_sign):
            return True
    else:
        # We should never get here.
        log.debug("Invalid Platform: %s", platform)
    return False


def getChkFile(filename):
    _special_files = ['freebl3.dll', 'softokn3.dll', 'nssdbm3.dll']
    b = os.path.basename(filename)
    if b in _special_files:
        d = os.path.dirname(filename)
        f = os.path.splitext(b)[0] + '.chk'
        return os.path.join(d, f)
    return None


def checkTools():
    """Returns True if all of the helper commands ($MAR, $SEVENZIP) are
    runnable.

    Raises a OSError if they can't be found.
    """
    # Check that MAR and SEVENZIP are executable
    null = open(os.devnull, "w")
    try:
        call([MAR, '-h'], stdout=null)
    except OSError:
        raise OSError("mar must be in your $PATH, or set via $MAR")
    try:
        call([SEVENZIP, '-h'], stdout=null)
    except OSError:
        raise OSError("7z must be in your $PATH, or set via $SEVENZIP")
