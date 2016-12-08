import re

from build.versions import ANY_VERSION_REGEX


def getAppVersion(version):
    return re.match(ANY_VERSION_REGEX, version).group(1)


def getPrettyVersion(version):
    version = re.sub(r'a([0-9]+)$', r' Alpha \1', version)
    version = re.sub(r'b([0-9]+)$', r' Beta \1', version)
    version = re.sub(r'rc([0-9]+)$', r' RC \1', version)
    return version


def getL10nDashboardVersion(version, product, parse_version=True):
    if product == 'firefox':
        ret = 'fx'
    elif product == 'fennec':
        ret = 'fennec'
    elif product == 'thunderbird':
        ret = 'tb'
    elif product == 'seamonkey':
        ret = 'sea'

    if not parse_version:
        ret += version
    else:
        parsed = re.match(ANY_VERSION_REGEX, version)
        if parsed.group(2) and parsed.group(2).startswith('b'):
            ret = '%s%s_beta_%s' % (
                ret, version.split(".")[0], parsed.group(2))
        else:
            ret += version
    return ret
