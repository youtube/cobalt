import re

from release.info import isFinalRelease


class BuildVersionsException(Exception):
    pass

# Versions that match this should not be bumped
DO_NOT_BUMP_REGEX = '^\d\.\d(pre)?$'

# Regex that matches all possible versions and milestones
ANY_VERSION_REGEX =\
    ('(\d+\.\d[\d\.]*)'    # A version number
     '((a|b)\d+)?'        # Might be an alpha or beta
     '(esr)?'             # Might be an esr
     '(pre)?')            # Might be a 'pre' (nightly) version

# NB: If a file could match more than one of the regexes below the behaviour
# will be undefined, because the key order will differ from system to system
# and possibly run to run. Try to avoid this.
BUMP_FILES = {
    r'^.*(version.*\.txt|milestone\.txt)$': '^%(version)s$',
    r'^.*confvars\.sh$': '^MOZ_APP_VERSION=%(version)s$'
}


def bumpFile(filename, contents, version):
    # First, find the right regex for this file
    newContents = []
    for fileRegex, versionRegex in BUMP_FILES.iteritems():
        if re.match(fileRegex, filename):
            # Second, find the line with the version in it
            for line in contents.splitlines():
                regex = versionRegex % {'version': ANY_VERSION_REGEX}
                match = re.match(regex, line)
                # If this is the version line, and the file doesn't have
                # the correct version, change it.
                if match and match.group() != version:
                    newContents.append(
                        re.sub(ANY_VERSION_REGEX, version, line))
                # If it's not the version line, or the version is correct,
                # don't do anything
                else:
                    newContents.append(line)
            newContents = "\n".join(newContents)
            # Be sure to preserve trailing newlines, if they exist
            if contents.endswith("\n"):
                newContents += "\n"
            break
    if len(newContents) == 0:
        raise BuildVersionsException("Don't know how to bump %s" % filename)
    return newContents


def nextVersion(version, pre=False):
    """Returns the version directly after `version', optionally with "pre"
       appended to it."""
    if re.match(DO_NOT_BUMP_REGEX, version):
        bumped = version
    else:
        bumped = increment(version)
    if pre:
        bumped += "pre"
    return bumped

# The following function was copied from http://code.activestate.com/recipes/442460/
# Written by Chris Olds
lastNum = re.compile(r'(?:[^\d]*(\d+)[^\d]*)+')


def increment(s):
    """ look for the last sequence of number(s) in a string and increment """
    m = lastNum.search(s)
    if m:
        next = str(int(m.group(1)) + 1)
        start, end = m.span(1)
        s = s[:max(end - len(next), start)] + next + s[end:]
    return s


def getPossibleNextVersions(version):
    """Return possibly next versions for a given version.
       There's a few distinct cases here:
       * ESRs:  The only possible next version is the next minor version.
                Eg: 17.0.3esr -> 17.0.4esr
       * Betas: The next beta with the same major version and also the next
                major version's beta 1. Eg: 18.0b4 -> 18.0b5, 19.0b1
       * Other: The next major version's .0 release and the next minor version.
                Eg: 15.0 -> 15.0.1, 16.0; 17.0.2 -> 17.0.3, 18.0

       Versions with 'pre' are deprecated, and explicitly not supported.
    """
    ret = set()
    # Get the parts we care about from the version. The last group is the 'pre'
    # tag, which doesn't affect our work.
    m = re.match(ANY_VERSION_REGEX, version)
    if not m:
        return ret
    base, beta, _, esr = m.groups()[:4]
    # The next major version is used in a couple of places, so we figure it out
    # ahead of time. Eg: 17.0 -> 18.0 or 15.0.3 -> 16.0
    nextMajorVersion = increment(base.split('.')[0]) + '.0'
    # Modern ESRs have two possibilities:
    # 1) Bump the second digit for a planned release and reset the third digit
    #    to 0.
    # 2) Bump the last digit for an unexpected release
    #
    # Prior to ESR 24 we did #2 for all types of releases.
    if esr:
        # if version is like N.0esr, add an extra 0 to make it bump properly
        if version.count('.') < 2:
            version = version.replace('esr', '.0esr')
        first, second, _ = version.split('.', 2)
        if int(first) >= 24:
            ret.add('%s.%s.0esr' % (first, increment(second)))
        ret.add(increment(version))
    # Betas are similar, except we need the next major version's beta 1, too.
    elif beta:
        ret.add(increment(version))
        ret.add('%sb1' % nextMajorVersion)
    # Other releases are a bit more complicated, because we need to handle
    # going from a x.y -> x.y.z version number.
    else:
        ret.add(nextMajorVersion)
        if isFinalRelease(version):
            ret.add('%s.1' % version)
        else:
            ret.add(increment(version))
    return ret
