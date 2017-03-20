import logging

from apache_conf_parser import ApacheConfParser

from release.platforms import ftp2bouncer

SCHEMA_2_OPTIONAL_ATTRIBUTES_SINGLE_VALUE = (
    'showPrompt', 'showNeverForVersion', 'showSurvey', 'licenseUrl',
    'billboardURL', 'openURL', 'notificationURL', 'alertURL', 'promptWaitTime',
)
SCHEMA_2_OPTIONAL_ATTRIBUTES_MULTI_VALUE = ('actions',)
SCHEMA_2_OPTIONAL_ATTRIBUTES = SCHEMA_2_OPTIONAL_ATTRIBUTES_SINGLE_VALUE + \
    SCHEMA_2_OPTIONAL_ATTRIBUTES_MULTI_VALUE
log = logging.getLogger()


def substitutePath(path, platform=None, locale=None, version=None):
    """Returns a platform/locale specific path based on a generic one."""
    subs = {
        'platform': platform,
        'locale': locale,
        'version': version,
        'bouncer-platform': ftp2bouncer(platform)
    }
    for sub, replacement in subs.items():
        if '%%%s%%' % sub in path:
            if replacement is None:
                raise TypeError("No substitution provided for '%s'" % sub)
            path = path.replace('%%%s%%' % sub, replacement)
    return path


class PatcherConfigError(ValueError):
    pass


