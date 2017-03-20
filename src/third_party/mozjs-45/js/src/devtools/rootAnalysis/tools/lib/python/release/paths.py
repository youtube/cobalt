from urlparse import urlunsplit

product_ftp_map = {
    'fennec': 'mobile',
}

def product2ftp(product):
    return product_ftp_map.get(product, product)


def makeCandidatesDir(product, version, buildNumber, nightlyDir='candidates',
                      protocol=None, server=None, ftp_root='/pub/mozilla.org/'):
    if protocol:
        assert server is not None, "server is required with protocol"

    product = product2ftp(product)
    directory = ftp_root + product + '/' + nightlyDir + '/' + \
        str(version) + '-candidates/build' + str(buildNumber) + '/'

    if protocol:
        return urlunsplit((protocol, server, directory, None, None))
    else:
        return directory


def makeReleasesDir(product, version=None, protocol=None, server=None,
                    ftp_root='/pub/mozilla.org/'):
    if protocol:
        assert server is not None, "server is required with protocol"

    directory = '%s%s/releases/' % (ftp_root, product)
    if version:
        directory += '%s/' % version

    if protocol:
        return urlunsplit((protocol, server, directory, None, None))
    else:
        return directory
