# WARNING: This Docker image is only rebuilt on postsubmits (pushes to branches).
# Modifying this Dockerfile, requirements.txt, or .pre-commit-config.yaml in a PR
# will change the checksum and cause the PR lint job to fail because the new image
# is not built during PR checks.
ARG BASE_IMAGE_SHA=sha256:be15f84c44c4ee68b4f187128f0278df1b0f424c04fb5f08b098967d5b896388


FROM marketplace.gcr.io/google/debian12@${BASE_IMAGE_SHA}

# Install system dependencies
RUN apt-get update && apt-get install -y --no-install-recommends \
    git \
    gn \
    python3 \
    python3-pip \
    && rm -rf /var/cache/apt/archives /var/lib/apt/lists/*

# Add safe.directory wildcard configuration to prevent ownership issues in GHA (system-wide)
RUN git config --system --add safe.directory '*'

# Install pre-commit using pip with pinned hashes
COPY requirements.txt /tmp/
RUN python3 -m pip install --break-system-packages --require-hashes -r /tmp/requirements.txt \
    && rm -f /tmp/requirements.txt

# Pre-install pre-commit hooks (requires a dummy git repo to run)
# NOTE: .pre-commit-config.yaml must be copied to this directory before building.
# (This is handled automatically in the CI workflow, but must be done manually if building locally).
ENV PRE_COMMIT_HOME=/var/cache/pre-commit
RUN mkdir -p $PRE_COMMIT_HOME && chmod 777 $PRE_COMMIT_HOME

WORKDIR /app
COPY .pre-commit-config.yaml /app/
RUN git init \
    && pre-commit install-hooks \
    && rm -rf .git .pre-commit-config.yaml /root/.cache \
    && find $PRE_COMMIT_HOME -type d -name ".go" -exec rm -rf {} + \
    && chmod -R 777 $PRE_COMMIT_HOME
