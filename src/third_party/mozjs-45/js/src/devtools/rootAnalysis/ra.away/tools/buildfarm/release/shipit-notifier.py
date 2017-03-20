#!/usr/bin/env python
"""shipit-agent.py [options]

Listens for buildbot release messages from pulse and then sends them
to the shipit REST API."""

import pytz
import site

from optparse import OptionParser
from ConfigParser import ConfigParser
from mozillapulse import consumers
from mozillapulse import config as pconf
from os import path
from dateutil import parser

site.addsitedir(path.join(path.dirname(__file__), "../../lib/python"))
from kickoff.api import Status
from release.info import getReleaseName

import logging as log

NAME_KEYS = ('product', 'version', 'build_number')
PROPERTIES_KEYS = ('platform', 'chunkTotal', 'chunkNum', 'event_group')


def get_properties_dict(props):
    """Convert properties tuple into dict"""
    props_dict = {}
    for prop in props:
        props_dict[prop[0]] = prop[1]
    return props_dict


def receive_message(config, data, message):
    buildername = data["payload"]["build"]["builderName"]
    properties = data["payload"]["build"]["properties"]
    results = data["payload"]["results"]

    if not buildername.startswith('release-') or \
            ' test ' in buildername:
        return
    if results != 0:
        log.debug("Non zero results from %s", buildername)
        return
    if not properties:
        log.exception("No properties set by %s", buildername)
        return

    log.info("Processing %s", buildername)
    properties_dict = get_properties_dict(properties)

    payload = {}
    for k, v in properties_dict.iteritems():
        if k in PROPERTIES_KEYS:
            payload[k] = v
    if "event_group" not in payload:
        log.debug("Ignoring message without event_group set")
        return
    # rename event_group to group
    payload["group"] = payload.pop("event_group")
    payload["results"] = results
    payload["event_name"] = buildername
    # Convert sent to UTC
    timestamp = parser.parse(data["_meta"]["sent"]).astimezone(pytz.utc)
    payload["sent"] = timestamp.strftime("%Y-%m-%d %H:%M:%S")

    name = getReleaseName(
        properties_dict["product"], properties_dict["version"],
        properties_dict["build_number"])

    log.info("Adding new release event for %s with event_name %s", name,
             buildername)
    status_api = Status(
        (config.get('api', 'username'), config.get('api', 'password')),
        api_root=config.get('api', 'api_root'))
    status_api.update(name, data=payload)


def main():
    parser = OptionParser()
    parser.add_option("-c", "--config", dest="config",
                      help="Configuration file")
    options = parser.parse_args()[0]
    config = ConfigParser()
    try:
        config.read(options.config)
    except:
        parser.error("Could not open configuration file")

    def got_message(data, message):
        try:
            receive_message(config, data, message)
        finally:
            message.ack()

    if not options.config:
        parser.error('Configuration file is required')

    if not all([config.has_section('pulse'),
                config.has_option('pulse', 'user'),
                config.has_option('pulse', 'password')]):
        log.critical('Config file must have a [pulse] section containing and '
                     'least "user" and "password" options.')
        exit(1)

    verbosity = {True: log.DEBUG, False: log.WARN}
    log.basicConfig(
        format='%(asctime)s %(message)s',
        level=verbosity[config.getboolean('shipit-notifier', 'verbose')]
    )

    pulse_cfg = pconf.PulseConfiguration.read_from_config(config)

    # Adjust applabel when wanting to run shipit on multiple machines
    pulse = consumers.BuildConsumer(applabel='shipit-notifier', connect=False)
    pulse.config = pulse_cfg
    pulse.configure(topic='build.#.finished',
                    durable=True, callback=got_message)

    log.info('listening for pulse messages')
    pulse.listen()

if __name__ == "__main__":
    try:
        main()
    except Exception:
        # Supervisor disables a service if it fails too often and too fast.
        import time
        time.sleep(120)
