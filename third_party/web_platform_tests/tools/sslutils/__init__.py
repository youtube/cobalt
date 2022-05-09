import sys
import os
sys.path.append(os.path.dirname(__file__))

import openssl
import pregenerated
from base import NoSSLEnvironment
from openssl import OpenSSLEnvironment
from pregenerated import PregeneratedSSLEnvironment

environments = {"none": NoSSLEnvironment,
                "openssl": OpenSSLEnvironment,
                "pregenerated": PregeneratedSSLEnvironment}
