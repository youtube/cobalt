# Stage 1: Extractor and Pruner
# We use a temporary stage to unpack and delete large, non-essential files.
FROM ubuntu@sha256:c7eb020043d8fc2ae0793fb35a37bff1cf33f156d4d4b12ccc7f3ef8706c38b1 AS builder

# Install unzip for platform-tools
RUN apt-get update && apt-get install -y unzip && rm -rf /var/lib/apt/lists/*

WORKDIR /opt/cobalt_browsertests

# Add and extract the artifacts tarball.
# Expects the artifacts tarball to be in the build context.
ADD cobalt/testing/browser_tests/tools/cobalt_browsertests_deps.tar* .

# Prune unstripped libraries and symbolication tools to keep the image light.
# We keep the stripped versions which are already at the root of the build dir.
# These paths are relative to /opt/cobalt_browsertests/src/
RUN find src/out/*/lib.unstripped -type f -delete || true
RUN rm -rf src/third_party/llvm-build \
           src/third_party/android_platform/development/scripts \
           src/tools/python

# Extract platform-tools if available
RUN if [ -f ./tools/platform-tools.zip ]; then \
    unzip ./tools/platform-tools.zip -d /opt && rm ./tools/platform-tools.zip; \
fi


# Stage 2: Final Light Runner Image
FROM ubuntu@sha256:c7eb020043d8fc2ae0793fb35a37bff1cf33f156d4d4b12ccc7f3ef8706c38b1

# Install runtime dependencies.
RUN apt-get update && apt-get install -y \
    unzip \
    netcat-openbsd \
    python3 \
    ca-certificates \
    xvfb \
    openbox \
    x11-xserver-utils \
    xserver-xorg-video-dummy \
    xserver-xorg-core \
    dbus-x11 \
    xcompmgr \
    libnss3 \
    libasound2 \
    libxcomposite1 \
    libxdamage1 \
    libxrandr2 \
    libgbm1 \
    socat \
    iproute2 \
    usbutils \
    && apt-get clean && rm -rf /var/lib/apt/lists/*

WORKDIR /opt/cobalt_browsertests

# Copy pruned artifacts from builder stage.
COPY --from=builder /opt/cobalt_browsertests /opt/cobalt_browsertests
COPY --from=builder /opt/platform-tools /opt/platform-tools

# Configure environment
ENV PATH="/opt/platform-tools:${PATH}"
ENV CHROME_HEADLESS=1
ENV LANG=en_US.UTF-8

ENTRYPOINT ["python3", "run_tests.py"]