class PatcherConfig(dict):
    def __init__(self, cfg=None):
        self['appName'] = None
        self['current-update'] = {}
        self['release'] = {}
        self['past-update'] = []
        if cfg:
            self.readXml(cfg)

    def _stripStringNode(self, n):
        """Returns the value section of a node, even if it contains whitespace."""
        return n.split(" ", 1)[1].strip()

    def getFromVersions(self):
        # From versions come from both the "from" value in current-update
        # as well as by analyzing the past-update section. The original patcher
        # scripts go so far as to analyze the channels in the past-update
        # sections to deal with cases where the list of channels was different
        # in earlier versions. These days we don't have that case so we simply
        # assume that all of the fromVersions in the past-update lines are
        # versions that should have update paths to the latest on all channels.
        return tuple(set([self['current-update']['from']] + [v[0] for v in
                                                             self['past-update']]))

    def getOptionalAttrs(self, version, locale):
        if version not in self['release']:
            log.debug("%s not found in config" % version)
            return {}
        schema = self['release'][version]['schema']
        # Currently, only schema 2 has optional attributes
        attrs = {}
        if schema == 2:
            if locale in self['current-update'].get('action-locales',
                                                    self['release'][version]['locales']):
                for attr in SCHEMA_2_OPTIONAL_ATTRIBUTES:
                    if attr in self['current-update']:
                        attrs[attr] = substitutePath(self['current-update'][attr],
                                                     locale=locale)
        return attrs

    def getPath(self, version, platform, locale, type_):
        if type_ == 'complete':
            path = self['current-update']['complete']['path']
        else:
            if version not in self['current-update']['partials']:
                raise PatcherConfigError(
                    "Couldn't find partial update path for %s" % version)
            path = self['current-update']['partials'][version]['path']
        return substitutePath(path, platform, locale, version)

    def getUrl(self, version, platform, locale, type_, channel):
        """Returns the final URL to a specific update. Completes come from
           the <complete> block. Partials come from their version-specific block
           inside of <partials>. In both cases if a [channel]-url exists, it
           will be returned. If it doesn't exist the generic url will be
           returned. If neither exists a PatcherConfigError will be raised.
           The returned URL will have %platform% and %locale% substitutions
           performed on it."""
        # Find a list of potential URLs....
        if type_ == 'complete':
            urls = self['current-update']['complete']
        else:
            if version not in self['current-update']['partials']:
                raise PatcherConfigError(
                    "Couldn't find partial update URL for %s" % version)
            urls = self['current-update']['partials'][version]
        # Look for a channel-specific one. If that doesn't exist, try the
        # generic one.
        try:
            url = urls.get('%s-url' % channel, urls['url'])
        except KeyError:
            raise PatcherConfigError("Couldn't find URL for (%s, %s, %s, %s, %s)" % (version, platform, locale, type_, channel))
        return substitutePath(url, platform, locale, version)

    def getUpdatePaths(self):
        """A generator that yields all of the update paths for this config.
           Each individual update path is a tuple of:
            (fromVersion, platform, locale, updateTypes, channels)

           fromVersion, platform, and locale are all single-value strings.
           updateTypes is a list of one or both of 'complete' and 'partial'
           channels is a list channels the update path is applicable to
        """
        if not self['current-update']:
            return

        fromVersions = self.getFromVersions()
        channels = tuple(self['current-update']['testchannel'] +
                         self['current-update']['channel'])
        toLocales = self['release'][self['current-update']['to']]['locales']
        toExceptions = self['release'][self['current-update']['to']]['exceptions']

        # Now that we know all of the versions that need updates to the latest
        # we can start yielding the paths.
        for version in fromVersions:
            r = self['release'][version]
            for platform in r['platforms']:
                p = r['platforms'][platform]
                for locale in r['locales']:
                    # The exception lists are a little weird. If a locale is
                    # not in the exception list at all, it is applicable to all
                    # platforms. If it *is* in the exception list it is only
                    # applicable to the platforms listed.
                    if locale in r['exceptions'] and platform not in r['exceptions'][locale]:
                        log.debug("Not generating update path for %s %s because it's not in the exception list for old release." % (platform, locale))
                        continue
                    # Make sure the locale is in the "to" release. If it's not
                    # we shouldn't generate an update for it!
                    if locale not in toLocales:
                        log.debug('Not generating update path for %s %s %s because %s isn\'t a locale for the "to" version' % (version, platform, locale, locale))
                        continue
                    # Make sure the locale isn't an exception in the "to" release
                    if locale in toExceptions and platform not in toExceptions[locale]:
                        log.debug("Not generating update path for %s %s because it's not in the exception list for new release." % (platform, locale))
                        continue
                    # Some patcher configs will have a <partial> block for
                    # backwards compatibility purposes. We don't support that
                    # though, so if a partial update isn't mentioned in
                    # the partials section we don't know anything about it.
                    if 'partials' in self['current-update'] and version in self['current-update']['partials']:
                        types = ('complete', 'partial')
                    else:
                        types = ('complete',)
                    yield (version, platform, locale, channels, types)

    def addPastUpdate(self, value):
        for existing in self['past-update']:
            if value[0] == existing[0]:
                raise PatcherConfigError("Found multiple past-updates with duplicate to/from versions: %s" % value)
        self['past-update'].append(value)

    def addRelease(self, version, value):
        if version in self['release']:
            raise PatcherConfigError(
                "Found multiple releases with same version: %s" % version)
        self['release'][version] = value

    def readXml(self, cfg):
        """Reads a patcher config file translating into a dictionary.
           This method isn't capable of parsing all of the patcher configs
           that its Perl ancestor can because we don't need to support some
           older features anymore. Scenarios known not to work are:
            - Multiple <app> subsections.
            - Channel differences between past-update lines
            - <partial> is ignored (replaced by <partials>)
            - <rc> is ignored
        """
        # Read the config, set the appName
        c = ApacheConfParser(cfg, infile=False).nodes[0]
        if not c.body.nodes:
            raise PatcherConfigError("No app found in config.")
        if len(c.body.nodes) > 1:
            raise PatcherConfigError("Too many apps found (only 1 allowed).")
        app = c.body.nodes[0]
        self['appName'] = app.name

        # Parse the config, with the heavy lifting in helper methods.
        for node in app.body.nodes:
            if node.name == 'current-update':
                if self['current-update']:
                    raise PatcherConfigError(
                        "Found multiple current-update blocks.")
                self['current-update'] = self.parseCurrentUpdate(
                    node.body.nodes)
            elif node.name == 'past-update':
                self.addPastUpdate(self.parsePastUpdate(list(node.arguments)))
            elif node.name == 'release':
                for releaseNode in node.body.nodes:
                    self.addRelease(releaseNode.name,
                                    self.parseRelease(releaseNode.body.nodes))

        # Now, a bunch of verifications
        # Make sure we have a current-update
        if not self['current-update']:
            raise PatcherConfigError(
                "Required section current-update is empty")
        # Make sure required nodes exist in <current-update> and <release>
        # nodes.
        for node in ('channel', 'testchannel', 'complete', 'details', 'from', 'to'):
            if node not in self['current-update']:
                raise PatcherConfigError(
                    "Required node '%s' not found in current-update" % node)
        for version, releaseNode in self['release'].items():
            for node in ('locales', 'version', 'platforms'):
                if node not in releaseNode:
                    raise PatcherConfigError("Required node '%s' not found in release node '%s'" % (node, version))
        # Make sure that versions mentioned have a release block.
        if self['current-update']['to'] not in self['release']:
            raise PatcherConfigError("No release found for version '%s'" %
                                     self['current-update']['to'])
        if self['current-update']['from'] not in self['release']:
            raise PatcherConfigError("No release found for version '%s'" %
                                     self['current-update']['from'])
        for version in self['current-update'].get('partials', {}):
            if version not in self['release']:
                raise PatcherConfigError(
                    "No release found for version '%s'" % version)
        for version in self['past-update']:
            if version[0] not in self['release']:
                raise PatcherConfigError(
                    "No release found for version '%s'" % version[0])

    def parsePastUpdate(self, pastUpdate):
        # A past-update node is a single block of text in the format:
        # past-update from to channel...
        if len(pastUpdate) < 3:
            raise PatcherConfigError(
                "Too few elements in past-update block: %s" % pastUpdate)
        return [pastUpdate[0], pastUpdate[1], pastUpdate[2:]]

    def parseCurrentUpdate(self, currentUpdate):
        cu = {}
        for node in currentUpdate:
            # force is the only thing we're allowed to have more than once
            if node.name != 'force' and node.name in cu:
                raise PatcherConfigError("Found multiple entries for '%s' in current-update section" % node.name)

            # These are all single-value nodes.
            if node.name in ('details', 'from', 'to', 'beta-dir') + \
                    SCHEMA_2_OPTIONAL_ATTRIBUTES_SINGLE_VALUE:
                cu[node.name] = self._stripStringNode(node.content)
            # These are potentially multiple value nodes.
            elif node.name in ('channel', 'testchannel', 'action-locales') + \
                    SCHEMA_2_OPTIONAL_ATTRIBUTES_MULTI_VALUE:
                cu[node.name] = list(node.arguments)
            # Force is weird in that it's a multiple value node, but the
            # patcher config bumping script lists it multiple times rather
            # than putting all entries on the same line.
            elif node.name in ('force',):
                if node.name not in cu:
                    cu[node.name] = []
                cu[node.name].append(self._stripStringNode(node.content))
            # The rest are subsections which are only allowed once.
            elif node.name in ('complete', 'partials'):
                cu[node.name] = {}
                for subNode in node.body.nodes:
                    if subNode.name in cu[node.name]:
                        raise PatcherConfigError("Found multiple entries for '%s' in current-update's %s section" % (subNode.name, node.name))
                    # The partials sections has an additional level of depth.
                    # After that level, everything is single-value nodes.
                    if node.name in ('partials',):
                        cu[node.name][subNode.name] = {}
                        for deepNode in subNode.body.nodes:
                            if deepNode.name in cu[node.name][subNode.name]:
                                raise PatcherConfigError("Found multiple entries for '%s' in current-update's %s/%s section" % (deepNode.name, node.name, subNode.name))
                            cu[node.name][subNode.name][deepNode.name] = self._stripStringNode(deepNode.content)
                    # The other sections only contain single-value nodes.
                    else:
                        cu[node.name][subNode.name] = self._stripStringNode(
                            subNode.content)
        return cu

    def parseRelease(self, release):
        r = {}
        for node in release:
            if node.name in r:
                raise PatcherConfigError("Found multiple entries for '%s' in release section" % node.name)

            # These are all single-value nodes.
            if node.name in ('checksumsurl', 'completemarurl',
                             'extension-version', 'prettyVersion', 'version',
                             'mar-channel-ids'):
                r[node.name] = self._stripStringNode(node.content)
            elif node.name == 'schema':
                r['schema'] = int(node.arguments[0])
            # This is a potentially multiple value node.
            elif node.name == 'locales':
                r['locales'] = list(node.arguments)
            # The rest are subsections.
            elif node.name in ('exceptions', 'platforms'):
                r[node.name] = {}
                for subNode in node.body.nodes:
                    if subNode.name in r[node.name]:
                        raise PatcherConfigError("Found multiple entries for '%s' in a release's %s section" % (subNode.name, node.name))
                    # Nodes in this section are comma-separated lists.
                    # (Unlike every other list in this file.)
                    if node.name in ('exceptions',):
                        r[node.name][subNode.name] = [
                            p.strip(',') for p in subNode.arguments]
                    # Nodes in this section are space-separated.
                    elif node.name in ('platforms',):
                        r[node.name][subNode.name] = int(subNode.arguments[0])
        return r
