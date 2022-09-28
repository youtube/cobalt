# Copyright 2022 The Cobalt Authors. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
"""gRPC Mobile Harness Gateway server."""

from __future__ import print_function
import grpc
import mobile_harness_gateway_pb2
import mobile_harness_gateway_pb2_grpc
import sys

_WORK_DIR = '/mobile_harness_gateway'
_MOBILE_HARNESS_GATEWAY_SERVICE_HOST = 'mobile-harness-gateway-service'
_MOBILE_HARNESS_GATEWAY_SERVICE_PORT = '50051'


class MobileHarnessGatewayClient():
  """Mobile Harness Gateway Client class."""

  def __init__(self):
    self.channel = grpc.insecure_channel(
      target='%s:%s' %
        (_MOBILE_HARNESS_GATEWAY_SERVICE_HOST,
         _MOBILE_HARNESS_GATEWAY_SERVICE_PORT),
      # These options need to match server settings.
      options=[
        ('grpc.keepalive_time_ms', 10000),
        ('grpc.keepalive_timeout_ms', 5000),
        ('grpc.keepalive_permit_without_calls', 1),
        ('grpc.http2.max_pings_without_data', 0),
        ('grpc.http2.min_time_between_pings_ms', 10000),
        ('grpc.http2.min_ping_interval_without_data_ms', 5000)
      ]
    )
    self.stub = mobile_harness_gateway_pb2_grpc.mobile_harness_gatewayStub(
        self.channel)

  def run_command(self, workdir, args):
    for response_line in self.stub.exec_command(
        mobile_harness_gateway_pb2.MobileHarnessCommand(
            workdir=workdir, args=args)):
      print(response_line.response, end='')


def main():
  """Main routine."""
  client = MobileHarnessGatewayClient()
  try:
    cmd = ' '.join(sys.argv[1:])
    client.run_command(workdir=_WORK_DIR, args=cmd)
  except grpc.RpcError as e:
    print(e)
    return e.code().value


if __name__ == '__main__':
  sys.exit(main())
