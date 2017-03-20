import urlparse
from paths import makeCandidatesDir, makeReleasesDir
from platforms import buildbot2ftp
from download import url_exists


class Partial(object):
    """Models a partial update, used in release_sanity"""
    def __init__(self, product, version, buildNumber, protocol='http',
                 server='ftp.mozilla.org'):
        self.product = product
        self.version = version
        self.buildNumber = buildNumber
        self.server = server
        self.protocol = protocol

    def short_name(self):
        name = [self.product, str(self.version)]
        # e.g. firefox 123.0b4
        if self.buildNumber is not None:
            name.extend(['build', str(self.buildNumber)])
            # e.g. firefox 123.0b4 build 5
        return " ".join(name)

    def _is_from_candidates_dir(self):
        return self.buildNumber is not None

    def complete_mar_name(self):
        return '%s-%s.complete.mar' % (self.product, self.version)

    def complete_mar_url(self, platform):
        ftp_platform = buildbot2ftp(platform)
        url = makeReleasesDir(self.product, self.version,
                              protocol=self.protocol,
                              server=self.server)
        if self._is_from_candidates_dir():
            url = makeCandidatesDir(product=self.product,
                                    version=self.version,
                                    buildNumber=self.buildNumber,
                                    nightlyDir='candidates',
                                    server=self.server,
                                    protocol=self.protocol)
        url = "/".join([url, 'update', ftp_platform,
                        'en-US', self.complete_mar_name()])
        # remove any double // in url
        return urlparse.urljoin(url,
                                urlparse.urlparse(url).path.replace('//', '/'))

    def exists(self, platform):
        return url_exists(self.complete_mar_url(platform))

    def __str__(self):
        return self.short_name()
