#!/usr/bin/env python
# ***** BEGIN LICENSE BLOCK *****
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.
# ***** END LICENSE BLOCK *****
'''Interact with mozpool/lifeguard/bmm.
'''
import getpass
import re
import requests
import socket
import sys
import time

try:
    import simplejson as json
except ImportError:
    import json

JsonHeader = {'content-type': 'application/json'}

# 200 OK
# 201 Created
# 202 Accepted
# 300 Multiple Choices
# 301 Moved Permanently
# 302 Found
# 304 Not Modified
# 400 Bad Request
# 401 Unauthorized
# 403 Forbidden
# 404 Not Found
# 405 Method Not Allowed
# 409 Conflict
# 500 Server Error
# 501 Not Implemented
# 503 Service Unavailable

class MozpoolException(Exception):
    pass

class MozpoolConflictException(MozpoolException):
    pass

def mozpool_status_ok(status):
    if status in range(200,400):
        return True
    else:
        return False

def check_mozpool_status(status):
    if not mozpool_status_ok(status):
        if status == 409:
            raise MozpoolConflictException()
        import pprint
        raise MozpoolException('mozpool status not ok, code %s' % pprint.pformat(status))

class MozpoolHandler:
    def __init__(self, mozpool_api_url, log_obj=None):
        self.mozpool_api_url = mozpool_api_url
        self.mozpool_timeout=20
        self.user = getpass.getuser()
        self.hostname = socket.gethostname()
        if log_obj:
            self.log_obj = log_obj
        else:
            import logging as log
            self.log_obj = log

    # Helper methods {{{2
    def url_get(self, url, decode_json=True, **kwargs):
        """Generic get output from a url method.

        This could be moved to a generic url handler object.
        """
        self.log_obj.debug("Request GET %s..." % url)
        if kwargs.get("timeout") is None:
            kwargs["timeout"] = self.mozpool_timeout
        num_retries = 10
        try_num = 0
        while try_num <= num_retries:
            try_num += 1
            try:
                r = requests.get(url, **kwargs)
                self.log_obj.debug("Status code: %s" % str(r.status_code))
                if decode_json:
                    j = self.decode_json(r.text)
                    if j is not None:
                        return (j, r.status_code)
                    else:
                        raise MozpoolException("Try %d: Can't decode json from %s!" % (try_num, url))
                else:
                    return (r.text, r.status_code)
            except requests.exceptions.RequestException, e:
                raise MozpoolException("Try %d: Can't get %s: %s!" % (try_num, url, str(e)))
            if try_num <= num_retries:
                sleep_time = 2 * try_num
                self.log_obj.info("Sleeping %d..." % sleep_time)
                time.sleep(sleep_time)

    def decode_json(self, contents):
        try:
            return json.loads(contents, encoding="ascii")
        except ValueError, e:
            raise MozpoolException("Can't decode json: %s!" % str(e))
        except TypeError, e:
            raise MozpoolException("Can't decode json: %s!" % str(e))
        else:
            raise MozpoolException("Can't decode json: Unknown error!" % str(e))

    def url_post(self, url, data, auth=None, params=None,
                 good_statuses=None, decode_json=True, **kwargs):
        """Generic post to a url method.

        This could be moved to a generic url handler object.
        """
        self.log_obj.debug("Request POST %s..." % url)
        if kwargs.get("timeout") is None:
            kwargs["timeout"] = self.mozpool_timeout
        num_retries = 10
        if good_statuses is None:
            good_statuses = [200, 201, 202, 204, 302]
        try_num = 0
        while try_num <= num_retries:
            try_num += 1
            try:
                r = requests.post(url, data=data, **kwargs)
                if r.status_code in good_statuses:
                    self.log_obj.debug("Status code: %s" % str(r.status_code))
                    if decode_json:
                        j = self.decode_json(r.text)
                        if j is not None:
                            return (j, r.status_code)
                        else:
                            raise MozpoolException("Try %d: Can't decode json from %s!" % (try_num, url))
                    else:
                        return (r.text, r.status_code)
                else:
                    self.log_obj.critical("Bad return status from %s: %d!" % (url, r.status_code))
                    return (None, r.status_code)
            except requests.exceptions.RequestException, e:
                self.log_obj.exception("Try %d: Can't get %s: %s!" % (try_num, url, str(e)))
            if try_num <= num_retries:
                sleep_time = 2 * try_num
                self.log_obj.info("Sleeping %d..." % sleep_time)
                time.sleep(sleep_time)

    def partial_url_get(self, partial_url, **kwargs):
        return self.url_get(self.mozpool_api_url + partial_url, **kwargs)

    def partial_url_post(self, partial_url, **kwargs):
        return self.url_post(self.mozpool_api_url + partial_url, **kwargs)

    # Device queries {{{2
    def query_device_status(self, device, **kwargs):
        """ returns a JSON response body whose "status" key contains
            a short string describing the last-known status of the device,
            and whose "log" key contains an array of recent log entries
            for the device.
        """
        response, status = self.partial_url_get("/api/device/%s/status/" % device, **kwargs)
        check_mozpool_status(status)
        return response

    def query_device_state(self, device, **kwargs):
        """ returns a JSON response body whose "state" key contains
            a short string describing the last-known status of the device.
        """
        response, status = self.partial_url_get("/api/device/%s/state/" % device, **kwargs)
        check_mozpool_status(status)
        return response

    def query_device_details(self, device, **kwargs):
        bmm = self.mozpool_api_url
        response, status = self.url_get("%s/api/device/list/?details=1" % bmm, **kwargs)
        check_mozpool_status(status)
        devices = response.get("devices")
        if isinstance(devices, list):
            matches = filter(lambda dd: dd['name'] == device, devices)
            if len(matches) != 1:
                self.log_obj.critical("Couldn't find %s in device list!" % device_id)
                return
            else:
                return matches[0]
        else:
            # We shouldn't get here if query_all_device_details() FATALs...
            raise MozpoolException("Invalid response from query_all_device_details()!")

    def request_device(self, device, image, duration=30*60, assignee=None,
            pxe_config=None, b2gbase=None, environment='any', **kwargs):
        """ requests the given device. {id} may be "any" to let MozPool choose an
            unassigned device. The body must be a JSON object with at least the keys
            "requester", "duration", and "image". The value for "requester" takes an
            email address, for human users, or a hostname, for machine users. "duration"
            must be a value, in seconds, of the duration of the request (which can be
            renewed; see below).

            "image" specifies low-level configuration that should be done on the device
            by mozpool. Some image types will require additional parameters. Currently
            the only supported value is "b2g", for which a "b2gbase" key must also be
            present. The value of "b2gbase" must be a URL to a b2g build directory
            containing boot, system, and userdata tarballs.

            If successful, returns 200 OK with a JSON object with the key "request".
            The value of "request" is an object detailing the request, with the keys
            "assigned_device" (which is blank if mozpool is still attempting to find
            a device, "assignee", "expires", "id", "requested_device",
            and "url". The "url" attribute contains a partial URL
            for the request object and should be used in request calls, as detailed
            below. If 'any' device was requested, always returns 200 OK, since it will
            retry a few times if no devices are free. If a specific device is requested
            but is already assigned, returns 409 Conflict; otherwise, returns 200 OK.

            If a 200 OK code is returned, the client should then poll for the request's
            state (using the value of request["url"] returned in the JSON object with
            "status/" appended. A normal request will move through the states "new",
            "find_device", "contact_lifeguard", "pending", and "ready", in that order.
            When, and *only* when, the device is in the "ready" state, it is safe to be
            used by the client. Other possible states are "expired", "closed",
            "device_not_found", and "device_busy"; the assigned device (if any) is
            returned to the pool when any of these states are entered.
        """
        if not assignee:
            assignee = "%s-%s" % (self.user, self.hostname)
        if image == 'b2g':
            assert b2gbase is not None, "b2gbase must be supplied when image=='b2gbase'"
        assert duration == int(duration)

        data = {'assignee': assignee, 'duration': duration, 'image': image, 'environment': environment}
        if pxe_config is not None:
            data['pxe_config'] = pxe_config
        data['b2gbase'] = b2gbase
        response, status = self.url_post(
                "%s/api/device/%s/request/" % (self.mozpool_api_url, device),
                data=json.dumps(data), headers=JsonHeader)
        check_mozpool_status(status)
        return response

    def renew_request(self, request_url, new_duration):
        """ requests that the request's lifetime be updated. The request body
            should be a JSON object with the key "duration", the value of which is the
            *new* remaining time, in seconds, of the request. Returns 204 No Content.
        """
        request_url = request_url + 'renew/'
        data = {'duration': new_duration}
        response, status = self.url_post(request_url, data=json.dumps(data), headers=JsonHeader, decode_json=False)
        check_mozpool_status(status)
        return response

    def close_request(self, request_url, **kwargs):
        """ returns the device to the pool and deletes the request. Returns
            204 No Content.
        """
        if request_url:
            request_url = request_url + 'return/'
            data = {}
            response, status = self.url_post(request_url, data=json.dumps(data), headers=JsonHeader, decode_json=False)
            check_mozpool_status(status)
            return response
        else:
            return "request_url doesn't exist. Already closed?"

    def device_power_cycle(self, device, duration, assignee=None, pxe_config=None):
        """ initiate a power-cycle of this device. The POST body is a JSON object,
            with optional keys `pxe_config` and `boot_config`. If `pxe_config` is
            specified, then the device is configured to boot with that PXE config;
            otherwise, the device boots from its internal storage. If `boot_config` is
            supplied (as a string), it is stored for later use by the device via
            `/api/device/{id}/config/`.
        """
        if assignee == None:
            assignee = "%s-%s" % (self.user, self.hostname)
        data = {'assignee': assignee, 'duration': duration}
        if pxe_config is not None:
            data['pxe_config'] = pxe_config
        response, status = self.url_post(
            "%s/api/device/%s/power-cycle/" % (self.mozpool_api_url, device),
            data=json.dumps(data), headers=JsonHeader
        )
        check_mozpool_status(status)
        return response

    def device_ping(self, device, **kwargs):
        """ ping this device. Returns a JSON object with a `success` key, and
            value true or false. The ping happens synchronously, and takes around a
            half-second.
        """
        response, status = self.url_get(
            "%s/api/device/%s/ping" % (self.mozpool_api_url, device)
        )
        check_mozpool_status(status)
        return response

    def query_device_log(self, device, **kwargs):
        """ get a list of recent log lines for this device. The return value has
            a 'log' key containing a list of objects representing log lines.
        """
        response, status = self.url_get(
                device, "/api/device/%s/log/" % device, **kwargs)
        check_mozpool_status(status)
        return response

    def query_request_status(self, request_url, **kwargs):
        """ returns a JSON response body with keys "log" and "state". Log objects
            contain "message", "source", and "timestamp" keys. "state" is the name of
            the current state, "ready" being the state in which it is safe to use the
            device.
        """
        request_url = request_url + 'status/'
        response, status = self.url_get(request_url, **kwargs)
        check_mozpool_status(status)
        return response

    def query_request_details(self, request_url, **kwargs):
        """ returns a JSON response body whose "request" key contains an object
            representing the given request with the keys id, device_id, assignee,
            expires, and status. The expires field is given as an ISO-formatted time.
        """
        request_url = request_url + 'details/'
        response, status = self.url_get(request_url, **kwargs)
        check_mozpool_status(status)
        return response
