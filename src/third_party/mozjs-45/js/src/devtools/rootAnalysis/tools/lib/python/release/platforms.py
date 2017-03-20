# buildbot -> bouncer platform mapping
# 64-bit windows
try:
    import simplejson as json
except:
    import json
bouncer_platform_map = {'win32': 'win', 'win64': 'win64', 'macosx': 'osx',
                        'linux': 'linux', 'linux64': 'linux64',
                        'macosx64': 'osx'}
# buildbot -> ftp platform mapping
ftp_platform_map = {'win32': 'win32', 'win64': 'win64', 'macosx': 'mac',
                    'linux': 'linux-i686', 'linux64': 'linux-x86_64',
                    'macosx64': 'mac', 'linux-android': 'android',
                    'linux-mobile': 'linux', 'macosx-mobile': 'macosx',
                    'win32-mobile': 'win32', 'android': 'android',
                    'android-xul': 'android-xul'}
# buildbot -> shipped-locales platform mapping
# TODO: make sure 'win64' is correct when shipped-locales becomes aware of it
sl_platform_map = {'win32': 'win32', 'win64': 'win32', 'macosx': 'osx',
                   'linux': 'linux', 'linux64': 'linux', 'macosx64': 'osx'}
# buildbot -> update platform mapping
update_platform_map = {
    'android': ['Android_arm-eabi-gcc3'],
    'android-api-9': ['Android_arm-eabi-gcc3'],
    'android-api-11': ['Android_arm-eabi-gcc3'],
    'android-x86': ['Android_x86-gcc3'],
    'linux': ['Linux_x86-gcc3'],
    'linux64': ['Linux_x86_64-gcc3'],
    'macosx64': ['Darwin_x86_64-gcc3-u-i386-x86_64',  # The main platofrm
                 'Darwin_x86-gcc3-u-i386-x86_64',
                # We don't ship builds with these build targets, but some users
                # modify their builds in a way that has them report like these.
                # See bug 1071576 for details.
                 'Darwin_x86-gcc3', 'Darwin_x86_64-gcc3'],
    'win32': ['WINNT_x86-msvc', 'WINNT_x86-msvc-x86', 'WINNT_x86-msvc-x64'],
    'win64': ['WINNT_x86_64-msvc', 'WINNT_x86_64-msvc-x64'],

    # This is for v2.1 and v2.2 branches.
    # Must be killed in the future.
    'flame-kk': ['flame-kk','flame'],

    # BUILD_TARGET follows the pattern <platform>-<variant>-<sdk-version>
    # We add the proper aliases for backward compatibility
    'flame-user-kk': ['flame-user-kk', 'flame-kk', 'flame'],
    'aries-user-kk': ['aries-user-kk', 'aries'],
}

# These FTP -> other mappings are provided so that things interpreting patcher
# configs can figure update/bouncer platforms without using the Buildbot
# platform as an intermediary. (It's difficult to do that, because ftp:buildbot
# is a many:1 mapping, so some guesswork ends up being involved.) In the future
# when we redesign the patcher configs we can use Buildbot platform in them and
# rid ourselves of these mappings. (bug 778125)
ftp_update_platform_map = {
    'linux-i686': update_platform_map['linux'],
    'linux-x86_64': update_platform_map['linux64'],
    'mac': update_platform_map['macosx64'],
    'win32': update_platform_map['win32'],
    'win64': update_platform_map['win64'],
}

ftp_bouncer_platform_map = {
    'linux-i686': 'linux',
    'linux-x86_64': 'linux64',
    'mac': 'osx',
    'win32': 'win',
    'win64': 'win64',
}


def buildbot2bouncer(platform):
    return bouncer_platform_map.get(platform, platform)


def buildbot2ftp(platform):
    return ftp_platform_map.get(platform, platform)


def buildbot2shippedlocales(platform):
    return sl_platform_map.get(platform, platform)


def shippedlocales2buildbot(platform):
    matches = []
    try:
        [matches.append(
            k) for k, v in sl_platform_map.iteritems() if v == platform][0]
        return matches
    except IndexError:
        return [platform]


def buildbot2updatePlatforms(platform):
    return update_platform_map.get(platform, [platform])


def ftp2updatePlatforms(platform):
    return ftp_update_platform_map.get(platform, platform)


def ftp2bouncer(platform):
    return ftp_bouncer_platform_map.get(platform, platform)


def getPlatformLocales(shipped_locales, platforms):
    platform_locales = {}
    for platform in platforms:
        platform_locales[platform] = []
    for line in shipped_locales.splitlines():
        entry = line.strip().split()
        locale = entry[0]
        if len(entry) > 1:
            for platform in entry[1:]:
                for bb_platform in shippedlocales2buildbot(platform):
                    if bb_platform in platforms:
                        platform_locales[bb_platform].append(locale)
        else:
            for platform in platforms:
                platform_locales[platform].append(locale)
    return platform_locales


def getLocaleListFromShippedLocales(shipped_locales):
    """ return the list of locales in shipped_locales, without platform specific info """
    shipped_locales_list = []
    for line in shipped_locales.splitlines():
        entry = line.strip().split()
        shipped_locales_list.append(entry[0])
    return shipped_locales_list


def getPlatformLocalesFromJson(json_file, platforms):
    platform_locales = {}
    for platform in platforms:
        platform_locales[platform] = []
    fh = open(json_file)
    json_contents = json.load(fh)
    fh.close()
    for locale in json_contents.keys():
        for platform in json_contents[locale]["platforms"]:
            if platform not in platform_locales:
                platform_locales[platform] = []
            platform_locales[platform].append(locale)
    return platform_locales


def getAllLocales(shipped_locales):
    locales = []
    f = open(shipped_locales)
    for line in f.readlines():
        entry = line.split()
        locale = entry[0]
        if locale:
            locales.append(locale)
    f.close()
    return locales


def getPlatforms():
    return bouncer_platform_map.keys()


def getSupportedPlatforms():
    return ('linux', 'linux64', 'win32', 'win64', 'macosx', 'macosx64')
