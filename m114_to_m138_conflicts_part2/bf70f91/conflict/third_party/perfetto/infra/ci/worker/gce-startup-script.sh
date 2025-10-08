#!/bin/bash
# Copyright (C) 2019 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

set -eux -o pipefail

<<<<<<< HEAD
# worker-img is set at GCE VM creation time in the Makefile.

ATTRS='http://metadata.google.internal/computeMetadata/v1/instance/attributes'
URL="$ATTRS/worker-img"
WORKER_IMG=$(curl --silent --fail -H'Metadata-Flavor:Google' $URL)

# We use for checkout + build + cache. The only things that are persisted
# (artifacts) are uploaded to GCS. We use swapfs so we can exceed physical ram
# when using tmpfs. It's still faster than operating on a real filesystem as in
# most cases we have enough ram to not need swap.
shopt -s nullglob
=======
# num-workers is set at VM creation time in the Makefile.
URL='http://metadata.google.internal/computeMetadata/v1/instance/attributes/num-workers'
NUM_WORKERS=$(curl --silent --fail -H'Metadata-Flavor:Google' $URL || echo 1)

>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
for SSD in /dev/nvme0n*; do
mkswap $SSD
swapon -p -1 $SSD
done

<<<<<<< HEAD
# Enlarge the /tmp size to use swap and also allow exec, as some tests need it.
mount -o remount,mode=777,size=95%,exec /tmp

# Disable Addressa space ranomization. This is needed for Tsan tests to work in
# the sandbox. Newer kernel versions seem to block the
# personality(ADDR_NO_RANDOMIZE) call within the sandbox and would require the
# sandbox to be --privileged, which defeats the point of having a sandbox.
sudo sysctl -w kernel.randomize_va_space=0

docker run -d \
  --name worker \
  --privileged \
  --net=host \
  --log-driver gcplogs \
  --rm \
  -v /tmp:/tmp \
  -v /var/run/docker.sock:/var/run/docker.sock \
  $WORKER_IMG
=======
# This is used by the sandbox containers, NOT needed by the workers.
# Rationale for size=100G: by default tmpfs mount are set to RAM/2, which makes
# the CI depend too much on the underlying VM. Here and below, we pick an
# arbitrary fixed size (we use local scratch NVME as a swap device).
export SHARED_WORKER_CACHE=/mnt/disks/shared_worker_cache
rm -rf $SHARED_WORKER_CACHE
mkdir -p $SHARED_WORKER_CACHE
mount -t tmpfs tmpfs $SHARED_WORKER_CACHE -o mode=777,size=100G

# This is used to queue build artifacts that are uploaded to GCS.
export ARTIFACTS_DIR=/mnt/disks/artifacts
rm -rf $ARTIFACTS_DIR
mkdir -p $ARTIFACTS_DIR
mount -t tmpfs tmpfs $ARTIFACTS_DIR -o mode=777,size=100G

# Pull the latest images from the registry.
docker pull eu.gcr.io/perfetto-ci/worker
docker pull eu.gcr.io/perfetto-ci/sandbox

# Create the restricted bridge for the sandbox container.
# Prevent access to the metadata server and impersonation of service accounts.
docker network rm sandbox 2>/dev/null || true  # Handles the reboot case.
docker network create sandbox -o com.docker.network.bridge.name=sandbox
sudo iptables -I DOCKER-USER -i sandbox -d 169.254.0.0/16 -j REJECT

# These args will be appended to the docker run invocation for the sandbox.
export SANDBOX_NETWORK_ARGS="--network sandbox --dns 8.8.8.8"

# The worker_main_loop.py script creates one docker sandbox container for
# each job invocation. It needs to talk back to the host docker to do so.
# This implies that the worker container is trusted and should never run code
# from the repo, as opposite to the sandbox container that is isolated.
for i in $(seq $NUM_WORKERS); do

# We manually mount a tmpfs mount ourselves because Docker doesn't allow to
# both override tmpfs-size AND "-o exec" (see also
# https://github.com/moby/moby/issues/32131)
SANDBOX_TMP=/mnt/disks/sandbox-$i-tmp
rm -rf $SANDBOX_TMP
mkdir -p $SANDBOX_TMP
mount -t tmpfs tmpfs $SANDBOX_TMP -o mode=777,size=100G

docker rm -f worker-$i 2>/dev/null || true
docker run -d \
  -v /var/run/docker.sock:/var/run/docker.sock \
  -v $ARTIFACTS_DIR:$ARTIFACTS_DIR \
  --env SHARED_WORKER_CACHE="$SHARED_WORKER_CACHE" \
  --env SANDBOX_NETWORK_ARGS="$SANDBOX_NETWORK_ARGS" \
  --env ARTIFACTS_DIR="$ARTIFACTS_DIR" \
  --env SANDBOX_TMP="$SANDBOX_TMP" \
  --env WORKER_HOST="$(hostname)" \
  --name worker-$i \
  --hostname worker-$i \
  --log-driver gcplogs \
  eu.gcr.io/perfetto-ci/worker
done


# Register a systemd service to stop worker containers gracefully on shutdown.
cat > /etc/systemd/system/graceful_shutdown.sh <<EOF
#!/bin/sh
logger 'Shutting down worker containers'
docker ps -q  -f 'name=worker-\d+$' | xargs docker stop -t 30
exit 0
EOF

chmod 755 /etc/systemd/system/graceful_shutdown.sh

# This service will cause the graceful_shutdown.sh to be invoked before stopping
# docker, hence before tearing down any other container.
cat > /etc/systemd/system/graceful_shutdown.service <<EOF
[Unit]
Description=Worker container lifecycle
Wants=gcr-online.target docker.service
After=gcr-online.target docker.service
Requires=docker.service

[Service]
Type=oneshot
RemainAfterExit=yes
ExecStop=/etc/systemd/system/graceful_shutdown.sh
EOF

systemctl daemon-reload
systemctl start graceful_shutdown.service
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
