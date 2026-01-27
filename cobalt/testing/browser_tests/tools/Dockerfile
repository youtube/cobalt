# Specify the Base Image
FROM ubuntu@sha256:c7eb020043d8fc2ae0793fb35a37bff1cf33f156d4d4b12ccc7f3ef8706c38b1

# Install base utilities, Android test dependencies (adb, netcat),
# and Linux test dependencies (GUI/X11 libraries).
RUN apt-get update && apt-get install -y \
    adb \
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
    usbutils \
    zstd \
    && apt-get clean && rm -rf /var/lib/apt/lists/*

# Set the working directory
WORKDIR /opt/cobalt_browsertests

# Add and extract the artifacts tarball.
# Expects test_artifacts.tar.zstd to be in the build context.
COPY test_artifacts.tar.zstd .
RUN tar --use-compress-program=unzstd -xf test_artifacts.tar.zstd && rm test_artifacts.tar.zstd

ENTRYPOINT ["./run_tests.py"]
