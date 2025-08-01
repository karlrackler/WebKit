# Copyright (C) 2018-2025 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1.  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import json
import logging
import os
import requests
import socket

_log = logging.getLogger(__name__)


def fetch_data_from_url(url):
    _log.info('Fetching: {}'.format(url))
    response = None
    try:
        response = requests.get(url, timeout=10)
    except Exception as e:
        if response:
            _log.error('Failed to access {url} with status code {status_code}.'.format(url=url, status_code=response.status_code))
        else:
            _log.error('Failed to access {url} with exception: {exception}'.format(url=url, exception=e))
        return None
    if response.status_code != 200:
        _log.error('Accessed {url} with unexpected status code {status_code}.'.format(url=url, status_code=response.status_code))
        return None
    return response


def is_valid_id(id, expected_data_type=int):
    if not id:
        _log.warn('Invalid id: {}'.format(id))
        return False
    if type(id) != expected_data_type:
        _log.warn('Invalid type {} for id: {}, expected: {}'.format(type(id), id, expected_data_type))
        return False
    if expected_data_type == int and id < 0:
        _log.warn('Invalid id: {}, id should be positive integer.'.format(id))
        return False
    return True


def load_password(name, default=None):
    if os.getenv(name):
        return os.getenv(name)
    try:
        passwords = json.load(open('passwords.json'))
        return passwords.get(name, default)
    except FileNotFoundError as e:
        _log.error('ERROR: passwords.json missing: {}, using default value for {}\n'.format(e, name))
    except Exception as e:
        _log.error('Error in finding {} in passwords.json'.format(name))
    return default


def get_custom_suffix():
    hostname = socket.gethostname().strip()
    if 'dev' in hostname:
        return '-dev'
    if 'uat' in hostname:
        return '-uat'
    return ''


def get_cibuilds_for_queue(change, queue):
    return [build for build in change.cibuild_set.all() if build.builder_display_name == queue]
