# NOTE: This Docker image is only rebuilt on postsubmits (pushes to branches).
# To edit, push to experimental/* or feature/* branch
#
# BEFORE BUILDING: Run `cp .pre-commit-config.yaml cobalt/docker/pre-commit/`
ARG BASE_IMAGE_SHA=sha256:be15f84c44c4ee68b4f187128f0278df1b0f424c04fb5f08b098967d5b896388

FROM marketplace.gcr.io/google/debian12@${BASE_IMAGE_SHA}

### Install dependencies.
RUN apt-get update && apt-get install -y --no-install-recommends \
    git \
    gn \
    python3 \
    python3-pip \
    && rm -rf /var/cache/apt/archives /var/lib/apt/lists/*
COPY requirements.txt /tmp/
RUN python3 -m pip install --no-cache-dir --break-system-packages --require-hashes -r /tmp/requirements.txt \
    && rm -f /tmp/requirements.txt

WORKDIR /app
COPY .pre-commit-config.yaml /app/

# Setup pre-commit environment, install hooks, and clean up in one go to save disk.
ENV PRE_COMMIT_HOME=/var/cache/pre-commit
RUN git config --system --add safe.directory '*' \
    && git init \
    && mkdir -p "$PRE_COMMIT_HOME" \
    && chmod 777 "$PRE_COMMIT_HOME" \
    && pre-commit install-hooks \
    && rm -rf .git .pre-commit-config.yaml /root/.cache \
    && find "$PRE_COMMIT_HOME" -type d -name ".go" -exec rm -rf {} + \
    && chmod -R 777 "$PRE_COMMIT_HOME"
