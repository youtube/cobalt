import logging
log = logging.getLogger(__name__)

from release.info import getBaseTag
from release.versions import getAppVersion


def substituteReleaseConfig(config, product, version, **other):
    from jinja2 import Environment, FunctionLoader, StrictUndefined

    baseTag = getBaseTag(product, version)
    appVersion = getAppVersion(version)

    environment = Environment(undefined=StrictUndefined)
    template = environment.from_string(config)
    return template.render(product=product, version=version, baseTag=baseTag,
                           appVersion=appVersion, **other)
