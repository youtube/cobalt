# Specify the Base Image
FROM ubuntu@sha256:c7eb020043d8fc2ae0793fb35a37bff1cf33f156d4d4b12ccc7f3ef8706c38b1

# Install base utilities, Android test dependencies (adb, netcat),
# and Linux test dependencies (GUI/X11 libraries).
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

# Set the working directory
WORKDIR /opt/cobalt_browsertests

# Add and extract the artifacts tarball.
# Expects cobalt_browsertests_artifacts.tar.gz to be in the build context.
ADD cobalt_browsertests_artifacts.tar.gz .

# Extract platform-tools if available
ENV PATH="/opt/platform-tools:${PATH}"
RUN ls
RUN if [ -f ./tools/platform-tools.zip ]; then \
    unzip ./tools/platform-tools.zip -d /opt && rm ./tools/platform-tools.zip; \
else \
    echo "No platform-tools found, skipping..."; \
fi

ENTRYPOINT ["python3", "run_tests.py"]
